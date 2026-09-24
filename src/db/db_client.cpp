// db_client.cpp — SQLite WAL persistence implementation.
#include "db_client.hpp"

#include <cstdlib>
#include <filesystem>
#include <sqlite3.h>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace dometrics::db {
namespace {

[[noreturn]] void fail(const std::string& msg, sqlite3* db = nullptr) {
  std::string e = msg;
  if (db) {
    const char* s = sqlite3_errmsg(db);
    if (s)
      e += std::string(": ") + s;
  }
  throw std::runtime_error(e);
}

std::string home_dir() {
  const char* h = std::getenv("HOME");
  if (h && *h)
    return h;
  return "/tmp";
}

std::string canon(const std::string& p) {
  // Canonicalize without requiring existence: absolute + lexically normal.
  std::error_code ec;
  fs::path ap = fs::absolute(fs::path(p), ec);
  if (ec)
    return p;
  ap = ap.lexically_normal();
  std::string s = ap.string();
  if (s.size() > 1 && s.back() == '/')
    s.pop_back();
  return s;
}

bool is_subpath(const std::string& parent, const std::string& child) {
  std::string a = canon(parent), b = canon(child);
  if (a == b)
    return true;
  if (b.size() <= a.size())
    return false;
  return b.compare(0, a.size(), a) == 0 && b[a.size()] == '/';
}

} // namespace

DbClient::DbClient() = default;

DbClient::~DbClient() { close(); }

std::string DbClient::data_dir() {
  const char* x = std::getenv("XDG_CONFIG_HOME");
  if (x && *x)
    return std::string(x) + "/dometrics";
  return home_dir() + "/.config/dometrics";
}

std::string DbClient::legacy_data_dir() { return home_dir() + "/.dometrics"; }

std::string DbClient::default_db_path() { return data_dir() + "/dometrics.db"; }

namespace {
// One-way migration of pre-XDG state: if the new location is missing files
// that exist under the legacy dir, copy them over (best-effort, no throw).
void maybe_migrate_legacy(const std::string& new_db_path) {
  if (new_db_path != DbClient::default_db_path())
    return; // custom DOMETRICS_DB path: never touch user files
  std::error_code ec;
  if (fs::exists(new_db_path, ec))
    return; // already migrated (or fresh init will create it)
  const std::string legacy_db = DbClient::legacy_data_dir() + "/dometrics.db";
  if (!fs::exists(legacy_db, ec))
    return; // nothing to migrate
  fs::create_directories(fs::path(new_db_path).parent_path(), ec);
  // Copy (not move): the legacy file stays as a backup; -wal/-shm sidecars
  // are intentionally left behind so the new handle starts clean.
  fs::copy_file(legacy_db, new_db_path, ec);
  const std::string legacy_idle = DbClient::legacy_data_dir() + "/daemon.idle";
  const std::string new_idle = DbClient::data_dir() + "/daemon.idle";
  if (!ec && fs::exists(legacy_idle, ec) && !fs::exists(new_idle, ec))
    fs::copy_file(legacy_idle, new_idle, ec);
}
} // namespace

void DbClient::open(const std::string& db_path) {
  std::lock_guard<std::mutex> lk(mu_);
  if (db_)
    fail("DbClient::open: already open");
  std::error_code ec;
  maybe_migrate_legacy(db_path);
  fs::create_directories(fs::path(db_path).parent_path(), ec); // best effort
  sqlite3* raw = nullptr;
  int rc = sqlite3_open_v2(db_path.c_str(), &raw,
                           SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                               SQLITE_OPEN_FULLMUTEX,
                           nullptr);
  if (rc != SQLITE_OK) {
    std::string msg = "cannot open sqlite db";
    if (raw) {
      const char* s = sqlite3_errmsg(raw);
      if (s)
        msg += std::string(": ") + s;
      sqlite3_close(raw);
    }
    throw std::runtime_error(msg);
  }
  db_ = raw;
  char* err = nullptr;
  // High-throughput, non-blocking I/O: WAL + busy timeout + NORMAL sync.
  const char* pragmas[] = {"PRAGMA journal_mode=WAL;", "PRAGMA busy_timeout=5000;",
                           "PRAGMA synchronous=NORMAL;", "PRAGMA foreign_keys=ON;",
                           "PRAGMA temp_store=MEMORY;"};
  for (const char* p : pragmas) {
    rc = sqlite3_exec(db_, p, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
      std::string m = err ? err : "pragma failed";
      sqlite3_free(err);
      err = nullptr;
      sqlite3_close(db_);
      db_ = nullptr;
      throw std::runtime_error("sqlite pragma failed (" + std::string(p) + "): " + m);
    }
  }
  // Migrate inline (already holding mu_; migrate() must not re-lock).
  migrate();
  // Database file security: owner-only (0600) for the DB and its WAL/SHM
  // sidecars (created by the WAL pragma above).
  auto restrict = [](const std::string& p) {
    std::error_code pec;
    if (fs::exists(p, pec))
      fs::permissions(p, fs::perms::owner_read | fs::perms::owner_write,
                      fs::perm_options::replace, pec);
  };
  restrict(db_path);
  restrict(db_path + "-wal");
  restrict(db_path + "-shm");
}

void DbClient::close() {
  std::lock_guard<std::mutex> lk(mu_);
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

void DbClient::migrate() {
  // NOTE: caller holds mu_.
  const char* ddl = R"SQL(
CREATE TABLE IF NOT EXISTS projects(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT UNIQUE NOT NULL,
  root_path TEXT NOT NULL,
  parent_path TEXT DEFAULT '',
  start_date TEXT DEFAULT '',
  created_at TEXT DEFAULT (datetime('now','localtime'))
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_projects_root ON projects(root_path);
CREATE TABLE IF NOT EXISTS sessions(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  project_id INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
  start_time TEXT NOT NULL,
  end_time TEXT NOT NULL,
  active_seconds INTEGER NOT NULL DEFAULT 0,
  manual_entry INTEGER NOT NULL DEFAULT 0,
  note TEXT DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_sessions_proj ON sessions(project_id);
CREATE TABLE IF NOT EXISTS snapshots(
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  project_id INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
  timestamp TEXT DEFAULT (datetime('now','localtime')),
  lines_added INTEGER NOT NULL DEFAULT 0,
  lines_removed INTEGER NOT NULL DEFAULT 0,
  net_loc INTEGER NOT NULL DEFAULT 0,
  commit_hash TEXT DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_snapshots_proj ON snapshots(project_id);
)SQL";
  char* err = nullptr;
  int rc = sqlite3_exec(db_, ddl, nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    std::string m = err ? err : "migrate failed";
    sqlite3_free(err);
    fail("sqlite migrate failed: " + m, db_);
  }
}

// ---------------- Projects ----------------

int64_t DbClient::create_project(const std::string& name, const std::string& root_path,
                                 const std::string& parent_path,
                                 const std::string& start_date) {
  std::lock_guard<std::mutex> lk(mu_);
  if (!db_)
    fail("create_project: db not open");
  const char* sql = "INSERT INTO projects(name,root_path,parent_path,start_date,"
                    "created_at) VALUES(?,?,?,?,datetime('now','localtime'))"
                    " ON CONFLICT(name) DO UPDATE SET root_path=excluded.root_path,"
                    " parent_path=excluded.parent_path,"
                    " start_date=excluded.start_date;";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
    fail("prepare create_project", db_);
  sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_TRANSIENT);
  std::string rp = canon(root_path);
  sqlite3_bind_text(st, 2, rp.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 3, parent_path.empty() ? "" : canon(parent_path).c_str(), -1,
                    SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 4, start_date.c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(st);
  sqlite3_finalize(st);
  if (rc != SQLITE_DONE)
    fail("step create_project", db_);
  // Return id of (possibly upserted) row (inline lookup: caller holds mu_,
  // so do NOT call get_project_by_name() here — std::mutex is non-recursive).
  {
    const char* q = "SELECT id FROM projects WHERE name=?1;";
    sqlite3_stmt* s2 = nullptr;
    if (sqlite3_prepare_v2(db_, q, -1, &s2, nullptr) != SQLITE_OK)
      fail("prepare lookup project id", db_);
    sqlite3_bind_text(s2, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    int64_t id = -1;
    if (sqlite3_step(s2) == SQLITE_ROW)
      id = sqlite3_column_int64(s2, 0);
    sqlite3_finalize(s2);
    return id;
  }
}

bool DbClient::update_start_date(int64_t project_id, const std::string& start_date) {
  std::lock_guard<std::mutex> lk(mu_);
  if (!db_)
    fail("update_start_date: db not open");
  const char* sql = "UPDATE projects SET start_date=?1 WHERE id=?2;";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
    fail("prepare update_start_date", db_);
  sqlite3_bind_text(st, 1, start_date.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(st, 2, project_id);
  int rc = sqlite3_step(st);
  sqlite3_finalize(st);
  if (rc != SQLITE_DONE)
    fail("step update_start_date", db_);
  return sqlite3_changes(db_) > 0;
}

static Project row_to_project(sqlite3_stmt* st) {
  Project p;
  p.id = sqlite3_column_int64(st, 0);
  auto txt = [&](int c) -> std::string {
    const unsigned char* t = sqlite3_column_text(st, c);
    return t ? reinterpret_cast<const char*>(t) : "";
  };
  p.name = txt(1);
  p.root_path = txt(2);
  p.parent_path = txt(3);
  p.start_date = txt(4);
  p.created_at = txt(5);
  return p;
}

std::optional<Project> DbClient::get_project_by_id(int64_t id) {
  std::lock_guard<std::mutex> lk(mu_);
  if (!db_)
    fail("get_project_by_id: db not open");
  const char* sql = "SELECT id,name,root_path,parent_path,start_date,created_at"
                    " FROM projects WHERE id=?1;";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
    fail("prepare get_project_by_id", db_);
  sqlite3_bind_int64(st, 1, id);
  std::optional<Project> out;
  if (sqlite3_step(st) == SQLITE_ROW)
    out = row_to_project(st);
  sqlite3_finalize(st);
  return out;
}

std::optional<Project> DbClient::get_project_by_name(const std::string& name) {
  std::lock_guard<std::mutex> lk(mu_);
  if (!db_)
    fail("get_project_by_name: db not open");
  const char* sql = "SELECT id,name,root_path,parent_path,start_date,created_at"
                    " FROM projects WHERE name=?1;";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
    fail("prepare get_project_by_name", db_);
  sqlite3_bind_text(st, 1, name.c_str(), -1, SQLITE_TRANSIENT);
  std::optional<Project> out;
  if (sqlite3_step(st) == SQLITE_ROW)
    out = row_to_project(st);
  sqlite3_finalize(st);
  return out;
}

std::optional<Project> DbClient::get_project_by_path(const std::string& root_path) {
  std::lock_guard<std::mutex> lk(mu_);
  if (!db_)
    fail("get_project_by_path: db not open");
  std::string rp = canon(root_path);
  const char* sql = "SELECT id,name,root_path,parent_path,start_date,created_at"
                    " FROM projects WHERE root_path=?1;";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
    fail("prepare get_project_by_path", db_);
  sqlite3_bind_text(st, 1, rp.c_str(), -1, SQLITE_TRANSIENT);
  std::optional<Project> out;
  if (sqlite3_step(st) == SQLITE_ROW)
    out = row_to_project(st);
  sqlite3_finalize(st);
  return out;
}

std::optional<Project> DbClient::find_project_by_path(const std::string& path) {
  // Exact canonical match first (cheap indexed lookup) ...
  if (auto exact = get_project_by_path(path))
    return exact;
  // ... else nearest registered ancestor walking up the tree, so any
  // subfolder of a project resolves without --path.
  return find_enclosing_project(path);
}

std::optional<Project> DbClient::find_project_by_name(const std::string& name) {
  return get_project_by_name(name);
}

std::optional<Project> DbClient::find_enclosing_project(const std::string& path) {
  // Exact match first, else nearest ancestor (longest root_path prefix).
  auto all = list_projects();
  std::string target = canon(path);
  std::optional<Project> best;
  size_t best_len = 0;
  for (auto& p : all) {
    if (is_subpath(p.root_path, target)) {
      if (p.root_path.size() > best_len) {
        best_len = p.root_path.size();
        best = p;
      }
    }
  }
  return best;
}

std::vector<Project> DbClient::list_projects() {
  std::lock_guard<std::mutex> lk(mu_);
  if (!db_)
    fail("list_projects: db not open");
  const char* sql = "SELECT id,name,root_path,parent_path,start_date,created_at"
                    " FROM projects ORDER BY id;";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
    fail("prepare list_projects", db_);
  std::vector<Project> out;
  while (sqlite3_step(st) == SQLITE_ROW)
    out.push_back(row_to_project(st));
  sqlite3_finalize(st);
  return out;
}

std::vector<Project> DbClient::children_of(const std::string& root_path) {
  std::lock_guard<std::mutex> lk(mu_);
  if (!db_)
    fail("children_of: db not open");
  std::string rp = canon(root_path);
  const char* sql = "SELECT id,name,root_path,parent_path,start_date,created_at"
                    " FROM projects WHERE parent_path=?1 ORDER BY id;";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
    fail("prepare children_of", db_);
  sqlite3_bind_text(st, 1, rp.c_str(), -1, SQLITE_TRANSIENT);
  std::vector<Project> out;
  while (sqlite3_step(st) == SQLITE_ROW)
    out.push_back(row_to_project(st));
  sqlite3_finalize(st);
  return out;
}

Project DbClient::lineage_root(const Project& project) {
  Project cur = project;
  for (int i = 0; i < 64; ++i) { // cycle-safe bound
    if (cur.parent_path.empty())
      return cur;
    auto parent = get_project_by_path(cur.parent_path);
    if (!parent)
      return cur;
    if (parent->id == cur.id)
      return cur;
    cur = *parent;
  }
  return cur;
}

std::vector<Project> DbClient::lineage_of(const Project& project) {
  Project root = lineage_root(project);
  std::vector<Project> all = list_projects();
  std::vector<Project> out;
  out.push_back(root);
  // BFS over parent_path links starting at root.
  for (size_t i = 0; i < out.size(); ++i) {
    for (auto& p : all) {
      if (p.id == out[i].id)
        continue;
      bool already = false;
      for (auto& q : out)
        if (q.id == p.id) {
          already = true;
          break;
        }
      if (already)
        continue;
      if (!p.parent_path.empty() && canon(p.parent_path) == canon(out[i].root_path))
        out.push_back(p);
    }
  }
  return out;
}

// ---------------- Sessions ----------------

int64_t DbClient::add_session(int64_t project_id, const std::string& start_time,
                              const std::string& end_time, int64_t active_seconds,
                              int manual_entry) {
  std::lock_guard<std::mutex> lk(mu_);
  if (!db_)
    fail("add_session: db not open");
  const char* sql = "INSERT INTO sessions(project_id,start_time,end_time,"
                    "active_seconds,manual_entry) VALUES(?,?,?,?,?);";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
    fail("prepare add_session", db_);
  sqlite3_bind_int64(st, 1, project_id);
  sqlite3_bind_text(st, 2, start_time.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 3, end_time.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(st, 4, active_seconds);
  sqlite3_bind_int(st, 5, manual_entry);
  int rc = sqlite3_step(st);
  sqlite3_finalize(st);
  if (rc != SQLITE_DONE)
    fail("step add_session", db_);
  return sqlite3_last_insert_rowid(db_);
}

int64_t DbClient::add_manual_hours(int64_t project_id, double hours,
                                   const std::string& date_ymd,
                                   const std::string& note) {
  std::lock_guard<std::mutex> lk(mu_);
  if (!db_)
    fail("add_manual_hours: db not open");
  int64_t secs = static_cast<int64_t>(hours * 3600.0);
  std::string t0 = date_ymd + " 09:00:00";
  std::string t1 = date_ymd + " 09:00:00";
  const char* sql = "INSERT INTO sessions(project_id,start_time,end_time,"
                    "active_seconds,manual_entry,note) VALUES(?,?,?,?,1,?);";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
    fail("prepare add_manual_hours", db_);
  sqlite3_bind_int64(st, 1, project_id);
  sqlite3_bind_text(st, 2, t0.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(st, 3, t1.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(st, 4, secs);
  sqlite3_bind_text(st, 5, note.c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(st);
  sqlite3_finalize(st);
  if (rc != SQLITE_DONE)
    fail("step add_manual_hours", db_);
  return sqlite3_last_insert_rowid(db_);
}

int64_t DbClient::open_session(int64_t project_id, const std::string& start_time) {
  return add_session(project_id, start_time, start_time, 0, 0);
}

bool DbClient::touch_session(int64_t session_id, const std::string& end_time,
                             int64_t active_seconds) {
  std::lock_guard<std::mutex> lk(mu_);
  if (!db_)
    fail("touch_session: db not open");
  const char* sql = "UPDATE sessions SET end_time=?1,active_seconds=?2 WHERE id=?3;";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
    fail("prepare touch_session", db_);
  sqlite3_bind_text(st, 1, end_time.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(st, 2, active_seconds);
  sqlite3_bind_int64(st, 3, session_id);
  int rc = sqlite3_step(st);
  sqlite3_finalize(st);
  if (rc != SQLITE_DONE)
    fail("step touch_session", db_);
  return sqlite3_changes(db_) > 0;
}

bool DbClient::close_session(int64_t session_id, const std::string& end_time,
                             int64_t active_seconds) {
  return touch_session(session_id, end_time, active_seconds);
}

// ---------------- Snapshots ----------------

int64_t DbClient::add_snapshot(int64_t project_id, long added, long removed, long net_loc,
                               const std::string& commit_hash) {
  std::lock_guard<std::mutex> lk(mu_);
  if (!db_)
    fail("add_snapshot: db not open");
  const char* sql = "INSERT INTO snapshots(project_id,timestamp,lines_added,"
                    "lines_removed,net_loc,commit_hash) VALUES(?,"
                    "datetime('now','localtime'),?,?,?,?);";
  sqlite3_stmt* st = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
    fail("prepare add_snapshot", db_);
  sqlite3_bind_int64(st, 1, project_id);
  sqlite3_bind_int64(st, 2, added);
  sqlite3_bind_int64(st, 3, removed);
  sqlite3_bind_int64(st, 4, net_loc);
  sqlite3_bind_text(st, 5, commit_hash.c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(st);
  sqlite3_finalize(st);
  if (rc != SQLITE_DONE)
    fail("step add_snapshot", db_);
  return sqlite3_last_insert_rowid(db_);
}

ProjectStats DbClient::aggregate_stats(const std::vector<Project>& lineage) {
  std::lock_guard<std::mutex> lk(mu_);
  ProjectStats s;
  if (!db_)
    fail("aggregate_stats: db not open");
  for (auto& p : lineage) {
    // Sessions: split manual vs automatic.
    {
      const char* sql = "SELECT COALESCE(SUM(active_seconds),0),"
                        " COALESCE(SUM(CASE WHEN manual_entry=1 THEN active_seconds ELSE 0 END),0),"
                        " COUNT(*) FROM sessions WHERE project_id=?1;";
      sqlite3_stmt* st = nullptr;
      if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        fail("prepare aggregate sessions", db_);
      sqlite3_bind_int64(st, 1, p.id);
      if (sqlite3_step(st) == SQLITE_ROW) {
        s.active_seconds += sqlite3_column_int64(st, 0);
        s.manual_seconds += sqlite3_column_int64(st, 1);
        s.session_count += sqlite3_column_int64(st, 2);
      }
      sqlite3_finalize(st);
    }
    // Latest snapshot per project (cumulative totals -> take latest row).
    {
      const char* sql = "SELECT lines_added,lines_removed,net_loc,commit_hash,timestamp"
                        " FROM snapshots WHERE project_id=?1 ORDER BY id DESC LIMIT 1;";
      sqlite3_stmt* st = nullptr;
      if (sqlite3_prepare_v2(db_, sql, -1, &st, nullptr) != SQLITE_OK)
        fail("prepare aggregate snapshots", db_);
      sqlite3_bind_int64(st, 1, p.id);
      if (sqlite3_step(st) == SQLITE_ROW) {
        s.lines_added += sqlite3_column_int64(st, 0);
        s.lines_removed += sqlite3_column_int64(st, 1);
        s.net_loc += sqlite3_column_int64(st, 2);
        const unsigned char* h = sqlite3_column_text(st, 3);
        const unsigned char* t = sqlite3_column_text(st, 4);
        if (h && *h && s.last_commit.empty())
          s.last_commit = reinterpret_cast<const char*>(h);
        if (t && *t && s.last_snapshot_at.empty())
          s.last_snapshot_at = reinterpret_cast<const char*>(t);
      }
      sqlite3_finalize(st);
    }
  }
  return s;
}

} // namespace dometrics::db
