// git_utils.cpp — subprocess-based git introspection + btime detection.
#include "git_utils.hpp"
#include "time_util.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <sstream>
#include <sys/stat.h>

#if defined(__linux__)
#include <sys/sysmacros.h>
#endif

namespace fs = std::filesystem;

namespace dometrics::git {
namespace {

// RAII popen handle (dedicated deleter avoids -Wignored-attributes on
// function-pointer template arguments).
struct PcloseDeleter {
  void operator()(FILE* f) const noexcept {
    if (f)
      pclose(f);
  }
};
using PipePtr = std::unique_ptr<FILE, PcloseDeleter>;

// Run a shell command, capture stdout (stderr suppressed). Trims trailing \n.
std::string run_capture(const std::string& cmd) {
  std::string out;
  std::string full = cmd + " 2>/dev/null";
  PipePtr pipe(popen(full.c_str(), "r"));
  if (!pipe)
    return out;
  char buf[4096];
  while (std::fgets(buf, sizeof(buf), pipe.get()))
    out += buf;
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
    out.pop_back();
  return out;
}

// Escape a path for safe single-quoted shell embedding.
std::string sh_quote(const std::string& s) {
  std::string r = "'";
  for (char c : s) {
    if (c == '\'')
      r += "'\\''";
    else
      r += c;
  }
  r += "'";
  return r;
}

} // namespace

bool is_git_repo(const std::string& path) {
  std::string r = run_capture("git -C " + sh_quote(path) + " rev-parse --is-inside-work-tree");
  return r == "true";
}

int64_t oldest_commit_time(const std::string& path) {
  if (!is_git_repo(path))
    return -1;
  // Oldest commit epoch: `git log --reverse --format=%ct | head -1`
  std::string r = run_capture("git -C " + sh_quote(path) +
                              " log --reverse --format=%ct -- 2>/dev/null | head -n 1");
  if (r.empty())
    return -1;
  try {
    long long v = std::stoll(r);
    return v > 0 ? static_cast<int64_t>(v) : -1;
  } catch (...) {
    return -1;
  }
}

std::string head_hash(const std::string& path) {
  if (!is_git_repo(path))
    return "";
  return run_capture("git -C " + sh_quote(path) + " rev-parse HEAD");
}

long commit_count(const std::string& path) {
  if (!is_git_repo(path))
    return -1;
  std::string r =
      run_capture("git -C " + sh_quote(path) + " rev-list --count HEAD");
  if (r.empty())
    return -1;
  try {
    return std::stol(r);
  } catch (...) {
    return -1;
  }
}

Numstat log_numstat(const std::string& path) {
  Numstat ns;
  if (!is_git_repo(path))
    return ns;
  // Use popen directly (multi-line output).
  std::string cmd = "git -C " + sh_quote(path) +
                    " log --numstat --format=COMMIT:%H 2>/dev/null";
  PipePtr pipe(popen(cmd.c_str(), "r"));
  if (!pipe)
    return ns;
  char buf[8192];
  while (std::fgets(buf, sizeof(buf), pipe.get())) {
    // numstat lines look like: "<added>\t<removed>\t<path>"
    // Skip COMMIT: lines and blank lines; '-' means binary (skip).
    if (buf[0] == 'C' || buf[0] == '\n' || buf[0] == '\r')
      continue;
    long a = -1, r = -1;
    // parse leading integers
    char* p = buf;
    char* end = nullptr;
    a = std::strtol(p, &end, 10);
    if (end == p)
      continue;
    p = end;
    while (*p == '\t' || *p == ' ')
      ++p;
    if (*p == '-') // binary file
      continue;
    r = std::strtol(p, &end, 10);
    if (end == p)
      continue;
    if (a >= 0)
      ns.added += a;
    if (r >= 0)
      ns.removed += r;
  }
  return ns;
}

int64_t filesystem_birth(const std::string& path) {
  // Strategy: walk the tree (bounded), stat each file's birth time via
  // `stat -c %W` (Linux). %W = birth epoch or 0 if unknown.
  // Fallback: oldest mtime when btime unavailable.
  std::error_code ec;
  if (!fs::exists(path, ec))
    return -1;

#if defined(__linux__)
  auto birth_of = [](const std::string& p) -> int64_t {
    std::string q;
    q.reserve(p.size() + 16);
    // single-quote escape
    q += "stat -c %W '";
    for (char c : p) {
      if (c == '\'')
        q += "'\\''";
      else
        q += c;
    }
    q += "' 2>/dev/null";
    PipePtr pipe(popen(q.c_str(), "r"));
    if (!pipe)
      return -1;
    char b[64] = {0};
    if (!std::fgets(b, sizeof(b), pipe.get()))
      return -1;
    try {
      long long v = std::stoll(b);
      return v > 0 ? (int64_t)v : -1;
    } catch (...) {
      return -1;
    }
  };

  int64_t best = -1;
  int64_t oldest_mtime = -1;
  size_t visited = 0;
  constexpr size_t kMaxFiles = 50000;
  for (auto it = fs::recursive_directory_iterator(path,
                                        fs::directory_options::skip_permission_denied, ec);
       it != fs::recursive_directory_iterator(); ++it) {
    if (ec)
      break;
    if (++visited > kMaxFiles)
      break;
    const auto& e = *it;
    if (e.is_directory(ec)) {
      std::string n = e.path().filename().string();
      if (n == ".git" || n == "node_modules" || n == "target" || n == "build" ||
          n == ".venv" || n == "__pycache__") {
        it.disable_recursion_pending();
      }
      continue;
    }
    if (!e.is_regular_file(ec))
      continue;
    std::string p = e.path().string();
    int64_t b = birth_of(p);
    if (b > 0 && (best < 0 || b < best))
      best = b;
    // track mtime fallback
    struct stat st{};
    if (::stat(p.c_str(), &st) == 0) {
      if (oldest_mtime < 0 || st.st_mtime < oldest_mtime)
        oldest_mtime = st.st_mtime;
    }
  }
  // Also consider the root dir itself.
  int64_t rb = birth_of(path);
  if (rb > 0 && (best < 0 || rb < best))
    best = rb;
  struct stat rst{};
  if (::stat(path.c_str(), &rst) == 0) {
    if (oldest_mtime < 0 || rst.st_mtime < oldest_mtime)
      oldest_mtime = rst.st_mtime;
  }
  if (best > 0)
    return best;
  return oldest_mtime; // fallback (may be -1)
#else
  // macOS / other: oldest mtime walk.
  int64_t oldest = -1;
  for (auto it = fs::recursive_directory_iterator(path,
                                        fs::directory_options::skip_permission_denied, ec);
       it != fs::recursive_directory_iterator(); ++it) {
    if (ec)
      break;
    struct stat st{};
    std::string p = it->path().string();
    if (::stat(p.c_str(), &st) != 0)
      continue;
#if defined(__APPLE__)
    int64_t t = st.st_birthtime > 0 ? (int64_t)st.st_birthtime : (int64_t)st.st_mtime;
#else
    int64_t t = (int64_t)st.st_mtime;
#endif
    if (oldest < 0 || t < oldest)
      oldest = t;
  }
  return oldest;
#endif
}

std::string detect_start_date(const std::string& path) {
  int64_t gt = oldest_commit_time(path);
  if (gt > 0)
    return unix_to_date(gt);
  int64_t bt = filesystem_birth(path);
  if (bt > 0)
    return unix_to_date(bt);
  return today_date();
}

} // namespace dometrics::git
