#!/usr/bin/env bash
# Dometrics one-line installer: detects OS/arch, fetches the release binary,
# verifies its SHA256, and installs it to /usr/local/bin with mode 0755.
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/ardis007UwU/dometrics/main/scripts/install.sh | bash
set -euo pipefail

REPO="ardis007UwU/dometrics"
RELEASE_URL="https://github.com/${REPO}/releases/latest/download"
DEST_DIR="/usr/local/bin"
DEST_BIN="${DEST_DIR}/dometrics"

log()  { printf '[dometrics] %s\n' "$*"; }
die()  { printf '[dometrics] error: %s\n' "$*" >&2; exit 1; }

# --- Architecture / OS detection --------------------------------------------
detect_os() {
  local raw
  raw="$(uname -s | tr '[:upper:]' '[:lower:]')"
  case "$raw" in
    linux)  printf 'linux' ;;
    darwin) printf 'darwin' ;;
    *) die "unsupported OS: $(uname -s) (supported: Linux, macOS)" ;;
  esac
}

detect_arch() {
  local os="$1" raw
  raw="$(uname -m)"
  case "$raw" in
    x86_64|amd64) printf 'x86_64' ;;
    aarch64|arm64)
      if [ "$os" = "darwin" ]; then printf 'arm64'; else printf 'aarch64'; fi
      ;;
    *) die "unsupported architecture: $raw (supported: x86_64, aarch64/arm64)" ;;
  esac
}

# --- Download helper (curl preferred, wget fallback) -------------------------
download() {
  local url="$1" out="$2"
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL --retry 3 --retry-delay 2 -o "$out" "$url"
  elif command -v wget >/dev/null 2>&1; then
    wget -q -O "$out" "$url"
  else
    die "neither curl nor wget found; install one and retry"
  fi
}

sha256_of() {
  local file="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$file" | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$file" | awk '{print $1}'
  elif command -v openssl >/dev/null 2>&1; then
    openssl dgst -sha256 "$file" | awk '{print $NF}'
  else
    printf ''
  fi
}

# --- Main --------------------------------------------------------------------
main() {
  local os arch asset tmpdir
  os="$(detect_os)"
  arch="$(detect_arch "$os")"
  asset="dometrics-${os}-${arch}"
  log "detected ${os}/${arch} -> ${asset}"

  tmpdir="$(mktemp -d)"
  trap 'rm -rf "$tmpdir"' EXIT

  log "downloading ${RELEASE_URL}/${asset}"
  download "${RELEASE_URL}/${asset}" "${tmpdir}/dometrics" \
    || die "download failed: ${RELEASE_URL}/${asset}"

  # Verify against SHA256SUMS when published (fail closed on mismatch).
  if download "${RELEASE_URL}/SHA256SUMS" "${tmpdir}/SHA256SUMS" 2>/dev/null; then
    local expected actual
    expected="$(awk -v f="$asset" '$2 == f {print $1}' "${tmpdir}/SHA256SUMS")"
    actual="$(sha256_of "${tmpdir}/dometrics")"
    if [ -n "$expected" ] && [ -n "$actual" ]; then
      if [ "$expected" != "$actual" ]; then
        die "checksum mismatch for ${asset} (expected ${expected}, got ${actual})"
      fi
      log "sha256 verified: ${actual}"
    else
      log "warning: ${asset} not listed in SHA256SUMS; skipping verification"
    fi
  else
    log "warning: SHA256SUMS unavailable; skipping checksum verification"
  fi

  chmod 0755 "${tmpdir}/dometrics"
  "${tmpdir}/dometrics" --version >/dev/null 2>&1 \
    || die "downloaded binary failed self-check (--version)"

  if [ ! -d "$DEST_DIR" ]; then
    mkdir -p "$DEST_DIR" 2>/dev/null || sudo mkdir -p "$DEST_DIR"
  fi
  if [ -w "$DEST_DIR" ]; then
    install -m 0755 "${tmpdir}/dometrics" "$DEST_BIN"
  elif command -v sudo >/dev/null 2>&1; then
    log "installing to ${DEST_BIN} (sudo)"
    sudo install -m 0755 "${tmpdir}/dometrics" "$DEST_BIN"
  else
    die "no write permission for ${DEST_DIR} and sudo unavailable"
  fi

  log "installed: $("$DEST_BIN" --version)"
  case ":${PATH}:" in
    *":${DEST_DIR}:"*) ;;
    *) log "note: add ${DEST_DIR} to PATH: export PATH=\"${DEST_DIR}:\$PATH\"" ;;
  esac
}

main "$@"
