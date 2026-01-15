// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "document.h"

#include <spdlog/spdlog.h>

#include "services/document/document_service.h"

namespace jdocs {

Document::Document(const std::string &name) : name_(name) {}

Operation Document::ApplyOp(uint64_t &version, Operation client) {
  std::lock_guard<std::shared_mutex> lock(doc_mutex_);
  // 客户端操作的文档版本太旧或非法版本
  if (version < min_revision_ || version > revision_) {
    spdlog::error("invalid version");
    return {};
  }
  spdlog::warn("apply op function step 1");
  while (version < revision_) {
    client = transform(client, history_[version - min_revision_]);
    ++version;
  }

  spdlog::warn("apply op function step 2");

  ++revision_;
  version = revision_;

  content_ = compose(content_, client);

  spdlog::warn("apply op function step 3");
  PushToHistory(client);

  spdlog::warn("apply op function step 4");
  // 返回转换操作后的操作用于广播
  return client;
}

void Document::PushToHistory(Operation op) {
  if (history_.size() >= capacity_) {
    history_.pop_front();
    ++min_revision_;
  }
  history_.emplace_back(std::move(op));
}

std::list<uint32_t>::iterator Document::JoinUser(uint32_t conn_id) {
  std::lock_guard<std::shared_mutex> lock(users_mutex_);
  spdlog::warn("join user function called.");
  return users_.insert(users_.begin(), conn_id);
}

void Document::ExitUser(const std::list<uint32_t>::iterator &node) {
  std::lock_guard<std::shared_mutex> lock(users_mutex_);
  spdlog::warn("exit user function called.");
  users_.erase(node);
  if (users_.size() == 0) {
    if (!DocumentService::CloseDocument(name_)) {
      spdlog::error("close document failed.");
    }
  }
}

std::list<uint32_t> Document::GetUserList() {
  std::shared_lock<std::shared_mutex> lock(users_mutex_);
  return users_;
}

} // namespace jdocs
