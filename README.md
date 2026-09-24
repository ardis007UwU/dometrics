===============================================================================
                                 DOMETRICS
===============================================================================

Dometrics is a zero-maintenance, single-binary C++20 developer telemetry daemon 
and CLI. It tracks active coding time in real time via Linux inotify kernel events 
and indexes lines of code (LOC) into a local SQLite WAL database.

-------------------------------------------------------------------------------
1. INSTALLATION (ALL LINUX DISTROS & MACOS)
-------------------------------------------------------------------------------

[ Universal One-Liner (Fastest) ]
  curl -fsSL https://raw.githubusercontent.com/ardis007UwU/dometrics/main/scripts/install.sh | bash

[ Arch Linux / Manjaro / EndeavourOS (yay / paru) ]
  yay -S dometrics-bin
  # or
  paru -S dometrics-bin

[ Fedora / RHEL / CentOS / Rocky (dnf via Copr) ]
  sudo dnf copr enable ardis007UwU/dometrics
  sudo dnf install dometrics

[ Ubuntu / Debian / Pop!_OS / Linux Mint (apt) ]
  sudo add-apt-repository ppa:ardis007UwU/dometrics
  sudo apt update && sudo apt install dometrics

  # Or direct .deb installation:
  curl -LO https://github.com/ardis007UwU/dometrics/releases/latest/download/dometrics_1.0.0_amd64.deb
  sudo apt install ./dometrics_1.0.0_amd64.deb

[ macOS & Linux Homebrew (brew) ]
  brew install ardis007UwU/tap/dometrics

[ NixOS / Any Linux via Nix (nix) ]
  # Run instantly without installing:
  nix run github:ardis007UwU/dometrics -- summary

  # Install to user profile:
  nix profile install github:ardis007UwU/dometrics

[ Universal AppImage & Flatpak ]
  # AppImage:
  curl -LO https://github.com/ardis007UwU/dometrics/releases/latest/download/dometrics-x86_64.AppImage
  chmod +x dometrics-x86_64.AppImage
  ./dometrics-x86_64.AppImage summary

  # Flatpak:
  flatpak install flathub org.dominion.Dometrics

-------------------------------------------------------------------------------
2. QUICK START
-------------------------------------------------------------------------------

1. Initialize a Project:
   cd /path/to/your/project
   dometrics init

2. Start Background Daemon:
   dometrics daemon --start

3. View Telemetry Summary:
   dometrics summary

4. Scan Repository & Update LOC Snapshot:
   dometrics scan

-------------------------------------------------------------------------------
3. CLI COMMAND REFERENCE
-------------------------------------------------------------------------------

  init      : dometrics init [--name <name>]
              Registers current directory as a tracked project.

  summary   : dometrics summary [PROJECT_NAME]
              Displays the ASCII telemetry dashboard.

  daemon    : dometrics daemon [--start|--stop|--status]
              Manages the background inotify event watcher.

  scan      : dometrics scan [PROJECT_NAME]
              Scans directory tree and commits a new LOC snapshot.

  log       : dometrics log --add-hours <float>
              Adds manual hours to active project telemetry.

-------------------------------------------------------------------------------
4. ENVIRONMENT VARIABLES
-------------------------------------------------------------------------------

  DOMETRICS_DB            : Override default SQLite DB path 
                            (default: ~/.config/dometrics/dometrics.db)
  DOMETRICS_IDLE_TIMEOUT  : Override daemon idle timeout in seconds 
                            (default: 300)

-------------------------------------------------------------------------------
5. BUILDING FROM SOURCE
-------------------------------------------------------------------------------

  git clone https://github.com/ardis007UwU/dometrics.git
  cd dometrics
  cmake -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j$(nproc)
  sudo cmake --install build

===============================================================================
License: MIT
Repository: https://github.com/ardis007UwU/dometrics
===============================================================================
EOF

git add README.txt && git commit -m "docs: add plain text README.txt" && git push origin main
