// cli_router.cpp — parsing + handlers for init/daemon/log/summary/scan.
#include "cli_router.hpp"
#include "core/git_utils.hpp"
#include "core/loc_engine.hpp"
#include "core/scanner.hpp"
#include "core/time_util.hpp"
#include "daemon/watcher.hpp"
#include "db/db_client.hpp"

#include <cstdlib>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>

namespace fs = std::filesystem;
using dometrics::db::DbClient;

namespace dometrics::cli {
namespace {

std::string get_cwd() {
  std::error_code ec;
  std::string s = fs::current_path(ec).string();
  return ec ? std::string(".") : s;
}

std::string canon(const std::string& p) {
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

std::string default_name_for(const std::string& path) {
  return fs::path(canon(path)).filename().string();
}

std::string db_path_for_env() {
  const char* e = std::getenv("DOMETRICS_DB");
  if (e && *e)
    return e;
  return DbClient::default_db_path();
}

// Smart project resolution — the user never needs --path once initialized:
//   1. explicit --path  -> enclosing registered project for that path;
//   2. positional NAME  -> registered project with that name (e.g.
//      `dometrics summary DDCI`), falling back to path interpretation;
//   3. otherwise        -> enclosing registered project for the current
//      working directory (any subfolder resolves via parent walk-up).
// When a NAME was explicitly given, *name_given is set so callers can report
// "no project named 'X'" instead of silently falling back.
std::optional<db::Project>
resolve_target_project(DbClient& db, const std::map<std::string, std::string>& opt,
                       const std::vector<std::string>& positionals,
                       const std::vector<std::string>& skip_flags, bool* name_given) {
  if (name_given)
    *name_given = false;
  auto it = opt.find("path");
  if (it != opt.end())
    return db.find_project_by_path(canon(it->second));
  for (const auto& tok : positionals) {
    bool skip = false;
    for (const auto& f : skip_flags)
      if (tok == f) {
        skip = true;
        break;
      }
    if (skip || tok.empty() || tok[0] == '-')
      continue;
    if (name_given)
      *name_given = true;
    if (auto by_name = db.find_project_by_name(tok))
      return by_name;
    return db.find_project_by_path(canon(tok)); // maybe a path, not a name
  }
  return db.find_project_by_path(canon(get_cwd()));
}

// --- handlers ---------------------------------------------------------------

int handle_init(DbClient& db, const std::map<std::string, std::string>& opt) {
  std::string path = get_cwd();
  if (auto it = opt.find("path"); it != opt.end())
    path = it->second;
  path = canon(path);

  std::error_code ec;
  fs::create_directories(path, ec);

  std::string name;
  if (auto it = opt.find("name"); it != opt.end())
    name = it->second;
  if (name.empty())
    name = default_name_for(path);
  if (name.empty()) {
    std::cerr << "init: cannot derive project name; pass --name <name>\n";
    return 1;
  }

  std::string parent;
  if (auto it = opt.find("parent"); it != opt.end())
    parent = canon(it->second);

  std::string start;
  if (auto it = opt.find("start-date"); it != opt.end())
    start = it->second;
  if (!start.empty() && !valid_date(start)) {
    std::cerr << "init: invalid --start-date '" << start << "' (want YYYY-MM-DD)\n";
    return 1;
  }
  bool auto_detect = start.empty();
  if (auto_detect)
    start = git::detect_start_date(path);

  try {
    int64_t id = db.create_project(name, path, parent, start);
    if (id < 0) {
      std::cerr << "init: failed to register project\n";
      return 1;
    }
    std::cout << "Initialized project '" << name << "' (id=" << id << ")\n"
              << "  root:   " << path << "\n"
              << "  parent: " << (parent.empty() ? "(none)" : parent) << "\n"
              << "  start:  " << start << (auto_detect ? "  (auto-detected)" : "  (manual)")
              << "\n";
    // Automatic line indexing: compute exact `git log --numstat` totals
    // immediately so `summary` is accurate without a manual `scan`.
    // Best-effort only — indexing must never fail `init`.
    try {
      auto proj = db.get_project_by_id(id);
      if (proj) {
        scan::LineIndex idx = scan::index_project_lines(db, *proj);
        std::cout << "  indexed: " << scan::format_line_summary(idx) << "\n";
      }
    } catch (const std::exception& e) {
      std::cerr << "  warning: automatic line indexing failed (" << e.what()
                << "); run `dometrics scan` later.\n";
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "init failed: " << e.what() << "\n";
    return 1;
  }
}

int handle_daemon(DbClient& db, const std::vector<std::string>& args,
                  const std::map<std::string, std::string>& opt) {
  (void)db;
  std::string dbp = db_path_for_env();
  bool want_start = false, want_stop = false, want_status = false, foreground = false;
  int idle = daemon::kDefaultIdleSeconds;
  if (auto it = opt.find("idle"); it != opt.end()) {
    try {
      idle = std::stoi(it->second);
    } catch (...) {
      std::cerr << "daemon: invalid --idle value\n";
      return 1;
    }
    if (idle <= 0) {
      std::cerr << "daemon: --idle must be > 0 seconds\n";
      return 1;
    }
  } else if (const char* e = std::getenv("DOMETRICS_IDLE_TIMEOUT")) {
    try {
      int v = std::stoi(e);
      if (v > 0)
        idle = v;
    } catch (...) {
    }
  }
  for (auto& a : args) {
    if (a == "--start" || a == "start")
      want_start = true;
    else if (a == "--stop" || a == "stop")
      want_stop = true;
    else if (a == "--status" || a == "status")
      want_status = true;
    else if (a == "--foreground" || a == "-f" || a == "foreground")
      foreground = true;
  }
  // Flags may also arrive via the options map (parse_opts stores bare
  // `--start` as opt["start"]="1").
  if (opt.count("start"))
    want_start = true;
  if (opt.count("stop"))
    want_stop = true;
  if (opt.count("status"))
    want_status = true;
  if (opt.count("foreground") || opt.count("f"))
    foreground = true;
  if (!want_start && !want_stop && !want_status)
    want_status = true; // bare `daemon` => status
  if ((int)want_start + (int)want_stop + (int)want_status > 1) {
    std::cerr << "daemon: pick one of --start | --stop | --status\n";
    return 1;
  }
  if (want_stop)
    return daemon::daemon_stop();
  if (want_status)
    return daemon::daemon_status();
  // start
  if (foreground)
    return daemon::watch_all(dbp, idle);
  return daemon::daemon_start(dbp, idle);
}

int handle_log(DbClient& db, const std::vector<std::string>& args,
               const std::map<std::string, std::string>& opt) {
  auto it = opt.find("add-hours");
  if (it == opt.end()) {
    std::cerr << "log: missing --add-hours <float>\n";
    return 1;
  }
  double hours = 0;
  try {
    hours = std::stod(it->second);
  } catch (...) {
    std::cerr << "log: invalid --add-hours value\n";
    return 1;
  }
  if (hours <= 0 || hours > 24 * 365) {
    std::cerr << "log: --add-hours must be in (0, 8760]\n";
    return 1;
  }
  std::string date = today_date();
  if (auto d = opt.find("date"); d != opt.end())
    date = d->second;
  if (!valid_date(date)) {
    std::cerr << "log: invalid --date '" << date << "' (want YYYY-MM-DD)\n";
    return 1;
  }
  std::string note;
  if (auto n = opt.find("note"); n != opt.end())
    note = n->second;

  auto proj = resolve_target_project(db, opt, args, {}, nullptr);
  if (!proj) {
    std::cerr << "log: no project for this path or name; run `dometrics init` first.\n";
    return 1;
  }
  try {
    int64_t sid = db.add_manual_hours(proj->id, hours, date, note);
    std::cout << "Logged " << hours << "h on " << date << " for '" << proj->name
              << "' (session " << sid << ")\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "log failed: " << e.what() << "\n";
    return 1;
  }
}

std::string json_escape(const std::string& s) {
  std::ostringstream os;
  for (char c : s) {
    switch (c) {
    case '"':
      os << "\\\"";
      break;
    case '\\':
      os << "\\\\";
      break;
    case '\n':
      os << "\\n";
      break;
    case '\r':
      os << "\\r";
      break;
    case '\t':
      os << "\\t";
      break;
    default:
      if ((unsigned char)c < 0x20) {
        os << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
      } else
        os << c;
    }
  }
  return os.str();
}

// Thousand-separator formatting for dashboard numbers: 47631 -> "47,631".
std::string fmt_grouped(long v) {
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

// Minimalist single-block dashboard renderer. `scope` is the lineage root;
// `lineage` holds every repo aggregated into the totals. Linked-repo and
// hierarchy boilerplate is omitted entirely for single-repo lineages.
void render_dashboard(DbClient& db, const db::Project& scope,
                      const std::vector<db::Project>& lineage) {
  auto st = db.aggregate_stats(lineage);
  long commits = 0;
  for (auto& m : lineage) {
    long c = git::commit_count(m.root_path);
    if (c > 0)
      commits += c;
  }
  // Lifetime active is the sole time metric: tracked daemon time and manual
  // backfills are already combined in active_seconds.
  double total_h = st.active_seconds / 3600.0;

  auto row = [](const std::string& label, const std::string& value) {
    std::cout << "  " << std::left << std::setw(15) << label << ": " << value << "\n";
  };

  std::ostringstream hours;
  hours << std::fixed << std::setprecision(2) << total_h << "h  ("
        << fmt_duration(st.active_seconds) << ")";
  std::ostringstream lines;
  lines << "+" << fmt_grouped(st.lines_added) << " / -" << fmt_grouped(st.lines_removed)
        << "  (net " << fmt_grouped(st.net_loc) << " LOC)";

  std::cout << "\n";
  row("Project", scope.name);
  row("Path", scope.root_path);
  std::cout << "  ------------------------------------------------------------\n";
  row("Lifetime active", hours.str());
  row("Total lines", lines.str());
  row("Start date", scope.start_date);
  row("Commits", fmt_grouped(commits));
  row("Last snapshot", st.last_snapshot_at.empty() ? "(none)" : st.last_snapshot_at);
  if (lineage.size() > 1) {
    std::ostringstream linked;
    for (size_t i = 1; i < lineage.size(); ++i) {
      if (i > 1)
        linked << ", ";
      linked << lineage[i].name;
    }
    row("Linked repos", linked.str());
  }
  std::cout << "\n";
}

int handle_summary(DbClient& db, const std::vector<std::string>& args,
                   const std::map<std::string, std::string>& opt) {
  bool as_json = false, tui = false;
  for (auto& a : args) {
    if (a == "--json" || a == "json")
      as_json = true;
    else if (a == "--tui" || a == "tui")
      tui = true;
  }
  // Bare `--json`/`--tui` are stored in the options map by parse_opts.
  if (opt.count("json"))
    as_json = true;
  if (opt.count("tui"))
    tui = true;
  (void)tui; // --tui forces the ASCII dashboard (the default); accepted for compat.

  auto projects = db.list_projects();
  if (projects.empty()) {
    if (as_json) {
      std::cout << "{\"projects\":[],\"error\":\"no projects; run dometrics init\"}\n";
      return 0;
    }
    std::cout << "No projects tracked yet. Run `dometrics init` first.\n";
    return 0;
  }

  // Focus: explicit NAME/--path, else enclosing project of the cwd;
  // outside any project, fall back to all lineage roots.
  bool named = false;
  std::optional<db::Project> focus =
      resolve_target_project(db, opt, args, {"json", "tui"}, &named);
  std::vector<db::Project> scopes;
  if (focus) {
    scopes.push_back(db.lineage_root(*focus));
  } else if (named) {
    std::cerr << "summary: no project matches; run `dometrics init` first.\n";
    return 1;
  } else {
    for (auto& p : projects)
      if (p.parent_path.empty())
        scopes.push_back(p);
    if (scopes.empty())
      scopes = projects;
  }

  if (as_json) {
    std::ostringstream os;
    os << "{\n  \"projects\": [\n";
    bool first = true;
    for (auto& scope : scopes) {
      auto lin = db.lineage_of(scope);
      auto st = db.aggregate_stats(lin);
      long commits = 0;
      for (auto& m : lin) {
        long c = git::commit_count(m.root_path);
        if (c > 0)
          commits += c;
      }
      for (auto& m : lin) {
        if (!first)
          os << ",\n";
        first = false;
        auto mst = db.aggregate_stats({m});
        os << "    {\"id\":" << m.id << ",\"name\":\"" << json_escape(m.name)
           << "\",\"root_path\":\"" << json_escape(m.root_path) << "\",\"parent_path\":\""
           << json_escape(m.parent_path) << "\",\"start_date\":\"" << m.start_date
           << "\",\"active_seconds\":" << mst.active_seconds << ",\"manual_seconds\":"
           << mst.manual_seconds << ",\"net_loc\":" << mst.net_loc << "}";
      }
      (void)st;
      (void)commits;
    }
    os << "\n  ]\n}\n";
    std::cout << os.str();
    return 0;
  }

  // ASCII dashboard.
  std::cout << "+==============================================================+\n"
            << "|                    D O M E T R I C S                         |\n"
            << "+==============================================================+";
  for (auto& scope : scopes)
    render_dashboard(db, scope, db.lineage_of(scope));
  std::cout << "+==============================================================+\n";
  return 0;
}

int handle_scan(DbClient& db, const std::vector<std::string>& args,
                const std::map<std::string, std::string>& opt) {
  std::vector<db::Project> targets;
  bool named = false;
  auto focus = resolve_target_project(db, opt, args, {}, &named);
  if (focus) {
    // Scan the whole lineage so aggregates stay consistent.
    targets = db.lineage_of(db.lineage_root(*focus));
  } else if (named) {
    std::cerr << "scan: no project matches; run `dometrics init` first.\n";
    return 1;
  } else {
    targets = db.list_projects();
  }
  if (targets.empty()) {
    std::cerr << "scan: no projects; run `dometrics init` first.\n";
    return 1;
  }
  int rc = 0;
  for (auto& p : targets) {
    try {
      // Exact history totals via `git log --numstat`, persisted immediately.
      scan::LineIndex idx = scan::index_project_lines(db, p);
      std::cout << "Scanned '" << p.name << "': " << scan::format_line_summary(idx)
                << ", commits=" << (idx.commits < 0 ? 0 : idx.commits) << " [snapshot "
                << idx.snapshot_id << "]\n";
    } catch (const std::exception& e) {
      std::cerr << "scan failed for '" << p.name << "': " << e.what() << "\n";
      rc = 1;
    }
  }
  return rc;
}

// --- arg parsing ------------------------------------------------------------

std::map<std::string, std::string> parse_opts(const std::vector<std::string>& args,
                                              size_t from,
                                              std::vector<std::string>& positionals) {
  auto looks_like_number = [](const std::string& s) {
    if (s.empty())
      return false;
    size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    bool dot = false, digit = false;
    for (; i < s.size(); ++i) {
      if (std::isdigit((unsigned char)s[i]))
        digit = true;
      else if (s[i] == '.' && !dot)
        dot = true;
      else
        return false;
    }
    return digit;
  };
  std::map<std::string, std::string> opt;
  for (size_t i = from; i < args.size(); ++i) {
    const std::string& a = args[i];
    if (a.rfind("--", 0) == 0) {
      std::string k = a.substr(2);
      std::string v;
      auto eq = k.find('=');
      if (eq != std::string::npos) {
        v = k.substr(eq + 1);
        k = k.substr(0, eq);
      } else if (i + 1 < args.size() &&
                 (args[i + 1].rfind("-", 0) != 0 || looks_like_number(args[i + 1]))) {
        v = args[++i];
      } else {
        v = "1"; // boolean flag
      }
      opt[k] = v;
    } else if (a.rfind("-", 0) == 0 && a.size() == 2) {
      // short flags mapping: -p path (keep minimal)
      char c = a[1];
      std::string key = (c == 'p') ? "path" : std::string(1, c);
      if (i + 1 < args.size()) {
        opt[key] = args[++i];
      } else {
        opt[key] = "1";
      }
    } else {
      positionals.push_back(a);
    }
  }
  // alias: -p == --path handled above; also accept --start_date variant
  if (opt.count("start_date") && !opt.count("start-date"))
    opt["start-date"] = opt["start_date"];
  if (opt.count("add_hours") && !opt.count("add-hours"))
    opt["add-hours"] = opt["add_hours"];
  return opt;
}

} // namespace

void print_usage() {
  std::cout
      << "Usage: dometrics <command> [NAME] [options]\n\n"
      << "Project auto-detection: summary, scan, log and daemon resolve the\n"
      << "current working directory (or any parent folder) against registered\n"
      << "projects — no --path needed once initialized. A registered project\n"
      << "name may also be passed directly, e.g. `dometrics summary DDCI`.\n\n"
      << "Commands:\n"
      << "  init [--path <path>] [--name <name>] [--start-date YYYY-MM-DD] [--parent <parent_path>]\n"
      << "  daemon [--start|--stop|--status] [--idle <seconds>] [--foreground]\n"
      << "  log [NAME] --add-hours <float> [--date YYYY-MM-DD] [--note <string>] [--path <path>]\n"
      << "  summary [NAME] [--json|--tui] [--path <path>]\n"
      << "  scan [NAME] [--path <path>]\n\n"
      << "Env:\n"
      << "  DOMETRICS_DB            override database path\n"
      << "  DOMETRICS_IDLE_TIMEOUT  override idle seconds (default 300)\n\n"
      << "Run `dometrics help` for details.\n";
}

void print_help() {
  print_usage();
  std::cout
      << "\nDometrics — Dominion Metrics Engine.\n"
      << "Zero-dependency single-binary dev telemetry: active-time tracking via\n"
      << "filesystem events (inotify), git LOC deltas, multi-repo lineage, and an\n"
      << "embedded SQLite WAL database at ~/.config/dometrics/dometrics.db\n"
      << "(legacy ~/.dometrics/dometrics.db is migrated automatically).\n\n"
      << "Examples:\n"
      << "  dometrics init --name myapp\n"
      << "  dometrics init --path ./legacy --name legacy --start-date 2022-01-15\n"
      << "  dometrics init --path ./next --name next --parent ./legacy\n"
      << "  dometrics daemon --start --idle 300\n"
      << "  dometrics log --add-hours 6 --date 2024-03-01 --note \"offline sprint\"\n"
      << "  dometrics scan              # cwd auto-detected, or: dometrics scan DDCI\n"
      << "  dometrics summary           # cwd auto-detected, or: dometrics summary DDCI\n"
      << "  dometrics summary --json\n";
}

int run(int argc, char** argv) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i)
    args.emplace_back(argv[i]);

  if (args.empty() || args[0] == "--help" || args[0] == "-h" || args[0] == "help") {
    if (!args.empty() && args[0] != "help" && args.size() == 1) {
      print_usage();
      return 0;
    }
    print_help();
    return 0;
  }
  if (args[0] == "--version" || args[0] == "version") {
    std::cout << "dometrics 1.0.0\n";
    return 0;
  }

  std::string cmd = args[0];
  std::vector<std::string> positionals;
  auto opt = parse_opts(args, 1, positionals);

  // NOTE: the `daemon` command must run WITHOUT an open SQLite handle:
  // daemon --start forks, and carrying an open sqlite3 connection across
  // fork corrupts SQLite's WAL last-close tracking — the parent's
  // sqlite3_close() on exit unlinks the -wal/-shm files out from under the
  // daemon child (split-brain: daemon writes vanish). handle_daemon() opens
  // nothing itself; the grandchild opens its own connection in watch_all().
  if (cmd == "daemon") {
    DbClient unused;
    return handle_daemon(unused, positionals, opt);
  }

  DbClient db;
  try {
    db.open(db_path_for_env());
  } catch (const std::exception& e) {
    std::cerr << "cannot open database (" << db_path_for_env() << "): " << e.what()
              << "\n";
    return 1;
  }

  if (cmd == "init")
    return handle_init(db, opt);
  if (cmd == "log")
    return handle_log(db, positionals, opt);
  if (cmd == "summary")
    return handle_summary(db, positionals, opt);
  if (cmd == "scan")
    return handle_scan(db, positionals, opt);

  std::cerr << "unknown command: " << cmd << "\n\n";
  print_usage();
  return 2;
}

} // namespace dometrics::cli
