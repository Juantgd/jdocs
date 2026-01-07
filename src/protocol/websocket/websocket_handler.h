// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_PROTOCOL_WEBSOCKET_HANDLER_H_
#define JDOCS_PROTOCOL_WEBSOCKET_HANDLER_H_

#include "net/tcp_connection.h"
#include "protocol/websocket/websocket_parser.h"

namespace jdocs {

namespace {

#define WS_PING_PAYLOAD "Are you ok?"

constexpr size_t raw_ping_frame_size = 13;
// 直接发送，不需要拷贝
constexpr uint8_t raw_ping_frame[] = {0x89, 0x0B, 0x41, 0x72, 0x65, 0x20, 0x79,
                                      0x6F, 0x75, 0x20, 0x6F, 0x6B, 0x3F};

// 发送ping帧后等待pong帧的超时时间，默认30秒
constexpr static uint32_t kTimeToCloseAfterPing = 30000;

} // namespace

class WebSocketHandler : public ProtocolHandler {
public:
  WebSocketHandler(TcpConnection *connection);
  ~WebSocketHandler() = default;

  void RecvDataHandle(void *buffer, size_t length) override;

  void TimeoutHandle() override;

  static void send_data_frame(TcpConnection *connection, void *data,
                              size_t length);

private:
  // websocket协议处理状态机
  enum class ws_handle_state_t : uint8_t {
    kWsHandleStateNormal = 0,
    kWsHandleStateContinued,
    kWsHandleStateClosing,
    kWsHandleStateClosed
  };

  void frame_handle();

  void control_frame_handle();

  // 获取缓冲区实际能够存放的数据载荷大小
  static size_t get_affordable_payload_size(size_t payload_length,
                                            size_t buffer_size);

  // 封装websocket数据帧
  static size_t encapsulation_package(bool fin_flag,
                                      WebSocketParser::ws_opcode_t opcode,
                                      void *buffer, void *payload,
                                      size_t length);

  void send_ping_frame();

  void send_close_frame(uint16_t code, bool flag = false);

  void send_pong_frame(void *payload, size_t length);

  // 发送ping帧之后，等待pong帧的计时器
  TimeWheel::timer_node wait_pong_timer_{[this]() {
    if (this->wait_pong_flag)
      this->connection_->close();
  }};
  bool wait_pong_flag{false};

  ws_handle_state_t handle_state_{0};

  std::string payload_cache;

  WebSocketParser parser;
};

} // namespace jdocs

#endif
