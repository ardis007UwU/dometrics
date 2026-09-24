#pragma once
// watcher.hpp — background file-watch daemon (inotify on Linux, poll fallback).
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace dometrics::daemon {

// Idle timeout: no fs events for this long => pause active-time accumulation.
inline constexpr int kDefaultIdleSeconds = 300; // 5 mins
inline constexpr int kFlushSeconds = 30;        // DB flush cadence

std::string pid_file();
std::string log_file();
std::string idle_file();

// Daemon control (used by `dometrics daemon --start|--stop|--status`).
// Returns 0 on success, nonzero + message on error.
int daemon_start(const std::string& db_path, int idle_seconds);
int daemon_stop();
int daemon_status(); // prints status, 0 if running, 3 if stopped

// Watch a single project path. Blocks until stop_requested() or signal.
// Opens its own DbClient on db_path. idle_seconds<=0 => default.
int watch_project(const std::string& db_path, int64_t project_id,
                  const std::string& root_path, int idle_seconds);

// Watch ALL registered projects (used by the background daemon).
int watch_all(const std::string& db_path, int idle_seconds);

void request_stop();
bool stop_requested();

} // namespace dometrics::daemon
