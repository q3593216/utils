#include "ratekeeper.h"
#include <chrono>
#include <thread>
#include <iostream>

#include <algorithm>

static inline uint64_t seconds_since_boot() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000000000ULL + t.tv_nsec;
}

RateKeeper::RateKeeper(uint64_t rate) {
  interval_ = 1000000000ULL / rate;
  last_monitor_time_ = seconds_since_boot();
  next_frame_time_ = last_monitor_time_ + interval_;
}

bool RateKeeper::keepTime() {
  bool lagged = monitorTime();
  if (remaining_ > 0) {
    std::this_thread::sleep_until(std::chrono::steady_clock::time_point(std::chrono::nanoseconds(next_frame_time_)));
  }
  return lagged;
}

bool RateKeeper::monitorTime() {
  ++frame_;
  last_monitor_time_ = seconds_since_boot();
  remaining_ = next_frame_time_ - last_monitor_time_;

  bool lagged = remaining_ < 0;
  if (lagged) {
    if (print_delay_threshold_ > 0 && remaining_ < -print_delay_threshold_) {
      std::cout << "lagging by: "<< -remaining_ << std::endl;
    }
    next_frame_time_ = last_monitor_time_ + interval_;
  } else {
    next_frame_time_ += interval_;
  }
  return lagged;
}
