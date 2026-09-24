Dometrics

[![Release](https://img.shields.io/github/v/release/ardis007UwU/dometrics?style=flat-square)](https://github.com/ardis007UwU/dometrics/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg?style=flat-square)](LICENSE)
[![CI/CD Release](https://github.com/ardis007UwU/dometrics/actions/workflows/release.yml/badge.svg)](https://github.com/ardis007UwU/dometrics/actions)

**Dometrics** is a zero-maintenance, single-binary C++20 developer telemetry daemon and CLI. It tracks active coding time in real time via Linux `inotify` kernel events and indexes lines of code (LOC) into a local SQLite WAL database—100% offline with zero cloud tracking.

```text
+==============================================================+
|                    D O M E T R I C S                         |
+==============================================================+

  Project        : DDCI
  Path           : /run/media/ardis/AI_DRIVE/(a)Dominion-Studios/DDCI
  ------------------------------------------------------------
  Lifetime active: 234.34h  (234h 20m 35s)
  Total lines    : +47,631 / -410  (net 47,221 LOC)
  Start date     : 2026-09-10
  Commits        : 7
  Last snapshot  : 2026-09-24 14:46:11

+==============================================================+
Installation (All Linux Distros & macOS)1. Universal One-Line Script (Fastest)Automatically detects your OS/architecture, fetches the latest static Musl binary, and installs it to /usr/local/bin:Bashcurl -fsSL [https://raw.githubusercontent.com/ardis007UwU/dometrics/main/scripts/install.sh](https://raw.githubusercontent.com/ardis007UwU/dometrics/main/scripts/install.sh) | bash
2. Arch Linux / Manjaro / EndeavourOS (yay / paru)Install natively from the Arch User Repository (AUR):Bashyay -S dometrics-bin
# or
paru -S dometrics-bin
3. Fedora / RHEL / CentOS / Rocky (dnf via Copr)Enable the Fedora Copr repository and install natively:Bashsudo dnf copr enable ardis007UwU/dometrics
sudo dnf install dometrics
4. Ubuntu / Debian / Pop!_OS / Linux Mint (apt via PPA / .deb)Option A — Launchpad PPA:Bashsudo add-apt-repository ppa:ardis007UwU/dometrics
sudo apt update && sudo apt install dometrics
Option B — Direct .deb Install:Bashcurl -LO [https://github.com/ardis007UwU/dometrics/releases/latest/download/dometrics_1.0.0_amd64.deb](https://github.com/ardis007UwU/dometrics/releases/latest/download/dometrics_1.0.0_amd64.deb)
sudo apt install ./dometrics_1.0.0_amd64.deb
5. macOS & Linux Homebrew (brew)Bashbrew install ardis007UwU/tap/dometrics
6. NixOS / Any Linux via Nix (nix)Run instantly without installing:Bashnix run github:ardis007UwU/dometrics -- summary
Install to user profile:Bashnix profile install github:ardis007UwU/dometrics
7. Universal AppImage & FlatpakAppImage (Zero-install executable):Bashcurl -LO [https://github.com/ardis007UwU/dometrics/releases/latest/download/dometrics-x86_64.AppImage](https://github.com/ardis007UwU/dometrics/releases/latest/download/dometrics-x86_64.AppImage)
chmod +x dometrics-x86_64.AppImage
./dometrics-x86_64.AppImage summary
Flatpak:Bashflatpak install flathub org.dominion.Dometrics
Quick Start1. Register a ProjectNavigate to any project directory and initialize tracking:Bashcd /path/to/your/project
dometrics init
2. Start Background DaemonLaunch the lightweight background daemon (monitors edits silently in the background):Bashdometrics daemon --start
3. View SummaryRun dometrics summary from any project folder or subfolder:Bashdometrics summary
4. Update Line CountsRun a repo scan after major commits or refactors:Bashdometrics scan
CLI ReferenceCommandSyntaxDescriptioninitdometrics init [--name <name>]Registers current directory as a tracked projectsummarydometrics summary [PROJECT_NAME]Displays the minimalist ASCII telemetry dashboarddaemondometrics daemon [--start|--stop|--status]Manages the background inotify event watcherscandometrics scan [PROJECT_NAME]Scans directory tree and commits a new LOC snapshotlogdometrics log --add-hours <float>Adds manual hours to active project telemetry⚙️ Environment VariablesDOMETRICS_DB: Override default SQLite database location (default: ~/.config/dometrics/dometrics.db).DOMETRICS_IDLE_TIMEOUT: Override daemon idle timeout in seconds (default: 300).🔧 Building from SourceDependenciesC++20 compatible compiler (gcc >= 11 or clang >= 13)cmake >= 3.20sqlite3 development headersBuild & Global InstallBashgit clone [https://github.com/ardis007UwU/dometrics.git](https://github.com/ardis007UwU/dometrics.git)
cd dometrics
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
LicenseDistributed under the MIT License.
