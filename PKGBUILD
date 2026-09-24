# Maintainer: Dominion Studios <dev@dominion-studios.example>
pkgname=dometrics
pkgver=1.0.0
pkgrel=1
pkgdesc="Zero-maintenance developer telemetry CLI and background daemon"
arch=('x86_64' 'aarch64')
url="https://github.com/Dominion-Studios/dometrics"
license=('MIT')
depends=('glibc' 'sqlite')
makedepends=('cmake' 'gcc')
source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('62e2101beb5435ea5b59acc115fd5a8baa95492b4f47cbde7d7b4c3824c01648')
options=('!lto')

build() {
  cmake -S "$pkgname-$pkgver" -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
  cmake --build build --parallel
}

package() {
  install -Dm755 build/dometrics "$pkgdir/usr/bin/dometrics"
  install -Dm644 "$pkgname-$pkgver/LICENSE" \
    "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
