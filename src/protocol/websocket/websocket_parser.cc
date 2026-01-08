// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include <cstring>

#include <endian.h>

#include "websocket_parser.h"

namespace jdocs {

size_t WebSocketParser::ParserExecute(void *buffer, size_t length) {
  uint64_t index;
  uint64_t i = 0;
  uint8_t *buf = static_cast<uint8_t *>(buffer);
  if (state_ == parser_state_t::kWsParserDone)
    return 0;
  for (; i < length; ++i) {
    switch (state_) {
    case parser_state_t::kWsParserFinAndOpcode:
      fin_flag_ = (buf[i] & 0x80) != 0;
      rsv1_flag_ = (buf[i] & 0x40) != 0;
      rsv2_flag_ = (buf[i] & 0x20) != 0;
      rsv3_flag_ = (buf[i] & 0x10) != 0;
      opcode_ = (buf[i] & 0x0F);
      state_ = parser_state_t::kWsParserMaskAndLen;
      break;
    case parser_state_t::kWsParserMaskAndLen:
      mask_flag_ = (buf[i] & 0x80) != 0;
      length_ = (buf[i] & 0x7F);
      if (!mask_flag_) {
        error_code_ = WS_CLOSE_PROTOCOL_ERROR;
        return 0;
      }
      if (length_ < 126) {
        payload_length_ = length_;
        if (payload_length_ > kMaxPayloadLength) {
          error_code_ = WS_CLOSE_PAYLOAD_TOO_BIG;
          return 0;
        }
        state_ = parser_state_t::kWsParserMaskingKey;
        remain_bytes_ = 4;
      } else if (length_ == 126) {
        state_ = parser_state_t::kWsParserPayloadLen16;
        remain_bytes_ = 2;
      } else {
        state_ = parser_state_t::kWsParserPayloadLen64;
        remain_bytes_ = 8;
      }
      break;
    case parser_state_t::kWsParserPayloadLen16:
      ((uint8_t *)&payload_length_)[2 - remain_bytes_] = buf[i];
      if (--remain_bytes_ == 0) {
        payload_length_ = be16toh(payload_length_ & 0xFFFF);
        if (payload_length_ > kMaxPayloadLength) {
          error_code_ = WS_CLOSE_PAYLOAD_TOO_BIG;
          return 0;
        }
        state_ = parser_state_t::kWsParserMaskingKey;
        remain_bytes_ = 4;
      }
      break;
    case parser_state_t::kWsParserPayloadLen64:
      ((uint8_t *)&payload_length_)[8 - remain_bytes_] = buf[i];
      if (--remain_bytes_ == 0) {
        payload_length_ = be64toh(payload_length_);
        if (payload_length_ > kMaxPayloadLength) {
          error_code_ = WS_CLOSE_PAYLOAD_TOO_BIG;
          return 0;
        }
        state_ = parser_state_t::kWsParserMaskingKey;
        remain_bytes_ = 4;
      }
      break;
    case parser_state_t::kWsParserMaskingKey:
      mask_[4 - remain_bytes_] = buf[i];
      if (--remain_bytes_ == 0) {
        if (payload_length_ != 0) {
          state_ = parser_state_t::kWsParserPayloadData;
        } else {
          state_ = parser_state_t::kWsParserDone;
        }
        remain_bytes_ = payload_length_;
      }
      break;
    case parser_state_t::kWsParserPayloadData:
      if (data_.empty())
        data_.reserve(payload_length_);
      index = payload_length_ - remain_bytes_;
      data_.push_back(buf[i] ^ mask_[index % 4]);
      if (--remain_bytes_ == 0) {
        state_ = parser_state_t::kWsParserDone;
      }
      break;
    case parser_state_t::kWsParserDone:
      return i;
    }
  }
  return i;
}

void WebSocketParser::Reset() {
  state_ = parser_state_t::kWsParserFinAndOpcode;
  remain_bytes_ = 1;
  data_.clear();
}

const char *WebSocketParser::GetError() const {
  return close_message(error_code_);
}

const char *WebSocketParser::close_message(uint16_t error_code) {
  switch (error_code) {
#define X(name, code, reason)                                                  \
  case code:                                                                   \
    return reason;
    WS_CLOSE_STATUS_MAP(X)
#undef X
  default:
    return "Unknown Reason";
  }
}

size_t WebSocketParser::generate_close_frame(uint16_t code, void *buffer) {
  uint8_t *buf = static_cast<uint8_t *>(buffer);
  const char *close_msg = close_message(code);
  size_t len = strlen(close_msg);
  buf[0] = 0x88;
  buf[1] = (len + 2) & 0x7F;
  *(uint16_t *)&buf[2] = htobe16(code);
  memcpy(buf + 4, close_msg, len);
  return len + 4;
}
} // namespace jdocs
