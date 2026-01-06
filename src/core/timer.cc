// Copyright (c) 2025-2026 Juantgd. All Rights Reserved.

#include "timer.h"

#include "utils/helpers.h"

namespace jdocs {

TimeWheel::TimeWheel(uint32_t tick) : tick_(tick) {
  start_millis_ = get_current_millis();
}

void TimeWheel::timer_cancel(TimeWheel::timer_node *timer) {
  if (timer->IsFired())
    return;
  if (timer->pprev) {
    *(timer->pprev) = timer->next;
    timer->pprev = nullptr;
  }
  if (timer->next) {
    timer->next->pprev = timer->pprev;
    timer->next = nullptr;
  }
}

void TimeWheel::AddTimer(TimeWheel::timer_node *timer, uint32_t millis) {
  timer_cancel(timer);
  uint32_t ticks = millis / tick_;
  if (ticks == 0)
    ticks = 1;
  timer->expires_ = current_tick_ + ticks;
  timer->flag = false;
  __timer_add(timer);
}

void TimeWheel::__timer_add(TimeWheel::timer_node *timer) {
  uint64_t delta_ticks = timer->expires_ - current_tick_;
  size_t i;
  if (delta_ticks < TIME_WHEEL_TVR_SIZE) {
    i = timer->expires_ & TIME_WHEEL_TVR_MASK;
    __insert(&tv1[i], timer);
  } else if (delta_ticks <
             (1ULL << (TIME_WHEEL_TVR_BITS + TIME_WHEEL_TVN_BITS))) {
    i = (timer->expires_ >> TIME_WHEEL_TVR_BITS) & TIME_WHEEL_TVN_MASK;
    __insert(&tv2[i], timer);
  } else if (delta_ticks <
             (1ULL << (TIME_WHEEL_TVR_BITS + 2 * TIME_WHEEL_TVN_BITS))) {
    i = (timer->expires_ >> (TIME_WHEEL_TVR_BITS + TIME_WHEEL_TVN_BITS)) &
        TIME_WHEEL_TVN_MASK;
    __insert(&tv3[i], timer);
  } else if (delta_ticks <
             (1ULL << (TIME_WHEEL_TVR_BITS + 3 * TIME_WHEEL_TVN_BITS))) {
    i = (timer->expires_ >> (TIME_WHEEL_TVR_BITS + 2 * TIME_WHEEL_TVN_BITS)) &
        TIME_WHEEL_TVN_MASK;
    __insert(&tv4[i], timer);
  } else {
    i = (timer->expires_ >> (TIME_WHEEL_TVR_BITS + 3 * TIME_WHEEL_TVN_BITS)) &
        TIME_WHEEL_TVN_MASK;
    __insert(&tv5[i], timer);
  }
}

void TimeWheel::__timer_cascade(TimeWheel::timer_head *tv, size_t index) {
  timer_node *timer = tv[index].first;
  tv[index].first = nullptr;
  while (timer) {
    timer_node *next_timer = timer->next;
    __timer_add(timer);
    timer = next_timer;
  }
}

void TimeWheel::Tick() {
  ++current_tick_;
  size_t index = current_tick_ & TIME_WHEEL_TVR_MASK;
  if (index == 0) {
    uint32_t cascade, i = 0;
    do {
      cascade =
          (current_tick_ >> (TIME_WHEEL_TVR_BITS + TIME_WHEEL_TVN_BITS * i)) &
          TIME_WHEEL_TVN_MASK;
      if (i == 0) {
        __timer_cascade(tv2, cascade);
      } else if (i == 1) {
        __timer_cascade(tv3, cascade);
      } else if (i == 2) {
        __timer_cascade(tv4, cascade);
      } else {
        __timer_cascade(tv5, cascade);
      }
    } while (cascade == 0 && ++i < 4);
  }
  timer_node *timer = tv1[index].first;
  tv1[index].first = nullptr;
  while (timer) {
    // 避免回调函数中修改了timer的next指针导致无法遍历链表
    timer_node *next_timer = timer->next;
    // 触发超时回调
    timer->flag = true;
    timer->callback_();
    timer = next_timer;
  }
}

void TimeWheel::Update() {
  uint64_t now = get_current_millis();
  uint64_t target_ticks = (now - start_millis_) / tick_;
  while (current_tick_ < target_ticks)
    Tick();
}

} // namespace jdocs
