# Dometrics (v1.0.0)

A zero-maintenance, single-binary C++20 developer telemetry daemon and CLI.

Dometrics gets you from zero to tracking in under 2 minutes — with real-time `inotify` file system monitoring, a high-performance SQLite WAL database, and zero cloud telemetry.

---

## Quick Start (Universal Installer - Recommended)

The fastest and cleanest way to run Dometrics is via the universal installation script, which detects your OS and architecture, and pulls the fully hardened static Musl binary for you automatically.

### Step 1 - Download and Install

```bash
curl -fsSL [https://raw.githubusercontent.com/ardis007UwU/dometrics/main/scripts/install.sh](https://raw.githubusercontent.com/ardis007UwU/dometrics/main/scripts/install.sh) | bash
```

That's it. The installer:

- auto-detects Linux/macOS and your CPU architecture (`x86_64`, `aarch64`, `arm64`),
- pulls the latest standalone binary directly from GitHub Releases,
- installs it securely to `/usr/local/bin/dometrics`,
- sets the correct executable permissions.

> **Alternative (Package Managers):** Dometrics is also available natively via `yay -S dometrics-bin` (AUR), `brew install ardis007UwU/tap/dometrics` (Homebrew), `sudo dnf install dometrics` (Fedora Copr), and `nix profile install github:ardis007UwU/dometrics`.

### Architecture At a Glance

| Layer | Guarantee |
| --- | --- |
| Storage | Local SQLite WAL database — 100% offline, zero cloud tracking |
| Security | `0600` strict owner-only database file permissions |
| Performance | Sub-0.1% CPU background daemon using Linux `inotify` kernel events |
| Safety | POSIX signal handling (`SIGINT`, `SIGTERM`) for safe WAL flushes |
| Binary | Fully static Musl compilation (no dependency hell) |
| UI | Minimalist ASCII dashboard — no web servers, no electron |

---

## Alternative: Native Build & Prerequisites

If you prefer to compile and run Dometrics natively on your machine without the pre-built binaries, you need a **C++20 compiler**, **CMake**, and **SQLite3**.

Pick your OS below to install the dependencies:

### macOS

```bash
xcode-select --install
brew install cmake sqlite3 pkg-config
```

### Ubuntu / Debian / Pop!_OS

```bash
sudo apt update && sudo apt install -y build-essential cmake libsqlite3-dev pkg-config
```

### Fedora / RHEL / AlmaLinux

```bash
sudo dnf groupinstall "Development Tools" && sudo dnf install -y cmake sqlite-devel pkgconfig
```

### Arch Linux / Manjaro

```bash
sudo pacman -S --needed base-devel cmake sqlite pkgconf
```

### Alpine Linux

```bash
apk add build-base cmake sqlite-dev pkgconf
```

### Windows

The easiest way to run Dometrics natively on Windows is through WSL2 (Windows Subsystem for Linux):

```bash
wsl --install
```

Once inside your WSL Ubuntu terminal, run the Ubuntu/Debian command above.

---

## Step-by-Step Native Guide

### Step 1 - Clone the Repository

```bash
git clone [https://github.com/ardis007UwU/dometrics.git](https://github.com/ardis007UwU/dometrics.git)
cd dometrics
```

### Step 2 - Build Dometrics

**Option A (Recommended - Global Install):**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
```

**Option B (Manual CMake Build for Local Use):**

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Step 3 - Start the Daemon

Navigate to your project directory and start the background tracker:

```bash
cd /path/to/your/project
dometrics init
dometrics daemon --start
```

> **Runtime data lives in `~/.config/dometrics/`.** The database (`dometrics.db`) and its WAL sidecars live here with strict `0600` permissions. The binary resolves this automatically (or use `DOMETRICS_DB=/path` to override).

---

## Useful Commands

### Startup & Tracking Commands

| Command | Description |
| --- | --- |
| `dometrics init` | Registers the current directory as a tracked project |
| `dometrics daemon --start` | Starts the background `inotify` event watcher |
| `dometrics daemon --stop` | Stops the running background daemon |
| `dometrics daemon --status` | Checks if the daemon is currently active |

### Data & Reporting Commands

| Command | Description |
| --- | --- |
| `dometrics summary` | Displays the minimalist ASCII telemetry dashboard |
| `dometrics scan` | Forces a repository snapshot and updates the line count |
| `dometrics log --add-hours <h>` | Adds manual development hours to the active project |

---

## License and Copyright

Copyright (c) 2026 Dominion Studios. All rights reserved.

This project is licensed under the **MIT License**.

**Permitted:** Free to run, inspect, review, modify, distribute, and study for both personal and commercial use, provided the original copyright notice and permission notice are included in all copies or substantial portions of the software.

See the full terms in the [LICENSE](LICENSE) file.
