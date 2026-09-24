#!/usr/bin/env bash
# =============================================================================
# Dometrics — build & setup script
# Builds the single-binary `dometrics` CLI/daemon (C++20 + SQLite WAL).
# Usage:
#   ./build.sh                 # configure + build (Release)
#   ./build.sh --debug         # Debug build
#   ./build.sh --clean         # wipe build/ and rebuild
#   ./build.sh --install       # build + install to ~/.local/bin (or /usr/local/bin with sudo)
#   ./build.sh --check         # build + run smoke tests
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT}/build"
BUILD_TYPE="Release"
CLEAN=0
INSTALL=0
CHECK=0

for arg in "$@"; do
  case "$arg" in
    --debug)   BUILD_TYPE="Debug" ;;
    --clean)   CLEAN=1 ;;
    --install) INSTALL=1 ;;
    --check)   CHECK=1 ;;
    --help|-h)
      sed -n '2,12p' "$0"; exit 0 ;;
    *) echo "Unknown arg: $arg (try --help)" >&2; exit 1 ;;
  esac
done

# --- 1. Dependency checks -----------------------------------------------------
echo "[dometrics] Checking dependencies..."
need() { command -v "$1" >/dev/null 2>&1 || { echo "ERROR: missing '$1'. Install it first." >&2; exit 1; }; }
need cmake
need g++
need sqlite3

# SQLite dev headers: try compiling a tiny probe
if ! echo '#include <sqlite3.h>
int main(){return SQLITE_OK==SQLITE_OK?0:1;}' | g++ -std=c++20 -x c++ - -lsqlite3 -o /tmp/dometrics_sqlite_probe 2>/dev/null; then
  echo "ERROR: SQLite3 dev headers/libs not found." >&2
  echo "  Debian/Ubuntu: sudo apt install libsqlite3-dev" >&2
  echo "  Fedora:        sudo dnf install sqlite-devel" >&2
  echo "  Arch:          sudo pacman -S sqlite" >&2
  echo "  macOS:         brew install sqlite" >&2
  exit 1
fi
rm -f /tmp/dometrics_sqlite_probe

# Require g++ with C++20 <filesystem>
if ! g++ -std=c++20 -E -x c++ - < /dev/null >/dev/null 2>&1; then
  echo "ERROR: g++ does not support -std=c++20. Upgrade GCC (>=10)." >&2
  exit 1
fi

# --- 2. (Re)configure ----------------------------------------------------------
if [[ "$CLEAN" -eq 1 ]]; then
  echo "[dometrics] Cleaning ${BUILD_DIR}..."
  rm -rf "${BUILD_DIR}"
fi
mkdir -p "${BUILD_DIR}"

echo "[dometrics] Configuring (CMAKE_BUILD_TYPE=${BUILD_TYPE})..."
cmake -S "${ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

# --- 3. Build ------------------------------------------------------------------
echo "[dometrics] Building..."
cmake --build "${BUILD_DIR}" --parallel "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

BIN="${BUILD_DIR}/dometrics"
if [[ ! -x "$BIN" ]]; then
  echo "ERROR: build produced no binary at $BIN" >&2
  exit 1
fi
echo "[dometrics] Built: $BIN"
"$BIN" --help | head -25 || true

# --- 4. Optional install --------------------------------------------------------
if [[ "$INSTALL" -eq 1 ]]; then
  DEST="${HOME}/.local/bin/dometrics"
  mkdir -p "$(dirname "$DEST")"
  cp -f "$BIN" "$DEST"
  chmod +x "$DEST"
  echo "[dometrics] Installed to $DEST"
  case ":$PATH:" in
    *":${HOME}/.local/bin:"*) ;;
    *) echo "NOTE: add to PATH: export PATH=\"\$HOME/.local/bin:\$PATH\"" ;;
  esac
  # Optional system-wide install when run as root
  if [[ "$(id -u)" -eq 0 ]]; then
    cp -f "$BIN" /usr/local/bin/dometrics
    echo "[dometrics] Also installed to /usr/local/bin/dometrics"
  fi
fi

# --- 5. Optional smoke test -------------------------------------------------------
if [[ "$CHECK" -eq 1 ]]; then
  echo "[dometrics] Running smoke tests..."
  TMP="$(mktemp -d)"
  trap 'rm -rf "$TMP"' EXIT
  export HOME="$TMP"  # isolate DB to temp HOME
  mkdir -p "$TMP/proj" && cd "$TMP/proj"
  git init -q . 2>/dev/null || true
  echo "int main(){return 0;}" > main.cpp
  git add -A 2>/dev/null || true
  git -c user.email=t@t.t -c user.name=t commit -qm init 2>/dev/null || true
  "$BIN" init --path "$TMP/proj" --name smoke
  "$BIN" scan
  "$BIN" log --add-hours 2.5 --note "smoke test"
  "$BIN" summary
  echo "[dometrics] Smoke tests PASSED."
fi

echo "[dometrics] Done."
