// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#ifndef JDOCS_CORE_SERVER_H_
#define JDOCS_CORE_SERVER_H_

#include <shared_mutex>
#include <unordered_map>

#include "event_loop.h"
#include "worker.h"

namespace jdocs {

class JdocsServer {
public:
  JdocsServer(int port);
  ~JdocsServer();

  JdocsServer(const JdocsServer &) = delete;
  JdocsServer &operator=(const JdocsServer &) = delete;

  int Run();

  inline int ConnectionIdToRingFd(uint32_t conn_id) {
    return worker_threads_[(conn_id - 1) % nr_threads_]
        .GetRingInstance()
        ->ring_fd;
  }

  inline uint32_t GetNextConnectionId() const { return conn_count_; }

  inline struct io_uring *GetNextRingInstance() {
    return worker_threads_[(conn_count_++ % nr_threads_)].GetRingInstance();
  }
  inline int GetListeningFd() const { return serv_fd_; }

  // 通过用户id获取对应的连接id，不存在则返回0
  static uint32_t GetConnectionId(uint32_t user_id);
  // 添加连接id到连接对象的映射
  static void AddUserSession(uint32_t user_id, uint32_t conn_id);
  // 删除连接id到连接对象的映射
  static void DelUserSession(uint32_t user_id);

private:
  EventLoop event_loop_;
  int serv_fd_;
  // 每个worker线程保存connection_id到TcpConnection实例的映射
  // 而server主线程保存user_id到connection_id的映射
  static std::unordered_map<uint32_t, uint32_t> user_map_;

  std::vector<Worker> worker_threads_;
  unsigned int nr_threads_;
  // 为每个新连接设置一个唯一的连接id，
  // 通过使用连接id %
  // nr_threads_获取该连接所处于的工作线程，方便向工作线程的io_uring实例发送请求
  uint32_t conn_count_{0};

  static std::shared_mutex mutex_;
};

} // namespace jdocs

#endif
