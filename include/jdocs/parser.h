// Copyright (c) 2025 Juantgd. All Rights Reserved.

#ifndef JDOCS_INCLUDE_PARSER_H_
#define JDOCS_INCLUDE_PARSER_H_

#include <cstdlib>

namespace jdocs {

// 应用层协议解析器接口类，所有应用层协议的解析器的实现都需要继承该类
class Parser {
public:
  virtual ~Parser() {}
  // 应用层协议解析器的解析函数，传入需要解析的数据以及长度，返回解析了的字符数量
  virtual size_t ParserExecute(void *buffer, size_t length) = 0;
  // 当前解析器是否解析完毕
  inline virtual bool IsDone() const = 0;
  // 重置解析器的状态，使其恢复初始状态
  virtual void Reset() = 0;
  // 获取错误码，0则代表需要更多数据进行解析
  inline virtual int GetErrorCode() const = 0;
  virtual const char *GetError() const = 0;
};

} // namespace jdocs

#endif
