# Maintainer: ardis007UwU <ardis007@users.noreply.github.com>
#
# AUR binary package (`dometrics-bin`): installs the pre-built, fully static
# musl release asset from GitHub Releases — no compilation on the target host.
# Source-build alternative is not provided; upstream ships musl binaries for
# both x86_64 and aarch64.
pkgname=dometrics-bin
pkgver=1.0.0
pkgrel=1
pkgdesc="Zero-maintenance developer telemetry CLI and background daemon (static musl binary)"
arch=('x86_64' 'aarch64')
url="https://github.com/ardis007UwU/dometrics"
license=('MIT')
provides=('dometrics')
conflicts=('dometrics')
# Static musl binaries carry no shared-library dependencies.
depends=()

# Common source: MIT license text shipped in the tagged tree.
# Per-arch sources: raw (non-archive) release binaries, renamed on download.
source=("LICENSE::${url}/raw/v${pkgver}/LICENSE")
source_x86_64=("dometrics::${url}/releases/download/v${pkgver}/dometrics-linux-x86_64")
source_aarch64=("dometrics::${url}/releases/download/v${pkgver}/dometrics-linux-aarch64")

# LICENSE hash is pinned. Binary hashes are refreshed from the release
# SHA256SUMS by CI before AUR upload (SKIP keeps the tree installable pre-pin).
sha256sums=('019073b1efcf424f661089becaac0c5332448b11389b22ec891b9989d7cae2fb')
sha256sums_x86_64=('SKIP')
sha256sums_aarch64=('SKIP')

package() {
  # Single self-contained executable -> standard bin directory, owner-exec.
  install -Dm755 "${srcdir}/dometrics" "${pkgdir}/usr/bin/dometrics"
  install -Dm644 "${srcdir}/LICENSE" "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
}
