#pragma once
// scanner.hpp — automatic line-count indexing service.
//
// Single place where `git log --numstat` history totals are computed and
// persisted as snapshots. Used by both `dometrics scan` and automatic
// indexing on `dometrics init`, so line totals are available immediately
// without manual configuration.
#include <cstdint>
#include <string>

namespace dometrics::db {
class DbClient;
struct Project;
} // namespace dometrics::db

namespace dometrics::scan {

struct LineIndex {
  long added = 0;      // exact added lines from `git log --numstat`
  long removed = 0;    // exact removed lines from `git log --numstat`
  long net_loc = 0;    // current working-tree size (live LOC)
  long files = 0;      // files counted in the working tree
  long commits = 0;    // HEAD commit count (-1 when not a git repo)
  std::string head;    // HEAD commit hash (empty when unavailable)
  int64_t snapshot_id = -1; // persisted snapshot row id (-1 if not stored)
};

// Compute exact line totals for <project> via `git log --numstat` plus a
// live working-tree count, and persist them as a snapshot row.
// Throws std::runtime_error on DB failure; never throws for missing git
// data (yields zeros instead).
LineIndex index_project_lines(db::DbClient& db, const db::Project& project);

// One-line human summary, e.g. "+47,631 / -410 (git), tree=47,221 lines".
std::string format_line_summary(const LineIndex& idx);

} // namespace dometrics::scan
