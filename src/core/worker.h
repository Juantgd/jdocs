// Copyright (c) 2025 Juantgd. All Rights Reserved.

#ifndef JDOCS_CORE_WORKER_H_
#define JDOCS_CORE_WORKER_H_

#include <pthread.h>

#include "event_loop.h"

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

  const std::string &GetName() const { return name_; }

private:
  pthread_t thread_;
  pthread_barrier_t barrier_;
  std::string name_;
  EventLoop *event_loop_;
  JdocsServer *parent_;
};

} // namespace jdocs

#endif
