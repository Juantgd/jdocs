// Copyright (c) 2025 Juantgd. All Rights Reserved.

#include "tcp_connection.h"

#include <string.h>

#include "protocol/http/http_parser.h"
#include "protocol/websocket/websocket_parser.h"

namespace jdocs {

void TcpConnection::SendHandle(void *buffer, size_t length) {
  // 检查所有数据是否已经发送完毕，否则继续提交发送请求
  send_bytes_ += length;
}

// 读操作完成处理函数，传入已读取的缓冲区地址和读取的字节数
void TcpConnection::RecvHandle(void *buffer, size_t length) {
  recv_bytes_ += length;
  // 根据当前连接状态开始进行协议解析
  switch (stage_) {
  case stage_t::kStageHttp: {
    __http_stage_handle(buffer, length);
    break;
  }
  case stage_t::kStageWebsocket: {
    __websocket_stage_handle(buffer, length);
    break;
  }
  }
}

// 发送消息函数，用于跨线程信息传递
void TcpConnection::SendMessage(void *data, size_t length) {}

void TcpConnection::__http_stage_handle(void *buffer, size_t length) {
  HttpParser *parser = static_cast<HttpParser *>(parser_);
  parser->ParserExecute(buffer, length);
  if (parser_->IsDone()) {
    void *send_buf;
    int bidx = event_loop_->__get_send_buffer(&send_buf);
    if (bidx != -1) {
      size_t prep_send_bytes = HttpParser::generate_response_101(
          send_buf, parser->GetWebSocketKey());
      event_loop_->__prep_send_zc(fd_, conn_id_, send_buf,
                                  static_cast<uint16_t>(bidx), prep_send_bytes);
      // 处理完毕后根据需要是否重置解析器状态或是进入下一阶段
      __transition_stage(stage_t::kStageWebsocket);
    } else {
      // 没有可用的发送缓冲区，说明目前服务器负载有点大，所以直接关闭该连接
      spdlog::error("not avaliable buffer to send. closing client.");
      close();
    }
  } else {
    // 解析失败可能是因为需要更多数据进行解析，或是遇到了解析错误
    if (parser->GetErrorCode() != 0) {
      spdlog::error("HTTP Protocol parser failed. error: {}",
                    parser->GetError());
      void *send_buf;
      int bidx = event_loop_->__get_send_buffer(&send_buf);
      if (bidx != -1) {
        size_t prep_send_bytes = HttpParser::generate_response_400(send_buf);
        event_loop_->__prep_send_zc(fd_, conn_id_, send_buf,
                                    static_cast<uint16_t>(bidx),
                                    prep_send_bytes, true);
      } else {
        spdlog::error("not avaliable buffer to send. closing client.");
      }
      close();
    }
    // 需要更多数据进行解析，什么都不做
  }
}

void TcpConnection::__websocket_stage_handle(void *buffer, size_t length) {
  WebSocketParser *parser = static_cast<WebSocketParser *>(parser_);
  size_t parsed_bytes = parser->ParserExecute(buffer, length);
  if (parser->IsDone()) {
    switch (substage_) {
    case substage_t::kSubStageWebSocketEstablished: {
      void *send_buf;
      int bidx = event_loop_->__get_send_buffer(&send_buf);
      if (bidx == -1) {
        spdlog::error("not avaliable buffer to send. closing client.");
        close();
        return;
      }
      // websocket echo test
      if (parser->GetOpcode() == WebSocketParser::WS_OPCODE_CLOSE) {
        char close_buf[125]{};
        *(uint16_t *)close_buf = htobe16(1000);
        memcpy(close_buf + 2, "Successfully Closing!", 21);
        size_t prep_send_bytes = WebSocketParser::encapsulation_package(
            true, WebSocketParser::WS_OPCODE_CLOSE, send_buf, close_buf,
            strlen(close_buf));
        event_loop_->__prep_send_zc(fd_, conn_id_, send_buf,
                                    static_cast<uint16_t>(bidx),
                                    prep_send_bytes);
        __transition_substage(substage_t::kSubStageWebSocketClosing);
      } else {
        size_t payload_length;
        void *payload_buf = parser->GetPayloadData(&payload_length);
        size_t prep_send_bytes = WebSocketParser::encapsulation_package(
            true, WebSocketParser::WS_OPCODE_TEXT, send_buf, payload_buf,
            payload_length);
        event_loop_->__prep_send_zc(fd_, conn_id_, send_buf,
                                    static_cast<uint16_t>(bidx),
                                    prep_send_bytes);
        free(payload_buf);
        // 处理粘包情况
        if (parsed_bytes != length) {
          spdlog::info("出现粘包情况");
        }
      }
      parser->Reset();
      break;
    }
    case substage_t::kSubStageWebSocketClosing: {
      if (parser->GetOpcode() == WebSocketParser::WS_OPCODE_CLOSE) {
        close();
      }
      break;
    }
    case substage_t::kSubStageWebSocketClosed: {
      break;
    }
    }

  } else {
    // 解析出现错误，进行错误处理
    if (parser->GetErrorCode()) {
      spdlog::error("WebSocket Protocol parser failed. error: {}",
                    parser->GetError());
      void *send_buf;
      int bidx = event_loop_->__get_send_buffer(&send_buf);
      if (bidx != -1) {
        char error_buf[125]{};
        *(uint16_t *)error_buf =
            htobe16(static_cast<uint16_t>(parser->GetErrorCode()));
        memcpy(error_buf + 2, parser->GetError(), strlen(parser->GetError()));
        size_t prep_send_bytes = WebSocketParser::encapsulation_package(
            true, WebSocketParser::WS_OPCODE_CLOSE, send_buf, error_buf,
            strlen(error_buf));
        event_loop_->__prep_send_zc(fd_, conn_id_, send_buf,
                                    static_cast<uint16_t>(bidx),
                                    prep_send_bytes);
        __transition_substage(substage_t::kSubStageWebSocketClosing);
      } else {
        spdlog::error("not avaliable buffer to send. closing client.");
        close();
        return;
      }
    }
    // 需要更多数据进行解析
  }
}

void TcpConnection::__transition_stage(stage_t stage) {
  if (parser_)
    delete parser_;
  switch (stage) {
  case stage_t::kStageHttp: {
    parser_ = new HttpParser();
    stage_ = stage_t::kStageHttp;
    break;
  }
  case stage_t::kStageWebsocket: {
    parser_ = new WebSocketParser();
    stage_ = stage_t::kStageWebsocket;
    break;
  }
  }
  parser_->Reset();
}

void TcpConnection::__transition_substage(substage_t substage) {
  substage_ = substage;
}

TcpConnection::~TcpConnection() {
  if (parser_)
    delete parser_;
}

} // namespace jdocs
