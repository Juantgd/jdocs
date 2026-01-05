// Copyright (c) 2025 Juantgd. All Rights Reserved.

#ifndef JDOCS_PROTOCOL_WEBSOCKET_PARSER_H_
#define JDOCS_PROTOCOL_WEBSOCKET_PARSER_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace jdocs {

namespace {

#define WS_OPCODE_MAP(X)                                                       \
  X(0x0, CONTINUED)                                                            \
  X(0x1, TEXT)                                                                 \
  X(0x2, BINARY)                                                               \
  X(0x8, CLOSE)                                                                \
  X(0x9, PING)                                                                 \
  X(0xA, PONG)

#define WS_CLOSE_STATUS_MAP(X)                                                 \
  X(NORMAL, 1000, "connection successfully closed")                            \
  X(GOING_AWAY, 1001, "endpoint is going away")                                \
  X(PROTOCOL_ERROR, 1002, "encounter protocol error")                          \
  X(UNSUPPORTED_DATA, 1003, "received a type of data cannot accept")           \
  X(INVALID_PAYLOAD, 1007,                                                     \
    "received data within a message that was not consistent with the "         \
    "type of the message")                                                     \
  X(POLICY_VIOLATION, 1008, "received a message that violates its policy")     \
  X(PAYLOAD_TOO_BIG, 1009,                                                     \
    "received a message that is too big for it to process")                    \
  X(EXTENSION_REQUIRED, 1010,                                                  \
    "expected the one or more extension but not response")                     \
  X(INTERNAL_ERROR, 1011, "encountered an unexpected condition")

} // namespace

// websocket协议最大载荷长度限制，64KB大小
constexpr const uint32_t kMaxPayloadLength = 1 << 16;

struct WebSocketParser {
  WebSocketParser() = default;
  ~WebSocketParser() = default;

  size_t ParserExecute(void *buffer, size_t length);
  inline bool IsDone() const { return state_ == parser_state_t::kWsParserDone; }
  void Reset();
  inline int GetErrorCode() const { return error_code_; }
  const char *GetError() const;

  static const char *close_message(uint16_t error_code);

  static size_t generate_close_frame(uint16_t code, void *buffer);

  enum ws_opcode_t : uint8_t {
#define X(code, name) WS_OPCODE_##name = code,
    WS_OPCODE_MAP(X)
#undef X
  };

  enum class parser_state_t {
    kWsParserFinAndOpcode = 0,
    kWsParserMaskAndLen,
    kWsParserPayloadLen16,
    kWsParserPayloadLen64,
    kWsParserMaskingKey,
    kWsParserPayloadData,
    kWsParserDone
  };

  enum ws_close_code_t : uint16_t {
#define X(name, code, reason) WS_CLOSE_##name = code,
    WS_CLOSE_STATUS_MAP(X)
#undef X
  };

  parser_state_t state_;
  uint8_t fin_flag_ : 1;
  uint8_t rsv1_flag_ : 1;
  uint8_t rsv2_flag_ : 1;
  uint8_t rsv3_flag_ : 1;
  uint8_t opcode_ : 4;
  uint8_t mask_flag_ : 1;
  uint8_t length_ : 7;
  uint16_t error_code_{0};
  uint8_t mask_[4];
  uint64_t payload_length_;
  uint64_t remain_bytes_{1};
  // 存放解析后的载荷数据
  std::string data_;
};

} // namespace jdocs

#endif
