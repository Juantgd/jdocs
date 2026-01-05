// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_PROTOCOL_WEBSOCKET_HANDLER_H_
#define JDOCS_PROTOCOL_WEBSOCKET_HANDLER_H_

#include "net/tcp_connection.h"
#include "protocol/websocket/websocket_parser.h"

namespace jdocs {

class WebSocketHandler : public ProtocolHandler {
public:
  WebSocketHandler();
  ~WebSocketHandler() = default;

  void RecvDataHandle(TcpConnection *connection, void *buffer,
                      size_t length) override;

private:
  // websocket协议处理状态机
  enum class ws_handle_state_t : uint8_t {
    kWsHandleStateNormal = 0,
    kWsHandleStateContinued,
    kWsHandleStateClosing,
    kWsHandleStateClosed
  };

  void frame_handle(TcpConnection *connection);

  void control_frame_handle(TcpConnection *connection);

  void send_close_frame(TcpConnection *connection, uint16_t code,
                        bool flag = false);

  // TODO: 有待实现
  // void send_ping_frame();

  void send_pong_frame(TcpConnection *connection, void *payload, size_t length);

  void send_data_frame(TcpConnection *connection, void *data, size_t length);

  // 获取缓冲区实际能够存放的数据载荷大小
  static size_t get_affordable_payload_size(size_t payload_length,
                                            size_t buffer_size);

  // 封装websocket数据帧
  static size_t encapsulation_package(bool fin_flag,
                                      WebSocketParser::ws_opcode_t opcode,
                                      void *buffer, void *payload,
                                      size_t length);

  bool wait_pong_flag;
  ws_handle_state_t handle_state_{0};

  std::string payload_cache;

  WebSocketParser parser;
};

} // namespace jdocs

#endif
