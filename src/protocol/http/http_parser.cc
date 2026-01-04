// Copyright (c) 2025 Juantgd. All Rights Reserved.

#include "http_parser.h"

#include <cstring>

namespace jdocs {

size_t HttpParser::ParserExecute(void *buffer, size_t length) {
  size_t i = 0;
  char ch, lc;
  if (state_ == parser_state_t::kHttpParserDone)
    return 0;
  for (; i != length; ++i) {
    ch = static_cast<char *>(buffer)[i];
  reexecute:
    switch (state_) {
    case parser_state_t::kHttpParserStart: {
      if (ch != 'G') {
        error_code_ = error_code_t::PARSER_ERROR_INVALID_METHOD;
        return 0;
      }
      state_ = parser_state_t::kHttpParserMethod;
      break;
    }
    case parser_state_t::kHttpParserMethod: {
      if (ch != kHttpMethod[++index_]) {
        error_code_ = error_code_t::PARSER_ERROR_INVALID_METHOD;
        return 0;
      } else if (index_ == sizeof(kHttpMethod) - 2) {
        state_ = parser_state_t::kHttpParserBeforeUri;
        index_ = 0;
      }
      break;
    }
    case parser_state_t::kHttpParserBeforeUri: {
      if (ch == ' ') {
        if (++index_ > 1) {
          error_code_ = error_code_t::PARSER_ERROR_PROTOCOL_ERROR;
          return 0;
        }
        break;
      }
      if (ch != '/') {
        error_code_ = error_code_t::PARSER_ERROR_PROTOCOL_ERROR;
        return 0;
      }
      if (index_ != 1) {
        error_code_ = error_code_t::PARSER_ERROR_PROTOCOL_ERROR;
        return 0;
      }
      state_ = parser_state_t::kHttpParserUri;
      index_ = 0;
      goto reexecute;
      break;
    }
    case parser_state_t::kHttpParserUri: {
      if (ch == ' ') {
        state_ = parser_state_t::kHttpParserAfterUri;
        index_ = 0;
        goto reexecute;
      }
      if (index_ >= kHttpHeaderElementSize - 1) {
        error_code_ = error_code_t::PARSER_ERROR_FIELD_OUT_OF_RANGE;
        return 0;
      }
      request_uri_[index_++] = ch;
      ++request_uri_length_;
      break;
    }
    case parser_state_t::kHttpParserAfterUri: {
      if (ch == ' ') {
        if (++index_ > 1) {
          error_code_ = error_code_t::PARSER_ERROR_PROTOCOL_ERROR;
          return 0;
        }
        break;
      }
      if (ch != 'H') {
        error_code_ = error_code_t::PARSER_ERROR_PROTOCOL_ERROR;
      }
      if (index_ != 1) {
        error_code_ = error_code_t::PARSER_ERROR_PROTOCOL_ERROR;
        return 0;
      }
      state_ = parser_state_t::kHttpParserVersion;
      index_ = 0;
      break;
    }
    case parser_state_t::kHttpParserVersion: {
      if (ch != kHttpVersion[++index_]) {
        error_code_ = error_code_t::PARSER_ERROR_PROTOCOL_ERROR;
        return 0;
      } else if (index_ == sizeof(kHttpVersion) - 2) {
        state_ = parser_state_t::kHttpParserAfterVersion;
        index_ = 0;
      }
      break;
    }
    case parser_state_t::kHttpParserAfterVersion: {
      if (ch != '\r') {
        error_code_ = error_code_t::PARSER_ERROR_PROTOCOL_ERROR;
        return 0;
      }
      state_ = parser_state_t::kHttpParserHeaderLineAlmostDone;
      break;
    }
    case parser_state_t::kHttpParserHeaderLineAlmostDone: {
      if (ch != '\n') {
        error_code_ = error_code_t::PARSER_ERROR_PROTOCOL_ERROR;
        return 0;
      }
      count_ = 0;
      state_ = parser_state_t::kHttpParserHeaderLineDone;
      break;
    }
    case parser_state_t::kHttpParserHeaderLineDone: {
      if (ch == ' ') {
        error_code_ = error_code_t::PARSER_ERROR_PROTOCOL_ERROR;
        return 0;
      }
      if (ch == '\r')
        state_ = parser_state_t::kHttpParserFinished;
      else {
        state_ = parser_state_t::kHttpParserHeaderField;
        goto reexecute;
      }
      break;
    }
    case parser_state_t::kHttpParserFinished: {
      if (ch != '\n') {
        error_code_ = error_code_t::PARSER_ERROR_PROTOCOL_ERROR;
        return 0;
      }
      if (flags_ != websocket_handshake_flag::WS_HANDSHAKE_FLAG_COMPLETE) {
        error_code_ = error_code_t::PARSER_ERROR_NOT_FOUND_REQUIRED_FIELD;
        return 0;
      }
      state_ = parser_state_t::kHttpParserDone;
      return i + 1;
      break;
    }
    case parser_state_t::kHttpParserHeaderField: {
      if (index_ >= kHttpHeaderElementSize) {
        error_code_ = error_code_t::PARSER_ERROR_FIELD_OUT_OF_RANGE;
        return 0;
      }
      if (ch == ':' && index_) {
        state_ = parser_state_t::kHttpParserFieldValue;
        index_ = 0;
        break;
      }
      lc = tokens[(uint8_t)ch];
      if (lc == ' ' || !lc) {
        error_code_ = error_code_t::PARSER_ERROR_INVALID_HEADER_FIELD;
        return 0;
      }
      if (!index_) {
        switch (lc) {
        case 'o': {
          header_state_ = parser_header_state_t::kHeaderOrigin;
          break;
        }
        case 'h': {
          header_state_ = parser_header_state_t::kHeaderHost;
          break;
        }
        case 'c': {
          header_state_ = parser_header_state_t::kHeaderConnection;
          break;
        }
        case 'u': {
          header_state_ = parser_header_state_t::kHeaderUpgrade;
          break;
        }
        case 's': {
          header_state_ = parser_header_state_t::kHeaderSecWebsocket;
          break;
        }
        }
      } else {
        switch (header_state_) {
        case parser_header_state_t::kHeaderNormal:
          break;
        case parser_header_state_t::kHeaderOrigin: {
          if (index_ > sizeof(kHttpOrigin) - 2)
            header_state_ = parser_header_state_t::kHeaderNormal;
          else if (lc != kHttpOrigin[index_])
            header_state_ = parser_header_state_t::kHeaderNormal;
          break;
        }
        case parser_header_state_t::kHeaderHost: {
          if (index_ > sizeof(kHttpHost) - 2) {
            header_state_ = parser_header_state_t::kHeaderNormal;
            flags_ &= ~websocket_handshake_flag::WS_HANDSHAKE_FLAG_HOST;
          } else if (lc != kHttpHost[index_])
            header_state_ = parser_header_state_t::kHeaderNormal;
          else if (index_ == sizeof(kHttpHost) - 2)
            flags_ |= websocket_handshake_flag::WS_HANDSHAKE_FLAG_HOST;
          break;
        }
        case parser_header_state_t::kHeaderConnection: {
          if (index_ > sizeof(kHttpConnection) - 2) {
            header_state_ = parser_header_state_t::kHeaderNormal;
            flags_ &= ~websocket_handshake_flag::WS_HANDSHAKE_FLAG_CONNECTION;
          } else if (lc != kHttpConnection[index_])
            header_state_ = parser_header_state_t::kHeaderNormal;
          else if (index_ == sizeof(kHttpConnection) - 2)
            flags_ |= websocket_handshake_flag::WS_HANDSHAKE_FLAG_CONNECTION;
          break;
        }
        case parser_header_state_t::kHeaderUpgrade: {
          if (index_ > sizeof(kHttpUpgrade) - 2) {
            header_state_ = parser_header_state_t::kHeaderNormal;
            flags_ &= ~websocket_handshake_flag::WS_HANDSHAKE_FLAG_UPGRADE;
          } else if (lc != kHttpUpgrade[index_])
            header_state_ = parser_header_state_t::kHeaderNormal;
          else if (index_ == sizeof(kHttpUpgrade) - 2)
            flags_ |= websocket_handshake_flag::WS_HANDSHAKE_FLAG_UPGRADE;
          break;
        }
        case parser_header_state_t::kHeaderSecWebsocket: {
          if (index_ > sizeof(kHttpSecWebsocket) - 2) {
            switch (lc) {
            case 'k': {
              header_state_ = parser_header_state_t::kHeaderSecWebsocketKey;
              break;
            }
            case 'v': {
              header_state_ = parser_header_state_t::kHeaderSecWebsocketVersion;
              break;
            }
            case 'p': {
              header_state_ =
                  parser_header_state_t::kHeaderSecWebsocketProtocol;
              break;
            }
            case 'e': {
              header_state_ =
                  parser_header_state_t::kHeaderSecWebsocketExtensions;
              break;
            }
            default: {
              header_state_ = parser_header_state_t::kHeaderNormal;
              break;
            }
            }
            // goto reexecute;
          } else if (lc != kHttpSecWebsocket[index_])
            header_state_ = parser_header_state_t::kHeaderNormal;
          break;
        }
        case parser_header_state_t::kHeaderSecWebsocketKey: {
          if (index_ > sizeof(kHttpSecWebsocketKey) - 2) {
            header_state_ = parser_header_state_t::kHeaderNormal;
            flags_ &=
                ~websocket_handshake_flag::WS_HANDSHAKE_FLAG_WEBSOCKET_KEY;
          } else if (lc != kHttpSecWebsocketKey[index_])
            header_state_ = parser_header_state_t::kHeaderNormal;
          else if (index_ == sizeof(kHttpSecWebsocketKey) - 2)
            flags_ |= websocket_handshake_flag::WS_HANDSHAKE_FLAG_WEBSOCKET_KEY;
          break;
        }
        case parser_header_state_t::kHeaderSecWebsocketVersion: {
          if (index_ > sizeof(kHttpSecWebsocketVersion) - 2) {
            header_state_ = parser_header_state_t::kHeaderNormal;
            flags_ &=
                ~websocket_handshake_flag::WS_HANDSHAKE_FLAG_WEBSOCKET_VERSION;
          } else if (lc != kHttpSecWebsocketVersion[index_])
            header_state_ = parser_header_state_t::kHeaderNormal;
          else if (index_ == sizeof(kHttpSecWebsocketVersion) - 2)
            flags_ |=
                websocket_handshake_flag::WS_HANDSHAKE_FLAG_WEBSOCKET_VERSION;
          break;
        }
        case parser_header_state_t::kHeaderSecWebsocketProtocol: {
          if (index_ > sizeof(kHttpSecWebsocketProtocol) - 2) {
            header_state_ = parser_header_state_t::kHeaderNormal;
          } else if (lc != kHttpSecWebsocketProtocol[index_])
            header_state_ = parser_header_state_t::kHeaderNormal;
          break;
        }
        case parser_header_state_t::kHeaderSecWebsocketExtensions: {
          if (index_ > sizeof(kHttpSecWebsocketExtensions) - 2) {
            header_state_ = parser_header_state_t::kHeaderNormal;
          } else if (lc != kHttpSecWebsocketExtensions[index_])
            header_state_ = parser_header_state_t::kHeaderNormal;
          break;
        }
        }
      }
      ++index_;
      break;
    }
    case parser_state_t::kHttpParserFieldValue: {
      if (count_++ >= kHttpHeaderElementSize) {
        error_code_ = PARSER_ERROR_FIELD_OUT_OF_RANGE;
        return 0;
      }
      if (ch == '\r') {
        if (!index_) {
          error_code_ = PARSER_ERROR_INVALID_FIELD_VALUE;
          return 0;
        }
        state_ = parser_state_t::kHttpParserHeaderLineAlmostDone;
        header_state_ = parser_header_state_t::kHeaderNormal;
        index_ = 0;
        break;
      }
      if ((ch == ' ' || ch == '\t') && !index_)
        break;
      lc = tokens[(uint8_t)ch];
      switch (header_state_) {
      case parser_header_state_t::kHeaderNormal:
        break;
      case parser_header_state_t::kHeaderHost: {
        host_[index_] = ch;
        ++host_length_;
        break;
      }
      case parser_header_state_t::kHeaderOrigin: {
        origin_[index_] = ch;
        ++origin_length_;
        break;
      }
      case parser_header_state_t::kHeaderConnection: {
        if (ch == ',') {
          if (index_ == 0) {
            error_code_ = error_code_t::PARSER_ERROR_PROTOCOL_ERROR;
            return 0;
          }
          index_ = 0;
          goto nextloop;
        }
        if (!index_ && lc == 'u')
          header_state_ = parser_header_state_t::kHeaderConnectionUpgrade;
        break;
      }
      case parser_header_state_t::kHeaderConnectionUpgrade: {
        if (index_ > sizeof(kHttpUpgrade) - 2) {
          if (ch != ' ' && ch != '\t' && ch != ',') {
            flags_ &=
                ~websocket_handshake_flag::WS_HANDSHAKE_FLAG_CONNECTION_UPGRADE;
          } else if (ch == ',')
            header_state_ = parser_header_state_t::kHeaderNormal;
        } else if (lc != kHttpUpgrade[index_]) {
          header_state_ = parser_header_state_t::kHeaderConnection;
        } else if (index_ == sizeof(kHttpUpgrade) - 2) {
          flags_ |=
              websocket_handshake_flag::WS_HANDSHAKE_FLAG_CONNECTION_UPGRADE;
        }
        break;
      }
      case parser_header_state_t::kHeaderUpgrade: {
        if (ch == ',') {
          if (index_ == 0) {
            error_code_ = error_code_t::PARSER_ERROR_PROTOCOL_ERROR;
            return 0;
          }
          index_ = 0;
          goto nextloop;
        }
        if (!index_ && lc == 'w')
          header_state_ = parser_header_state_t::kHeaderUpgradeWebsocket;
        break;
      }
      case parser_header_state_t::kHeaderUpgradeWebsocket: {
        if (index_ > sizeof(kHttpWebsocket) - 2) {
          if (ch != ' ' && ch != '\t' && ch != ',') {
            flags_ &=
                ~websocket_handshake_flag::WS_HANDSHAKE_FLAG_UPGRADE_WEBSOCKET;
          } else if (ch == ',')
            header_state_ = parser_header_state_t::kHeaderNormal;
        } else if (lc != kHttpWebsocket[index_]) {
          header_state_ = parser_header_state_t::kHeaderUpgrade;
        } else if (index_ == sizeof(kHttpWebsocket) - 2) {
          flags_ |=
              websocket_handshake_flag::WS_HANDSHAKE_FLAG_UPGRADE_WEBSOCKET;
        }
        break;
      }
      case parser_header_state_t::kHeaderSecWebsocketKey: {
        if (index_ >= 24) {
          if (ch != ' ' && ch != '\t') {
            error_code_ = PARSER_ERROR_INVALID_FIELD_VALUE;
            return 0;
          }
        } else if (websocket_key_valid[(uint8_t)ch]) {
          websocket_key_[index_] = ch;
        } else {
          error_code_ = PARSER_ERROR_INVALID_FIELD_VALUE;
          return 0;
        }
        break;
      }
      case parser_header_state_t::kHeaderSecWebsocketVersion: {
        if (ch == ',') {
          if (index_ == 0) {
            error_code_ = error_code_t::PARSER_ERROR_PROTOCOL_ERROR;
            return 0;
          }
          index_ = 0;
          goto nextloop;
        }
        if (!index_ && lc == '1')
          header_state_ = parser_header_state_t::kHeaderSecWebsocketVersion13;
        break;
      }
      case parser_header_state_t::kHeaderSecWebsocketVersion13: {
        if (index_ > sizeof(kHttpSecWebsocketVersion13) - 2) {
          if (ch != ' ' && ch != '\t' && ch != ',') {
            flags_ &= ~websocket_handshake_flag::
                          WS_HANDSHAKE_FLAG_WEBSOCKET_VERSION_13;
          } else if (ch == ',')
            header_state_ = parser_header_state_t::kHeaderNormal;
        } else if (lc != kHttpSecWebsocketVersion13[index_]) {
          header_state_ = parser_header_state_t::kHeaderSecWebsocketVersion;
        } else if (index_ == sizeof(kHttpSecWebsocketVersion13) - 2) {
          flags_ |=
              websocket_handshake_flag::WS_HANDSHAKE_FLAG_WEBSOCKET_VERSION_13;
        }
        break;
      }
      case parser_header_state_t::kHeaderSecWebsocketProtocol: {
        // TODO: 后续拓展
        break;
      }
      case parser_header_state_t::kHeaderSecWebsocketExtensions: {
        // TODO: 后续拓展
        break;
      }
      }
      index_++;
    nextloop:
      break;
    }
    case parser_state_t::kHttpParserDone:
      return 0;
      break;
    }
  }
  return i;
}

void HttpParser::Reset() {
  state_ = parser_state_t::kHttpParserStart;
  header_state_ = parser_header_state_t::kHeaderNormal;
  error_code_ = 0;
  index_ = 0;
  count_ = 0;
  request_uri_length_ = 0;
  origin_length_ = 0;
  host_length_ = 0;
}

size_t HttpParser::generate_response_101(void *buffer, const char *key) {
  memcpy(buffer, kHttpResponse101, sizeof(kHttpResponse101) - 1);
  generate_accept_key(key, (char *)buffer + sizeof(kHttpResponse101) - 1);
  ((char *)buffer)[sizeof(kHttpResponse101) + 27] = '\r';
  ((char *)buffer)[sizeof(kHttpResponse101) + 28] = '\n';
  ((char *)buffer)[sizeof(kHttpResponse101) + 29] = '\r';
  ((char *)buffer)[sizeof(kHttpResponse101) + 30] = '\n';
  ((char *)buffer)[sizeof(kHttpResponse101) + 31] = 0;
  return sizeof(kHttpResponse101) + 31;
}

size_t HttpParser::generate_response_400(void *buffer) {
  memcpy(buffer, kHttpResponse400, sizeof(kHttpResponse400) - 1);
  return sizeof(kHttpResponse400) - 1;
}

} // namespace jdocs
