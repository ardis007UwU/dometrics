#pragma once
// db_client.hpp — SQLite WAL-backed persistence for Dometrics.
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace dometrics::db {

struct Project {
  int64_t id = -1;
  std::string name;
  std::string root_path;
  std::string parent_path; // empty = lineage root
  std::string start_date;  // YYYY-MM-DD ISO8601
  std::string created_at;  // YYYY-MM-DD HH:MM:SS
};

struct ProjectStats {
  int64_t active_seconds = 0;
  int64_t manual_seconds = 0;
  int64_t session_count = 0;
  long lines_added = 0;
  long lines_removed = 0;
  long net_loc = 0;
  std::string last_commit;
  std::string last_snapshot_at;
};

class DbClient {
public:
  DbClient();
  ~DbClient();

  DbClient(const DbClient&) = delete;
  DbClient& operator=(const DbClient&) = delete;

  // Open (creating parent dirs) + apply WAL pragmas + migrate schema.
  // Throws std::runtime_error on failure.
  void open(const std::string& db_path);
  void close();
  bool is_open() const { return db_ != nullptr; }

  static std::string default_db_path();
  static std::string data_dir();
  // Pre-XDG location, kept for one-way migration only.
  static std::string legacy_data_dir();

  // --- Projects ---
  int64_t create_project(const std::string& name, const std::string& root_path,
                         const std::string& parent_path, const std::string& start_date);
  bool update_start_date(int64_t project_id, const std::string& start_date);
  std::optional<Project> get_project_by_id(int64_t id);
  std::optional<Project> get_project_by_name(const std::string& name);
  std::optional<Project> get_project_by_path(const std::string& root_path);
  // Smart lookup by path: exact canonical match first, else the nearest
  // registered ancestor walking up the directory tree (so any subfolder of
  // a project resolves without passing --path).
  std::optional<Project> find_project_by_path(const std::string& path);
  // Lookup by registered project name (e.g. `dometrics summary DDCI`).
  std::optional<Project> find_project_by_name(const std::string& name);
  // Nearest enclosing project walking up from <path> (exact or ancestor dir).
  std::optional<Project> find_enclosing_project(const std::string& path);
  std::vector<Project> list_projects();
  // All projects in the same aggregate lineage as <project> (root + descendants).
  std::vector<Project> lineage_of(const Project& project);
  // Ultimate lineage root (follows parent_path chain, cycle-safe).
  Project lineage_root(const Project& project);
  std::vector<Project> children_of(const std::string& root_path);

  // --- Sessions ---
  int64_t add_session(int64_t project_id, const std::string& start_time,
                      const std::string& end_time, int64_t active_seconds,
                      int manual_entry);
  int64_t add_manual_hours(int64_t project_id, double hours,
                           const std::string& date_ymd, const std::string& note);
  int64_t open_session(int64_t project_id, const std::string& start_time);
  bool touch_session(int64_t session_id, const std::string& end_time,
                     int64_t active_seconds);
  bool close_session(int64_t session_id, const std::string& end_time,
                     int64_t active_seconds);

  // --- Snapshots ---
  int64_t add_snapshot(int64_t project_id, long added, long removed, long net_loc,
                       const std::string& commit_hash);

  // Aggregated stats across a lineage (pass lineage project list).
  ProjectStats aggregate_stats(const std::vector<Project>& lineage);

private:
  // All SQL goes through sqlite3_prepare_v2 + bind_* (parameterized only);
  // no raw-SQL escape hatch exists by design.
  void migrate();
  sqlite3* db_ = nullptr;
  mutable std::mutex mu_;
};

} // namespace dometrics::db
