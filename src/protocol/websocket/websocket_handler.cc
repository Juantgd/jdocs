// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "websocket_handler.h"
#include "net/tcp_connection.h"
#include "protocol/websocket/websocket_parser.h"

#include <string.h>

namespace jdocs {

WebSocketHandler::WebSocketHandler() { parser.Reset(); }

void WebSocketHandler::RecvDataHandle(TcpConnection *connection, void *buffer,
                                      size_t length) {
next_loop:
  spdlog::info("websocket: parsing ...");
  if (connection->closed())
    return;
  size_t parsed_bytes = parser.ParserExecute(buffer, length);
  if (parser.IsDone()) {
    // 控制帧处理
    if (parser.opcode_ & 0x08) {
      control_frame_handle(connection);
    } else {
      // 非控制帧处理
      frame_handle(connection);
      // 此处进行业务处理，所有数据已解析完毕
      if (handle_state_ == ws_handle_state_t::kWsHandleStateNormal) {
        // service_handle(payload_cache);
        // websocket echo server test.
        spdlog::info("service handle working...");
        send_data_frame(connection, payload_cache.data(), payload_cache.size());
        payload_cache.clear();
      }
    }
    parser.Reset();
    // 处理粘包情况
    if (parsed_bytes < length) {
      length -= parsed_bytes;
      buffer = (char *)buffer + parsed_bytes;
      goto next_loop;
    }
  } else {
    // 解析出现错误
    if (parser.GetErrorCode()) {
      send_close_frame(connection,
                       static_cast<uint16_t>(parser.GetErrorCode()));
    }
    // 需要更多数据进行解析
  }
}

// 封装websocket数据帧，传入的载荷数据不要超过缓冲区大小，否则会发生溢出
size_t WebSocketHandler::encapsulation_package(
    bool fin_flag, WebSocketParser::ws_opcode_t opcode, void *buffer,
    void *payload, size_t length) {
  size_t frame_size = 0;
  size_t payload_offset = 2;
  uint8_t *buf = static_cast<uint8_t *>(buffer);
  if (length < 126) {
    frame_size = payload_offset + length;
    buf[1] = length & 0x7F;
  } else if (length >= 126 && length < 0xFFFF) {
    payload_offset = 4;
    frame_size = payload_offset + length;
    buf[1] = 126;
    *((uint16_t *)&buf[2]) = htobe16(length & 0xFFFF);
  } else {
    payload_offset = 10;
    frame_size = payload_offset + length;
    buf[1] = 127;
    *((uint64_t *)&buf[2]) = htobe64(length);
  }
  buf[0] = static_cast<uint8_t>((fin_flag << 7) | opcode);
  memcpy(buf + payload_offset, payload, length);
  return frame_size;
}

size_t WebSocketHandler::get_affordable_payload_size(size_t payload_length,
                                                     size_t buffer_size) {
  if (buffer_size <= 2)
    return 0;
  if (payload_length < 126)
    buffer_size -= 2;
  else if (payload_length >= 126 && payload_length < 0xFFFF)
    buffer_size = buffer_size > 4 ? buffer_size - 4 : 0;
  else
    buffer_size = buffer_size > 10 ? buffer_size - 10 : 0;
  return buffer_size > payload_length ? payload_length : buffer_size;
}

void WebSocketHandler::frame_handle(TcpConnection *connection) {
  if (handle_state_ == ws_handle_state_t::kWsHandleStateClosing)
    return;
  switch (parser.opcode_) {
  case WebSocketParser::WS_OPCODE_CONTINUED: {
    if (handle_state_ == ws_handle_state_t::kWsHandleStateContinued) {
      if (parser.fin_flag_)
        handle_state_ = ws_handle_state_t::kWsHandleStateNormal;
      if (payload_cache.size() + parser.data_.size() > kMaxPayloadLength) {
        send_close_frame(connection, WebSocketParser::WS_CLOSE_PAYLOAD_TOO_BIG);
      } else {
        payload_cache.append(parser.data_);
      }
    } else {
      send_close_frame(connection, WebSocketParser::WS_CLOSE_PROTOCOL_ERROR);
    }
    break;
  }
  case WebSocketParser::WS_OPCODE_TEXT:
  case WebSocketParser::WS_OPCODE_BINARY: {
    if (!parser.fin_flag_) {
      handle_state_ = ws_handle_state_t::kWsHandleStateContinued;
    }
    payload_cache = std::move(parser.data_);
    break;
  }
  default: {
    // 收到暂不支持的操作码
    send_close_frame(connection, WebSocketParser::WS_CLOSE_UNSUPPORTED_DATA);
  }
  }
}

void WebSocketHandler::control_frame_handle(TcpConnection *connection) {
  switch (parser.opcode_) {
  case WebSocketParser::WS_OPCODE_CLOSE: {
    // 客户端发起了close请求
    if (handle_state_ != ws_handle_state_t::kWsHandleStateClosing) {
      send_close_frame(connection, WebSocketParser::WS_CLOSE_NORMAL, true);
    }
    connection->close();
    break;
  }
  case WebSocketParser::WS_OPCODE_PING: {
    spdlog::info("websocket: got a PING frame.");
    send_pong_frame(connection, parser.data_.data(), parser.data_.size());
    break;
  }
  case WebSocketParser::WS_OPCODE_PONG: {
    // TODO: 如果先前发送了ping包，则处理，比如取消超时关闭连接事件
    if (wait_pong_flag) {
      wait_pong_flag = false;
    }
    break;
  }
  default: {
    // 收到暂不支持的操作码
    send_close_frame(connection, WebSocketParser::WS_CLOSE_UNSUPPORTED_DATA);
  }
  }
}

void WebSocketHandler::send_close_frame(TcpConnection *connection,
                                        uint16_t code, bool flag) {
  void *send_buf;
  int bidx = connection->GetEventLoop()->__get_send_buffer(&send_buf);
  if (bidx == -1) {
    // 没有可用的发送缓冲区，说明服务器目前负载压力过大，直接关闭该连接
    connection->close();
    return;
  }
  spdlog::info("websocket: send close frame. message: {}",
               WebSocketParser::close_message(code));
  size_t prep_send_bytes =
      WebSocketParser::generate_close_frame(code, send_buf);
  connection->GetEventLoop()->__prep_send_zc(
      connection->fd(), connection->conn_id(), send_buf,
      static_cast<uint16_t>(bidx), prep_send_bytes, flag);
  handle_state_ = ws_handle_state_t::kWsHandleStateClosing;
}

void WebSocketHandler::send_pong_frame(TcpConnection *connection, void *payload,
                                       size_t length) {
  void *send_buf;
  int bidx = connection->GetEventLoop()->__get_send_buffer(&send_buf);
  if (bidx == -1) {
    connection->close();
    return;
  }
  size_t prep_send_bytes = encapsulation_package(
      true, WebSocketParser::WS_OPCODE_PONG, send_buf, payload, length);
  connection->GetEventLoop()->__prep_send_zc(connection->fd(),
                                             connection->conn_id(), send_buf,
                                             (uint16_t)bidx, prep_send_bytes);
}

void WebSocketHandler::send_data_frame(TcpConnection *connection, void *data,
                                       size_t length) {
  void *send_buf;
  int bidx;
  bool fin_flag = true, once_flag = true;
  size_t payload_length, prep_send_bytes;
  WebSocketParser::ws_opcode_t opcode;
  do {
    bidx = connection->GetEventLoop()->__get_send_buffer(&send_buf);
    if (bidx == -1) {
      spdlog::error("send_data_frame failed. not avaliable buffer to send.");
      connection->close();
      return;
    }
    payload_length = get_affordable_payload_size(length, kBufferSize);
    fin_flag = true;
    opcode = WebSocketParser::WS_OPCODE_CONTINUED;
    // 不能一次发送完成
    if (payload_length < length) {
      fin_flag = false;
    }
    if (once_flag) {
      opcode = WebSocketParser::WS_OPCODE_TEXT;
      once_flag = false;
    }
    prep_send_bytes =
        encapsulation_package(fin_flag, opcode, send_buf, data, payload_length);
    connection->GetEventLoop()->__prep_send_zc(
        connection->fd(), connection->conn_id(), send_buf, (uint16_t)bidx,
        prep_send_bytes, !fin_flag);
    data = (char *)data + payload_length;
    length -= payload_length;
  } while (length);
}

} // namespace jdocs
