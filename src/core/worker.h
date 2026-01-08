// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_CORE_WORKER_H_
#define JDOCS_CORE_WORKER_H_

#include <memory>
#include <unordered_map>

#include <pthread.h>

#include "event_loop.h"
#include "net/tcp_connection.h"

namespace jdocs {

class Worker {
public:
  Worker(JdocsServer *server, std::string name);
  ~Worker();

  Worker(const Worker &) = delete;
  Worker &operator=(const Worker &) = delete;
  Worker(Worker &&) = default;
  Worker &operator=(Worker &&) = default;

  static void *worker_main(void *arg);
  inline struct io_uring *GetRingInstance() {
    return event_loop_->GetRingInstance();
  }

  std::shared_ptr<TcpConnection> GetConnection(uint32_t conn_id);

  void AddConnection(uint32_t conn_id,
                     std::shared_ptr<TcpConnection> connection);

  void DelConnection(uint32_t conn_id);

  const std::string &GetName() const { return name_; }

private:
  pthread_t thread_;
  pthread_barrier_t barrier_;
  std::string name_;
  EventLoop *event_loop_;
  JdocsServer *parent_;

  // 保存着connection_id到TcpConnection类实例的映射
  std::unordered_map<uint32_t, std::shared_ptr<TcpConnection>> conn_map_;
};

} // namespace jdocs

#endif
