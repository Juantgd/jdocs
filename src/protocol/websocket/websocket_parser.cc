// Copyright (c) 2025 Juantgd. All Rights Reserved.

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
        payload_length_ = be16toh(payload_length_);
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
      if (data_ == NULL)
        data_ = malloc(payload_length_);
      index = payload_length_ - remain_bytes_;
      ((uint8_t *)(data_))[index] = buf[i] ^ mask_[index % 4];
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
  data_ = NULL;
}

const char *WebSocketParser::GetError() const {
  switch (error_code_) {
#define X(name, code, reason)                                                  \
  case code:                                                                   \
    return reason;
    WS_CLOSE_STATUS_MAP(X)
#undef X
  default:
    return "Unknown Reason";
  }
}

// 封装websocket数据帧，传入的载荷数据不要超过缓冲区大小，否则会发生溢出
size_t WebSocketParser::encapsulation_package(bool fin_flag, ws_opcode_t opcode,
                                              void *buffer, void *payload,
                                              size_t length) {
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

size_t WebSocketParser::get_affordable_payload_size(size_t payload_length,
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

} // namespace jdocs
