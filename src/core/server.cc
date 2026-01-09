// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "server.h"

#include <thread>

#include <liburing.h>

#include "context.h"
#include "utils/helpers.h"

namespace jdocs {

JdocsServer::JdocsServer(int port) : event_loop_(this, nullptr, false) {
  serv_fd_ = create_listening_socket(port);
  if (serv_fd_ < 0) {
    exit(EXIT_FAILURE);
  }
  unsigned int nr_threads = std::thread::hardware_concurrency();
  nr_threads_ = nr_threads ? nr_threads : 1;
  // nr_threads_ = 1;
  worker_threads_.reserve(nr_threads_);
  char buf[16] = {};
  for (unsigned int i = 0; i < nr_threads_; ++i) {
    int bytes = snprintf(buf, 16, "worker-%u", i);
    assert(bytes > 0);
    worker_threads_.emplace_back(this,
                                 std::string(buf, static_cast<size_t>(bytes)));
  }
}

JdocsServer::~JdocsServer() { close(serv_fd_); }

int JdocsServer::Run() {
  // 准备提交一个accept请求
  struct io_uring_sqe *sqe = event_loop_.GetSqe();
  io_uring_prep_multishot_accept_direct(sqe, serv_fd_, NULL, NULL, 0);
  user_data_encode(sqe, __ACCEPT, 0, serv_fd_, 0);
  // 开始事件循环
  return event_loop_.Run();
}

std::unordered_map<uint32_t, uint32_t> JdocsServer::user_map_;
std::shared_mutex JdocsServer::mutex_;

uint32_t JdocsServer::GetConnectionId(uint32_t user_id) {
  std::shared_lock<std::shared_mutex> lock_(mutex_);
  auto it = user_map_.find(user_id);
  if (it != user_map_.end()) {
    return it->second;
  }
  return 0;
}

void JdocsServer::AddUserSession(uint32_t user_id, uint32_t conn_id) {
  bool ret;
  {
    std::unique_lock<std::shared_mutex> lock_(mutex_);
    ret = user_map_.insert({user_id, conn_id}).second;
  }
  if (!ret) {
    spdlog::error("unordered_map insert failed.");
    exit(EXIT_FAILURE);
  }
}

void JdocsServer::DelUserSession(uint32_t user_id) {
  std::unique_lock<std::shared_mutex> lock_(mutex_);
  auto it = user_map_.find(user_id);
  if (it != user_map_.end()) {
    user_map_.erase(it);
  }
}

} // namespace jdocs
