// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "protocol/http/http_parser.h"

#include <gtest/gtest.h>
using namespace jdocs;

const char *request1[] = {"GET /path HTTP/1.1\r\n",
                          "Host: example.com:8000\r\n",
                          "Upgrade: websocket\r\n",
                          "Connection: Upgrade\r\n",
                          "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n",
                          "Sec-WebSocket-Version: 13\r\n\r\n"};

const char *request2[] = {"GET /path/%e6%96%87%e4%bb%b6.txt HTTP/1.1\r\n",
                          "Host: example.com:8000\r\n",
                          "Upgrade: websocket\r\n",
                          "Connection: Upgrade\r\n",
                          "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n",
                          "Sec-WebSocket-Version: 13\r\n\r\n"};

// 我的文档.docx
const char *request3[] = {
    "GET /file/%e6%88%91%e7%9a%84%e6%96%87%e6%a1%a3.docx HTTP/1.1\r\n",
    "Host: example.com:8000\r\n",
    "Upgrade: websocket\r\n",
    "Connection: Upgrade\r\n",
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n",
    "Sec-WebSocket-Version: 13\r\n\r\n"};

// 参数1=测试
const char *request4[] = {
    "GET /path?%e5%8f%82%e6%95%b01=%e6%b5%8b%e8%af%95 HTTP/1.1\r\n",
    "Host: example.com:8000\r\n",
    "Upgrade: websocket\r\n",
    "Connection: Upgrade\r\n",
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n",
    "Sec-WebSocket-Version: 13\r\n\r\n"};

// kw=操作系统&时间=2026年
const char *request5[] = {
    "GET /search?kw=%e6%93%8d%e4%bd%9c%e7%b3%bb%e7%bb%9f&%e6%",
    "97%b6%e9%97%b4=2026%e5%b9%b4 HTTP/1.1\r\n",
    "Host: example.com:8000\r\n",
    "Upgrade: websocket\r\n",
    "Connection: Upgrade\r\n",
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n",
    "Sec-WebSocket-Version: 13\r\n\r\n"};

const char *request6[] = {
    "GET /test?file_list=abc.xls&file_list=word.docx&file_",
    "list=video.mp4 HTTP/1.1\r\n",
    "Host: example.com:8000\r\n",
    "Upgrade: websocket\r\n",
    "Connection: Upgrade\r\n",
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n",
    "Sec-WebSocket-Version: 13\r\n\r\n"};

TEST(HttpParserTest, HttpParserUriTest) {
  {
    HttpParser parser;
    uint32_t count = sizeof(request1) / sizeof(request1[0]);
    parser.ParserExecute((void *)request1[0], strlen(request1[0]));
    ASSERT_FALSE(parser.IsDone());
    for (uint32_t i = 1; i < count; ++i) {
      parser.ParserExecute((void *)request1[i], strlen(request1[i]));
    }
    ASSERT_TRUE(parser.IsDone());
    ASSERT_STREQ(parser.location_.c_str(), "/path");
    ASSERT_STREQ(parser.host_.c_str(), "example.com:8000");
    ASSERT_TRUE(parser.origin_.empty());
  }
  {
    HttpParser parser;
    uint32_t count = sizeof(request2) / sizeof(request2[0]);
    for (uint32_t i = 0; i < count; ++i) {
      parser.ParserExecute((void *)request2[i], strlen(request2[i]));
    }
    ASSERT_TRUE(parser.IsDone());
    ASSERT_STREQ(parser.location_.c_str(), "/path/文件.txt");
  }
  {
    HttpParser parser;
    uint32_t count = sizeof(request3) / sizeof(request3[0]);
    for (uint32_t i = 0; i < count; ++i) {
      parser.ParserExecute((void *)request3[i], strlen(request3[i]));
    }
    ASSERT_TRUE(parser.IsDone());
    ASSERT_STREQ(parser.location_.c_str(), "/file/我的文档.docx");
  }
  {
    HttpParser parser;
    uint32_t count = sizeof(request4) / sizeof(request4[0]);
    for (uint32_t i = 0; i < count; ++i) {
      parser.ParserExecute((void *)request4[i], strlen(request4[i]));
    }
    ASSERT_TRUE(parser.IsDone());
    ASSERT_STREQ(parser.location_.c_str(), "/path");
    ASSERT_STREQ(parser.query_args_["参数1"][0].c_str(), "测试");
  }
  {
    HttpParser parser;
    uint32_t count = sizeof(request5) / sizeof(request5[0]);
    for (uint32_t i = 0; i < count; ++i) {
      parser.ParserExecute((void *)request5[i], strlen(request5[i]));
    }
    ASSERT_TRUE(parser.IsDone());
    ASSERT_STREQ(parser.location_.c_str(), "/search");
    ASSERT_EQ(parser.query_args_["kw"].size(), 1);
    ASSERT_STREQ((parser.query_args_["kw"][0]).c_str(), "操作系统");
    ASSERT_STREQ((parser.query_args_["时间"][0]).c_str(), "2026年");
  }
  {
    HttpParser parser;
    uint32_t count = sizeof(request6) / sizeof(request6[0]);
    for (uint32_t i = 0; i < count; ++i) {
      parser.ParserExecute((void *)request6[i], strlen(request6[i]));
    }
    ASSERT_TRUE(parser.IsDone());
    ASSERT_STREQ(parser.location_.c_str(), "/test");
    ASSERT_EQ(parser.query_args_["file_list"].size(), 3);
    std::vector<std::string> &files = parser.query_args_["file_list"];
    ASSERT_STREQ(files[0].c_str(), "abc.xls");
    ASSERT_STREQ(files[1].c_str(), "word.docx");
    ASSERT_STREQ(files[2].c_str(), "video.mp4");
  }
}
