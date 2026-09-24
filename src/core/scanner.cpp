// scanner.cpp — automatic line-count indexing implementation.
#include "scanner.hpp"
#include "core/git_utils.hpp"
#include "core/loc_engine.hpp"
#include "db/db_client.hpp"

#include <sstream>

namespace dometrics::scan {
namespace {

// Thousand-separator formatting: 47631 -> "47,631".
std::string grouped(long v) {
  bool neg = v < 0;
  unsigned long u = neg ? (unsigned long)(-(v + 1)) + 1u : (unsigned long)v;
  std::string digits = std::to_string(u);
  std::string out;
  int n = 0;
  for (int i = (int)digits.size() - 1; i >= 0; --i) {
    out.insert(out.begin(), digits[(size_t)i]);
    if (++n % 3 == 0 && i > 0)
      out.insert(out.begin(), ',');
  }
  return neg ? "-" + out : out;
}

} // namespace

LineIndex index_project_lines(db::DbClient& db, const db::Project& project) {
  LineIndex idx;
  // Exact history totals straight from git — no estimation, no config.
  git::Numstat ns = git::log_numstat(project.root_path);
  idx.added = ns.added;
  idx.removed = ns.removed;
  // Live working-tree size as net LOC.
  loc::TreeStats tree = loc::count_tree(project.root_path);
  idx.net_loc = tree.lines;
  idx.files = tree.files;
  idx.head = git::head_hash(project.root_path);
  idx.commits = git::commit_count(project.root_path);
  // Persist immediately so `summary` is accurate right after `init`/`scan`.
  idx.snapshot_id =
      db.add_snapshot(project.id, idx.added, idx.removed, idx.net_loc, idx.head);
  return idx;
}

std::string format_line_summary(const LineIndex& idx) {
  std::ostringstream os;
  os << "+" << grouped(idx.added) << " / -" << grouped(idx.removed) << " (git), tree="
     << grouped(idx.net_loc) << " lines in " << grouped(idx.files) << " files";
  return os.str();
}

} // namespace dometrics::scan
