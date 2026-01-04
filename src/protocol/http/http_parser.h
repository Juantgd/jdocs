// Copyright (c) 2025 Juantgd. All Rights Reserved.

#ifndef JDOCS_PROTOCOL_HTTP_PARSER_H_
#define JDOCS_PROTOCOL_HTTP_PARSER_H_

#include <cstdint>

#include <openssl/evp.h>
#include <openssl/sha.h>

#include "parser.h"

namespace jdocs {

namespace {
// http请求报文中头部字段和字段值的最大长度限制，默认256字节
constexpr const uint32_t kHttpHeaderElementSize = 256;

#define kHttpMethod "GET"
#define kHttpVersion "HTTP/1.1"
#define kHttpOrigin "origin"
#define kHttpHost "host"
#define kHttpUpgrade "upgrade"
#define kHttpConnection "connection"
#define kHttpWebsocket "websocket"
#define kHttpSecWebsocket "sec-websocket-"
#define kHttpSecWebsocketKey "sec-websocket-key"
#define kHttpSecWebsocketVersion "sec-websocket-version"
#define kHttpSecWebsocketVersion13 "13"
#define kHttpSecWebsocketProtocol "sec-websocket-protocol"
#define kHttpSecWebsocketExtensions "sec-websocket-extensions"
#define kMagicValue "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

constexpr static char tokens[256] = {
    /*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7
       bel */
    0, 0, 0, 0, 0, 0, 0, 0,
    /*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si
     */
    0, 0, 0, 0, 0, 0, 0, 0,
    /*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23
       etb */
    0, 0, 0, 0, 0, 0, 0, 0,
    /*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us
     */
    0, 0, 0, 0, 0, 0, 0, 0,
    /*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '
     */
    ' ', '!', 0, '#', '$', '%', '&', '\'',
    /*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /
     */
    0, 0, '*', '+', 0, '-', '.', 0,
    /*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7
     */
    '0', '1', '2', '3', '4', '5', '6', '7',
    /*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?
     */
    '8', '9', 0, 0, 0, 0, 0, 0,
    /*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G
     */
    0, 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    /*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O
     */
    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    /*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W
     */
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
    /*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _
     */
    'x', 'y', 'z', 0, 0, 0, '^', '_',
    /*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g
     */
    '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    /* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o
     */
    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    /* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w
     */
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
    /* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127
       del */
    'x', 'y', 'z', 0, '|', 0, '~', 0};

constexpr static const char websocket_key_valid[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};

#define HTTP_PARSER_ERROR_MAP(X)                                               \
  X(0, NEED_MORE_DATA, "Need More Data To Parsing")                            \
  X(1, INVALID_METHOD, "HTTP Request Method Not GET")                          \
  X(2, INVALID_URI, "Request URI Length Out Of Range OR INVALID")              \
  X(3, INVALID_HEADER_FIELD, "Invalid Header Field")                           \
  X(4, INVALID_FIELD_VALUE, "Invalid Field Value")                             \
  X(5, NOT_FOUND_REQUIRED_FIELD,                                               \
    "Not Found The Field Required To Establish WebSocket Connection")          \
  X(6, PROTOCOL_ERROR, "Not Standard HTTP Protocol")                           \
  X(7, FIELD_OUT_OF_RANGE, "Header Field OR Value Out Of Range")

constexpr const char *errors_desc_table[] = {
#define X(n, name, str) str,
    HTTP_PARSER_ERROR_MAP(X)
#undef X
};

#define kHttpResponse400                                                       \
  "HTTP/1.1 400 Bad Request\r\nServer: jdocs_server\r\nContent-Type: "         \
  "text/plain; charset-utf8\r\nContent-Length: 26\r\nConnection: "             \
  "Close\r\n\r\nInvalid Handshake Request!"
#define kHttpResponse101                                                       \
  "HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\n"                \
  "Upgrade: websocket\r\n"                                                     \
  "Server: jdocs_server\r\n"                                                   \
  "Sec-WebSocket-Accept: "

// 用于生成sec-websocket-accept的值
static int generate_accept_key(const char *key, char *buffer) {
  u_char tmp[20];
  SHA1((u_char *)key, 60, tmp);
  return EVP_EncodeBlock((u_char *)buffer, tmp, 20);
}

} // namespace

class HttpParser : public Parser {
public:
  HttpParser() = default;
  ~HttpParser() = default;

  size_t ParserExecute(void *buffer, size_t length) override;
  inline bool IsDone() const override {
    return state_ == parser_state_t::kHttpParserDone;
  }
  void Reset() override;
  inline int GetErrorCode() const override { return error_code_; }
  const char *GetError() const override {
    return errors_desc_table[error_code_];
  }

  inline const char *GetWebSocketKey() const { return websocket_key_; }

  static size_t generate_response_101(void *buffer, const char *key);

  static size_t generate_response_400(void *buffer);

private:
  enum class parser_state_t {
    kHttpParserStart = 0,
    kHttpParserMethod,
    kHttpParserBeforeUri,
    kHttpParserUri,
    kHttpParserAfterUri,
    kHttpParserVersion,
    kHttpParserAfterVersion,
    kHttpParserHeaderField,
    kHttpParserFieldValue,
    kHttpParserHeaderLineAlmostDone,
    kHttpParserHeaderLineDone,
    kHttpParserFinished,
    kHttpParserDone
  };
  enum class parser_header_state_t {
    kHeaderNormal = 0,
    kHeaderOrigin,
    kHeaderHost,
    kHeaderUpgrade,
    kHeaderConnection,

    kHeaderSecWebsocket,
    kHeaderSecWebsocketKey,
    kHeaderSecWebsocketVersion,
    kHeaderSecWebsocketProtocol,
    kHeaderSecWebsocketExtensions,

    kHeaderConnectionUpgrade,
    kHeaderUpgradeWebsocket,
    kHeaderSecWebsocketVersion13
  };

  enum error_code_t {
#define X(n, name, str) PARSER_ERROR_##name = n,
    HTTP_PARSER_ERROR_MAP(X)
#undef X
  };

  // 用于判断websocket协议的http握手报文是否符合规则
  enum websocket_handshake_flag {
    WS_HANDSHAKE_FLAG_HOST = 1 << 0,
    WS_HANDSHAKE_FLAG_CONNECTION = 1 << 1,
    WS_HANDSHAKE_FLAG_CONNECTION_UPGRADE = 1 << 2,
    WS_HANDSHAKE_FLAG_UPGRADE = 1 << 3,
    WS_HANDSHAKE_FLAG_UPGRADE_WEBSOCKET = 1 << 4,
    WS_HANDSHAKE_FLAG_WEBSOCKET_KEY = 1 << 5,
    WS_HANDSHAKE_FLAG_WEBSOCKET_VERSION = 1 << 6,
    WS_HANDSHAKE_FLAG_WEBSOCKET_VERSION_13 = 1 << 7,
    WS_HANDSHAKE_FLAG_COMPLETE = (1 << 8) - 1
  };

  // 当前解析器的解析状态
  parser_state_t state_;
  // 当前解析器中的头部字段解析状态
  parser_header_state_t header_state_;
  // 缓存接收到的http请求报文中uri的值
  char request_uri_[kHttpHeaderElementSize];
  // 缓存origin字段的值
  char origin_[kHttpHeaderElementSize];
  // 缓存host字段的值
  char host_[kHttpHeaderElementSize];
  // 保存sec-websocket-key字段的值，以便生成sec-websocket-accept字段值
  char websocket_key_[62]{0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
                          0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
                          0,   0,   0,   0,   '2', '5', '8', 'E', 'A', 'F',
                          'A', '5', '-', 'E', '9', '1', '4', '-', '4', '7',
                          'D', 'A', '-', '9', '5', 'C', 'A', '-', 'C', '5',
                          'A', 'B', '0', 'D', 'C', '8', '5', 'B', '1', '1'};
  // 检测当前http请求报文的头部字段是否满足websocket握手报文所需
  uint8_t flags_;
  // 保存解析器的错误代码，以便用来生成对应的响应报文
  uint8_t error_code_;
  // 辅助解析函数的执行
  size_t index_;
  // 同上
  size_t count_;
  // request_uri字段长度
  size_t request_uri_length_;
  // origin字段长度
  size_t origin_length_;
  // host字段长度
  size_t host_length_;
};

} // namespace jdocs

#endif
