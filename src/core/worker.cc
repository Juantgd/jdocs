// Copyright (c) 2025 Juantgd. All Rights Reserved.

#include "worker.h"

namespace jdocs {

Worker::Worker(JdocsServer *server, std::string name)
    : name_(std::move(name)), parent_(server) {
  pthread_barrier_init(&barrier_, NULL, 2);
  pthread_create(&thread_, NULL, worker_main, this);
  pthread_barrier_wait(&barrier_);
}
Worker::~Worker() {
  pthread_join(thread_, NULL);
  pthread_barrier_destroy(&barrier_);
  if (event_loop_)
    delete event_loop_;
}

void *Worker::worker_main(void *arg) {
  pthread_setname_np(pthread_self(), "worker");
  Worker *worker = static_cast<Worker *>(arg);
  JdocsServer *server = static_cast<JdocsServer *>(worker->parent_);
  worker->event_loop_ = new EventLoop(server, worker, true);
  pthread_barrier_wait(&worker->barrier_);
  worker->event_loop_->Run();
  return NULL;
}

std::shared_ptr<TcpConnection> Worker::GetConnection(uint32_t conn_id) {
  auto it = conn_map_.find(conn_id);
  if (it != conn_map_.end()) {
    return it->second;
  }
  return nullptr;
}

void Worker::AddConnection(uint32_t conn_id,
                           std::shared_ptr<TcpConnection> connection) {
  bool ret = conn_map_.insert({conn_id, std::move(connection)}).second;
  if (!ret) {
    spdlog::error("unordered_map insert failed.");
  }
}

void Worker::DelConnection(uint32_t conn_id) {
  auto it = conn_map_.find(conn_id);
  if (it != conn_map_.end()) {
    conn_map_.erase(it);
  }
}

} // namespace jdocs
