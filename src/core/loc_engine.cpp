// loc_engine.cpp — working-tree line counting implementation.
#include "loc_engine.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace dometrics::loc {

bool is_skipped_dir(const std::string& name) {
  static const char* skip[] = {".git",  ".hg",       ".svn",     "node_modules",
                               "target", "build",     ".build",   "dist",
                               ".venv",  "venv",      "__pycache__", ".idea",
                               ".vscode"};
  for (const char* s : skip)
    if (name == s)
      return true;
  return false;
}

static std::string lowercase_ext(const std::string& filename) {
  auto pos = filename.rfind('.');
  if (pos == std::string::npos)
    return "";
  std::string e = filename.substr(pos);
  for (char& c : e)
    c = (char)std::tolower((unsigned char)c);
  return e;
}

bool is_counted_file(const std::string& filename) {
  // Skip well-known binary / generated artefacts.
  std::string e = lowercase_ext(filename);
  static const char* bin_ext[] = {".png", ".jpg", ".jpeg", ".gif",  ".bmp",
                                  ".ico", ".pdf", ".zip",  ".tar",  ".gz",
                                  ".7z",  ".o",   ".a",    ".so",   ".dylib",
                                  ".dll", ".exe", ".bin",  ".db",   ".sqlite",
                                  ".sqlite3", ".mp4", ".mp3", ".mov", ".wav",
                                  ".ttf", ".otf", ".woff", ".woff2"};
  for (const char* b : bin_ext)
    if (e == b)
      return false;
  std::string base = fs::path(filename).filename().string();
  if (base == "package-lock.json" || base == "yarn.lock" || base == "pnpm-lock.yaml")
    return false;
  return true;
}

long count_file_lines(const std::string& filepath) {
  std::ifstream f(filepath, std::ios::binary);
  if (!f)
    return -1;
  long lines = 0;
  // Binary sniff: NUL byte in first 8KB -> skip.
  char head[8192];
  f.read(head, sizeof(head));
  std::streamsize n = f.gcount();
  for (std::streamsize i = 0; i < n; ++i) {
    if (head[i] == '\0')
      return -1;
  }
  f.clear();
  f.seekg(0);
  std::string line;
  while (std::getline(f, line))
    ++lines;
  return lines;
}

TreeStats count_tree(const std::string& root) {
  TreeStats st;
  std::error_code ec;
  if (!fs::exists(root, ec))
    return st;
  for (auto it = fs::recursive_directory_iterator(root,
                                        fs::directory_options::skip_permission_denied, ec);
       it != fs::recursive_directory_iterator(); ++it) {
    if (ec) {
      ec.clear();
      continue;
    }
    const auto& e = *it;
    if (e.is_directory(ec)) {
      if (is_skipped_dir(e.path().filename().string()))
        it.disable_recursion_pending();
      continue;
    }
    if (!e.is_regular_file(ec))
      continue;
    std::string p = e.path().string();
    if (!is_counted_file(p))
      continue;
    // Cap scan size at 200k files / 5MB per file for speed.
    uintmax_t sz = e.file_size(ec);
    if (!ec && sz > 5 * 1024 * 1024)
      continue;
    long n = count_file_lines(p);
    if (n >= 0) {
      st.lines += n;
      st.files += 1;
    }
    if (st.files > 200000)
      break;
  }
  return st;
}

} // namespace dometrics::loc
