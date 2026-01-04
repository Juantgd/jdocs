// Copyright (c) 2025 Juantgd. All Rights Reserved.

#include <liburing.h>
#include <thread>

#include "context.h"
#include "server.h"
#include "utils/helpers.h"

namespace jdocs {

JdocsServer::JdocsServer(int port) : event_loop_(this, nullptr, false) {
  serv_fd_ = create_listening_socket(port);
  if (serv_fd_ < 0) {
    exit(EXIT_FAILURE);
  }
  unsigned int nr_threads = std::thread::hardware_concurrency();
  nr_threads_ = nr_threads ? nr_threads : 1;
  worker_threads_.reserve(nr_threads);
  char buf[16] = {};
  for (unsigned int i = 0; i < nr_threads; ++i) {
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

std::shared_ptr<TcpConnection> JdocsServer::GetConnection(uint32_t conn_id) {
  std::shared_lock<std::shared_mutex> lock_(mutex_);
  auto it = conn_map_.find(conn_id);
  if (it != conn_map_.end()) {
    return it->second;
  }
  return nullptr;
}

void JdocsServer::AddConnection(uint32_t conn_id,
                                std::shared_ptr<TcpConnection> connection) {
  bool ret;
  {
    std::unique_lock<std::shared_mutex> lock_(mutex_);
    ret = conn_map_.insert({conn_id, std::move(connection)}).second;
  }
  if (!ret) {
    spdlog::error("unordered_map insert failed.");
    exit(EXIT_FAILURE);
  }
}

void JdocsServer::DelConnection(uint32_t conn_id) {
  std::unique_lock<std::shared_mutex> lock_(mutex_);
  auto it = conn_map_.find(conn_id);
  if (it != conn_map_.end()) {
    conn_map_.erase(it);
  }
}

} // namespace jdocs
