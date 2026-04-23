/**
 * @file time.hpp
 * @brief C++14 header-only time library: Duration, Time, Rate.
 *
 * Modern C++14 time utilities refactored from Apollo Cyber RT:
 * - Named constructors: Duration::Milliseconds(100), Time::FromSeconds(1.5)
 * - User-defined literals: 100_ms, 1.5_s, 50_hz
 * - constexpr arithmetic and comparisons
 * - std::chrono interop: FromChrono() / ToChrono()
 * - Zero heap allocation on hot path (ToChars instead of ToString)
 * - Rate with bool return and cycle statistics
 *
 * Naming convention (Google C++ Style):
 * - Types: PascalCase (Duration, Time, Rate, CycleStats)
 * - Methods: PascalCase (ToSecond, FromChrono, Sleep)
 * - Constants: kPascalCase
 * - Members: trailing underscore (nanoseconds_)
 *
 * @copyright Apache License 2.0 (Time, Duration) + BSD License (Rate)
 */

#ifndef ATIME_TIME_HPP_
#define ATIME_TIME_HPP_

#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <thread>

#if defined(ATIME_ENABLE_FORMAT)
#include <ctime>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>
#endif

namespace atime {

// Forward declarations
class Duration;
class Time;

// ============================================================================
// Duration
// ============================================================================

/**
 * @brief Time duration with nanosecond precision.
 *
 * Stores duration as signed int64_t nanoseconds. Range: approximately
 * +/- 292 years. All construction is via named factory methods to
 * eliminate unit ambiguity.
 */
class Duration {
 public:
  // -- Named constructors (all constexpr) ----------------------------------

  /** @brief Create duration from nanoseconds. */
  static constexpr Duration Nanoseconds(int64_t ns) { return Duration(ns); }

  /** @brief Create duration from microseconds. */
  static constexpr Duration Microseconds(int64_t us) {
    return Duration(us * 1000LL);
  }

  /** @brief Create duration from milliseconds. */
  static constexpr Duration Milliseconds(int64_t ms) {
    return Duration(ms * 1000000LL);
  }

  /** @brief Create duration from seconds (integer). */
  static constexpr Duration Seconds(int64_t s) {
    return Duration(s * 1000000000LL);
  }

  /** @brief Create duration from seconds (floating-point). */
  static constexpr Duration Seconds(double s) {
    return Duration(static_cast<int64_t>(s * 1000000000.0));
  }

  /** @brief Create duration from minutes. */
  static constexpr Duration Minutes(int64_t m) {
    return Duration(m * 60000000000LL);
  }

  /** @brief Create zero duration. */
  static constexpr Duration Zero() { return Duration(0); }

  // -- Default constructor ---------------------------------------------------

  constexpr Duration() = default;

  // -- Chrono interop --------------------------------------------------------

  /** @brief Create duration from std::chrono::duration. */
  template <typename Rep, typename Period>
  static Duration FromChrono(std::chrono::duration<Rep, Period> d) {
    return Duration(
        std::chrono::duration_cast<std::chrono::nanoseconds>(d).count());
  }

  /** @brief Convert to std::chrono::duration type. */
  template <typename ChronoDuration>
  ChronoDuration ToChrono() const {
    return std::chrono::duration_cast<ChronoDuration>(
        std::chrono::nanoseconds(nanoseconds_));
  }

  // -- Accessors (all constexpr) ---------------------------------------------

  constexpr int64_t ToNanosecond() const { return nanoseconds_; }

  constexpr int64_t ToMicrosecond() const { return nanoseconds_ / 1000LL; }

  constexpr int64_t ToMillisecond() const { return nanoseconds_ / 1000000LL; }

  constexpr double ToSecond() const {
    return static_cast<double>(nanoseconds_) / 1000000000.0;
  }

  constexpr bool IsZero() const { return nanoseconds_ == 0; }

  constexpr bool IsNegative() const { return nanoseconds_ < 0; }

  // -- Formatting ------------------------------------------------------------

  /**
   * @brief Format duration to char buffer (no heap allocation).
   * @param buf Output buffer.
   * @param size Buffer size.
   * @return Number of characters written (excluding null terminator).
   *
   * Format: "[-]<seconds>.<9-digit-nanoseconds>" e.g., "0.100000000"
   */
  inline int ToChars(char* buf, size_t size) const {
    if (size == 0) return 0;
    int64_t ns = nanoseconds_;
    int pos = 0;
    if (ns < 0) {
      buf[pos++] = '-';
      ns = -ns;
    }
    int64_t sec = ns / 1000000000LL;
    int64_t frac = ns % 1000000000LL;
    pos += std::snprintf(buf + pos, size - static_cast<size_t>(pos),
                         "%lld.%09lld", static_cast<long long>(sec),
                         static_cast<long long>(frac));
    return pos;
  }

  // -- Arithmetic (all constexpr) --------------------------------------------

  constexpr Duration operator+(const Duration& rhs) const {
    return Duration(nanoseconds_ + rhs.nanoseconds_);
  }

  constexpr Duration operator-(const Duration& rhs) const {
    return Duration(nanoseconds_ - rhs.nanoseconds_);
  }

  constexpr Duration operator-() const { return Duration(-nanoseconds_); }

  constexpr Duration operator*(double scale) const {
    return Duration(static_cast<int64_t>(
        static_cast<double>(nanoseconds_) * scale));
  }

  constexpr Duration& operator+=(const Duration& rhs) {
    nanoseconds_ += rhs.nanoseconds_;
    return *this;
  }

  constexpr Duration& operator-=(const Duration& rhs) {
    nanoseconds_ -= rhs.nanoseconds_;
    return *this;
  }

  constexpr Duration& operator*=(double scale) {
    nanoseconds_ = static_cast<int64_t>(
        static_cast<double>(nanoseconds_) * scale);
    return *this;
  }

  // -- Comparison (all constexpr) --------------------------------------------

  constexpr bool operator==(const Duration& rhs) const {
    return nanoseconds_ == rhs.nanoseconds_;
  }
  constexpr bool operator!=(const Duration& rhs) const {
    return nanoseconds_ != rhs.nanoseconds_;
  }
  constexpr bool operator<(const Duration& rhs) const {
    return nanoseconds_ < rhs.nanoseconds_;
  }
  constexpr bool operator>(const Duration& rhs) const {
    return nanoseconds_ > rhs.nanoseconds_;
  }
  constexpr bool operator<=(const Duration& rhs) const {
    return nanoseconds_ <= rhs.nanoseconds_;
  }
  constexpr bool operator>=(const Duration& rhs) const {
    return nanoseconds_ >= rhs.nanoseconds_;
  }

  // -- Blocking --------------------------------------------------------------

  /** @brief Sleep for this duration. */
  inline void Sleep() const {
    std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds_));
  }

#if defined(ATIME_ENABLE_FORMAT)
  /** @brief Convert to human-readable string (heap allocation). */
  inline std::string ToString() const {
    char buf[32];
    ToChars(buf, sizeof(buf));
    return std::string(buf);
  }
#endif

 private:
  explicit constexpr Duration(int64_t ns) : nanoseconds_(ns) {}
  int64_t nanoseconds_ = 0;
};

// -- Duration stream output (optional) ---------------------------------------

#if defined(ATIME_ENABLE_FORMAT)
inline std::ostream& operator<<(std::ostream& os, const Duration& d) {
  char buf[32];
  d.ToChars(buf, sizeof(buf));
  os << buf << "s";
  return os;
}
#endif

// ============================================================================
// Time
// ============================================================================

/**
 * @brief Absolute time point with nanosecond precision.
 *
 * Stores time as unsigned uint64_t nanoseconds since epoch.
 * Now() uses system_clock (wall clock), MonoTime() uses steady_clock.
 */
class Time {
 public:
  // -- Named constructors (all constexpr) ------------------------------------

  /** @brief Create time from nanoseconds since epoch. */
  static constexpr Time FromNanoseconds(uint64_t ns) { return Time(ns); }

  /** @brief Create time from microseconds since epoch. */
  static constexpr Time FromMicroseconds(uint64_t us) {
    return Time(us * 1000ULL);
  }

  /** @brief Create time from milliseconds since epoch. */
  static constexpr Time FromMilliseconds(uint64_t ms) {
    return Time(ms * 1000000ULL);
  }

  /** @brief Create time from seconds (floating-point). */
  static constexpr Time FromSeconds(double s) {
    return Time(static_cast<uint64_t>(s * 1000000000.0));
  }

  /** @brief Maximum representable time. */
  static constexpr Time Max() { return Time(UINT64_MAX); }

  /** @brief Minimum representable time (zero). */
  static constexpr Time Min() { return Time(0); }

  // -- Default constructor ---------------------------------------------------

  constexpr Time() = default;

  // -- Clock sources ---------------------------------------------------------

  /**
   * @brief Get current wall clock time.
   *
   * Uses system_clock (not high_resolution_clock) to guarantee
   * wall clock semantics compatible with ToString().
   */
  static inline Time Now() {
    auto now = std::chrono::system_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                  now.time_since_epoch())
                  .count();
    return Time(static_cast<uint64_t>(ns));
  }

  /**
   * @brief Get monotonic time (immune to NTP/manual adjustments).
   *
   * Uses steady_clock. Suitable for measuring intervals and Rate control.
   */
  static inline Time MonoTime() {
    auto now = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                  now.time_since_epoch())
                  .count();
    return Time(static_cast<uint64_t>(ns));
  }

  /** @brief Sleep until the specified wall clock time. */
  static inline void SleepUntil(const Time& t) {
    auto ns = std::chrono::nanoseconds(t.nanoseconds_);
    std::chrono::system_clock::time_point tp(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(ns));
    std::this_thread::sleep_until(tp);
  }

  // -- Chrono interop --------------------------------------------------------

  /** @brief Convert to std::chrono::time_point. */
  template <typename Clock>
  std::chrono::time_point<Clock> ToTimePoint() const {
    auto ns = std::chrono::nanoseconds(nanoseconds_);
    return std::chrono::time_point<Clock>(
        std::chrono::duration_cast<typename Clock::duration>(ns));
  }

  // -- Accessors (all constexpr) ---------------------------------------------

  constexpr uint64_t ToNanosecond() const { return nanoseconds_; }

  constexpr uint64_t ToMicrosecond() const { return nanoseconds_ / 1000ULL; }

  constexpr uint64_t ToMillisecond() const {
    return nanoseconds_ / 1000000ULL;
  }

  constexpr double ToSecond() const {
    return static_cast<double>(nanoseconds_) / 1000000000.0;
  }

  constexpr bool IsZero() const { return nanoseconds_ == 0; }

  // -- Formatting ------------------------------------------------------------

  /**
   * @brief Format time to char buffer as "seconds.nanoseconds".
   * @param buf Output buffer (recommend 32 bytes).
   * @param size Buffer size.
   * @return Number of characters written.
   *
   * Format: "<epoch_seconds>.<9-digit-nanoseconds>"
   * Example: "1531225311.123456789"
   */
  inline int ToChars(char* buf, size_t size) const {
    if (size == 0) return 0;
    uint64_t sec = nanoseconds_ / 1000000000ULL;
    uint64_t frac = nanoseconds_ % 1000000000ULL;
    return std::snprintf(buf, size, "%llu.%09llu",
                         static_cast<unsigned long long>(sec),
                         static_cast<unsigned long long>(frac));
  }

  // -- Arithmetic (Time - Time = Duration, Time +/- Duration = Time) ---------

  constexpr Duration operator-(const Time& rhs) const {
    return Duration::Nanoseconds(
        static_cast<int64_t>(nanoseconds_ - rhs.nanoseconds_));
  }

  constexpr Time operator+(const Duration& rhs) const {
    return Time(nanoseconds_ +
                static_cast<uint64_t>(rhs.ToNanosecond()));
  }

  constexpr Time operator-(const Duration& rhs) const {
    return Time(nanoseconds_ -
                static_cast<uint64_t>(rhs.ToNanosecond()));
  }

  constexpr Time& operator+=(const Duration& rhs) {
    nanoseconds_ += static_cast<uint64_t>(rhs.ToNanosecond());
    return *this;
  }

  constexpr Time& operator-=(const Duration& rhs) {
    nanoseconds_ -= static_cast<uint64_t>(rhs.ToNanosecond());
    return *this;
  }

  // -- Comparison (all constexpr) --------------------------------------------

  constexpr bool operator==(const Time& rhs) const {
    return nanoseconds_ == rhs.nanoseconds_;
  }
  constexpr bool operator!=(const Time& rhs) const {
    return nanoseconds_ != rhs.nanoseconds_;
  }
  constexpr bool operator<(const Time& rhs) const {
    return nanoseconds_ < rhs.nanoseconds_;
  }
  constexpr bool operator>(const Time& rhs) const {
    return nanoseconds_ > rhs.nanoseconds_;
  }
  constexpr bool operator<=(const Time& rhs) const {
    return nanoseconds_ <= rhs.nanoseconds_;
  }
  constexpr bool operator>=(const Time& rhs) const {
    return nanoseconds_ >= rhs.nanoseconds_;
  }

#if defined(ATIME_ENABLE_FORMAT)
  /**
   * @brief Convert to human-readable date-time string.
   * @return String in format "YYYY-MM-DD HH:MM:SS.nnnnnnnnn"
   */
  inline std::string ToString() const {
    auto ns = std::chrono::nanoseconds(nanoseconds_);
    std::chrono::system_clock::time_point tp(ns);
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    struct tm stm;
    auto ret = localtime_r(&time_t_val, &stm);
    if (ret == nullptr) {
      char buf[32];
      ToChars(buf, sizeof(buf));
      return std::string(buf);
    }

    std::ostringstream ss;
    ss << std::put_time(ret, "%F %T");
    ss << "." << std::setw(9) << std::setfill('0')
       << nanoseconds_ % 1000000000ULL;
    return ss.str();
  }
#endif

 private:
  explicit constexpr Time(uint64_t ns) : nanoseconds_(ns) {}
  uint64_t nanoseconds_ = 0;
};

// -- Time stream output (optional) -------------------------------------------

#if defined(ATIME_ENABLE_FORMAT)
inline std::ostream& operator<<(std::ostream& os, const Time& t) {
  os << t.ToString();
  return os;
}
#endif

// ============================================================================
// CycleStats
// ============================================================================

/** @brief Rate cycle statistics. */
struct CycleStats {
  int64_t min_ns = INT64_MAX;  ///< Minimum cycle time in nanoseconds.
  int64_t max_ns = 0;          ///< Maximum cycle time in nanoseconds.
  int64_t sum_ns = 0;          ///< Sum of all cycle times (for average).
  uint32_t count = 0;          ///< Total number of cycles.
  uint32_t overrun_count = 0;  ///< Number of deadline misses.

  /** @brief Average cycle time as Duration. */
  Duration Average() const {
    if (count == 0) return Duration::Zero();
    return Duration::Nanoseconds(sum_ns / static_cast<int64_t>(count));
  }

  /** @brief Minimum cycle time as Duration. */
  Duration Min() const { return Duration::Nanoseconds(min_ns); }

  /** @brief Maximum cycle time as Duration. */
  Duration Max() const { return Duration::Nanoseconds(max_ns); }
};

// ============================================================================
// Rate
// ============================================================================

/**
 * @brief Frequency-based sleep controller with deadline tracking.
 *
 * Uses steady_clock (monotonic) internally to ensure correct behavior
 * even when system clock is adjusted (NTP, manual change).
 *
 * Usage:
 *   using namespace atime::literals;
 *   Rate rate(50_hz);   // 50 Hz = 20ms period
 *   while (running) {
 *       DoWork();
 *       bool on_time = rate.Sleep();
 *   }
 */
class Rate {
 public:
  /** @brief Create rate controller from period duration. */
  explicit Rate(Duration period)
      : start_(Time::MonoTime()),
        expected_cycle_time_(period),
        actual_cycle_time_(Duration::Zero()),
        stats_() {}

  /**
   * @brief Sleep until the next cycle deadline.
   * @return true if cycle completed on time, false if deadline was missed.
   */
  inline bool Sleep() {
    Time expected_end = start_ + expected_cycle_time_;
    Time actual_end = Time::MonoTime();

    // Detect backward time jump (should not happen with steady_clock,
    // but defensive programming)
    if (actual_end < start_) {
      expected_end = actual_end + expected_cycle_time_;
    }

    Duration sleep_time = expected_end - actual_end;
    actual_cycle_time_ = actual_end - start_;

    // Update statistics
    int64_t cycle_ns = actual_cycle_time_.ToNanosecond();
    if (cycle_ns < stats_.min_ns) stats_.min_ns = cycle_ns;
    if (cycle_ns > stats_.max_ns) stats_.max_ns = cycle_ns;
    stats_.sum_ns += cycle_ns;
    ++stats_.count;

    start_ = expected_end;

    // Check for deadline miss
    if (sleep_time.IsNegative() || sleep_time.IsZero()) {
      ++stats_.overrun_count;
      // Reset if fallen behind by more than one full cycle
      if (actual_end > expected_end + expected_cycle_time_) {
        start_ = actual_end;
      }
      return false;
    }

    // Sleep using steady_clock for monotonic behavior
    auto target = std::chrono::steady_clock::now() +
                  sleep_time.ToChrono<std::chrono::nanoseconds>();
    std::this_thread::sleep_until(target);
    return true;
  }

  /** @brief Reset the cycle timer. */
  inline void Reset() {
    start_ = Time::MonoTime();
    stats_ = CycleStats();
  }

  /** @brief Get actual cycle time of the last cycle. */
  Duration CycleTime() const { return actual_cycle_time_; }

  /** @brief Get expected cycle time (period). */
  Duration ExpectedCycleTime() const { return expected_cycle_time_; }

  /** @brief Get cycle statistics. */
  const CycleStats& statistics() const { return stats_; }

 private:
  Time start_;
  Duration expected_cycle_time_;
  Duration actual_cycle_time_;
  CycleStats stats_;
};

// ============================================================================
// User-Defined Literals
// ============================================================================

namespace literals {

/** @brief Nanosecond literal: 1000_ns */
constexpr Duration operator"" _ns(unsigned long long ns) {
  return Duration::Nanoseconds(static_cast<int64_t>(ns));
}

/** @brief Microsecond literal: 500_us */
constexpr Duration operator"" _us(unsigned long long us) {
  return Duration::Microseconds(static_cast<int64_t>(us));
}

/** @brief Millisecond literal: 100_ms */
constexpr Duration operator"" _ms(unsigned long long ms) {
  return Duration::Milliseconds(static_cast<int64_t>(ms));
}

/** @brief Second literal (integer): 5_s */
constexpr Duration operator"" _s(unsigned long long s) {
  return Duration::Seconds(static_cast<int64_t>(s));
}

/** @brief Second literal (floating-point): 1.5_s */
constexpr Duration operator"" _s(long double s) {
  return Duration::Seconds(static_cast<double>(s));
}

/** @brief Minute literal: 2_min */
constexpr Duration operator"" _min(unsigned long long m) {
  return Duration::Minutes(static_cast<int64_t>(m));
}

/**
 * @brief Frequency literal: 50_hz creates a period Duration.
 *
 * 50_hz = Duration::Nanoseconds(1000000000 / 50) = 20ms period.
 */
constexpr Duration operator"" _hz(unsigned long long hz) {
  return Duration::Nanoseconds(
      static_cast<int64_t>(1000000000ULL / hz));
}

}  // namespace literals

}  // namespace atime

#endif  // ATIME_TIME_HPP_
