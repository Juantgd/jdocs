// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_SERVICES_DOCUMENT_H_
#define JDOCS_SERVICES_DOCUMENT_H_

#include <cstdint>
#include <deque>
#include <list>
#include <shared_mutex>

#include "ot.h"

namespace jdocs {

class Document {
public:
  Document(const std::string &name);
  ~Document() = default;

  Operation ApplyOp(uint64_t &version, Operation client);

  inline Operation GetContent(uint64_t &version) {
    std::shared_lock<std::shared_mutex> lock(doc_mutex_);
    version = revision_;
    return content_;
  }

  void PushToHistory(Operation op);

  std::list<uint32_t>::iterator JoinUser(uint32_t conn_id);

  void ExitUser(const std::list<uint32_t>::iterator &node);

  // 获取用户列表快照
  std::list<uint32_t> GetUserList();

private:
  std::string name_;
  Operation content_;
  uint64_t revision_{0};
  uint64_t min_revision_{0};
  uint32_t capacity_{50};
  std::deque<Operation> history_;
  std::shared_mutex doc_mutex_;
  std::shared_mutex users_mutex_;

  std::list<uint32_t> users_;
};

} // namespace jdocs

#endif
