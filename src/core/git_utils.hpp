#pragma once
// git_utils.hpp — git introspection + filesystem birth-date detection.
#include <cstdint>
#include <string>
#include <vector>

namespace dometrics::git {

// Is <path> inside (or itself) a git work tree?
bool is_git_repo(const std::string& path);

// Oldest commit unix timestamp; -1 if unavailable.
int64_t oldest_commit_time(const std::string& path);

// HEAD commit hash (empty if unavailable).
std::string head_hash(const std::string& path);

// Total commit count on HEAD; -1 if unavailable.
long commit_count(const std::string& path);

// Aggregate added/removed lines over `git log --numstat` history.
struct Numstat {
  long added = 0;
  long removed = 0;
};
Numstat log_numstat(const std::string& path);

// Lowest filesystem birth time (btime) unix seconds for <path> tree.
// Falls back to oldest mtime when btime is unavailable. -1 on error.
int64_t filesystem_birth(const std::string& path);

// Auto-detect start date "YYYY-MM-DD":
//   1. oldest git commit, 2. filesystem birth, 3. today.
std::string detect_start_date(const std::string& path);

} // namespace dometrics::git
