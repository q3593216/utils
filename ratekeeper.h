#pragma once

#include <cstdint>
#include <string>

class RateKeeper {
public:
  RateKeeper(uint64_t rate);
  ~RateKeeper() {}
  bool keepTime();
  bool monitorTime();
  inline uint64_t frame() const { return frame_; }
  inline uint64_t remaining() const { return remaining_; }

private:
  uint64_t interval_;
  uint64_t next_frame_time_;
  uint64_t last_monitor_time_;
  int64_t remaining_ = 0;
  int64_t print_delay_threshold_ = 0;
  uint64_t frame_ = 0;
};
