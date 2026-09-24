#pragma once
// time_util.hpp — ISO8601 / timestamp helpers (header-only, zero-dependency).
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace dometrics {

inline int64_t now_unix() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

// Format unix seconds as "YYYY-MM-DD HH:MM:SS" (local time) for DB TIMESTAMP.
inline std::string fmt_timestamp(int64_t unix_sec) {
  std::time_t t = static_cast<std::time_t>(unix_sec);
  std::tm tmv{};
  localtime_r(&t, &tmv);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
  return std::string(buf);
}

inline std::string now_timestamp() { return fmt_timestamp(now_unix()); }

// Format unix seconds as ISO8601 date "YYYY-MM-DD" (local time).
inline std::string fmt_date(int64_t unix_sec) {
  std::time_t t = static_cast<std::time_t>(unix_sec);
  std::tm tmv{};
  localtime_r(&t, &tmv);
  char buf[16];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tmv);
  return std::string(buf);
}

inline std::string today_date() { return fmt_date(now_unix()); }

// Parse "YYYY-MM-DD" -> unix seconds at local midnight. Returns -1 on error.
inline int64_t parse_date(const std::string& ymd) {
  if (ymd.size() != 10 || ymd[4] != '-' || ymd[7] != '-')
    return -1;
  try {
    int y = std::stoi(ymd.substr(0, 4));
    int m = std::stoi(ymd.substr(5, 2));
    int d = std::stoi(ymd.substr(8, 2));
    if (m < 1 || m > 12 || d < 1 || d > 31 || y < 1970 || y > 2100)
      return -1;
    std::tm tmv{};
    tmv.tm_year = y - 1900;
    tmv.tm_mon = m - 1;
    tmv.tm_mday = d;
    tmv.tm_hour = 0;
    tmv.tm_min = 0;
    tmv.tm_sec = 0;
    tmv.tm_isdst = -1;
    std::time_t t = std::mktime(&tmv);
    if (t == (std::time_t)-1)
      return -1;
    return static_cast<int64_t>(t);
  } catch (...) {
    return -1;
  }
}

// Validate "YYYY-MM-DD".
inline bool valid_date(const std::string& ymd) { return parse_date(ymd) >= 0; }

// Unix seconds -> ISO date string (UTC-safe variant for git timestamps).
inline std::string unix_to_date(int64_t unix_sec) { return fmt_date(unix_sec); }

// Human hours formatting: 3661s -> "1.02h".
inline std::string fmt_hours(int64_t seconds) {
  std::ostringstream os;
  os << std::fixed << std::setprecision(2) << (seconds / 3600.0) << "h";
  return os.str();
}

inline std::string fmt_duration(int64_t seconds) {
  int64_t h = seconds / 3600;
  int64_t m = (seconds % 3600) / 60;
  int64_t s = seconds % 60;
  std::ostringstream os;
  os << h << "h " << m << "m " << s << "s";
  return os.str();
}

} // namespace dometrics
