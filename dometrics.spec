# RPM / Copr packaging manifest for Dometrics (source build).
#
# Build with mock/Copr directly from this spec + Source0 tarball:
#   rpmbuild -ba dometrics.spec
#   copr build <project> dometrics.spec --srpm
#
# Name must stay in sync with cmake project() versioning in CMakeLists.txt.
Name:           dometrics
Version:        1.0.0
Release:        1%{?dist}
Summary:        Zero-maintenance developer telemetry CLI and background daemon
License:        MIT
URL:            https://github.com/ardis007UwU/dometrics
# GitHub tag archive; codeload strips the leading "v", yielding dometrics-1.0.0/.
Source0:        %{url}/archive/v%{version}/dometrics-%{version}.tar.gz

BuildRequires:  cmake >= 3.16
BuildRequires:  gcc-c++
BuildRequires:  sqlite-devel
# Runtime: dynamically linked build against system SQLite + glibc.
Requires:       sqlite

%description
Dometrics is a zero-dependency single-binary telemetry tool that tracks active
coding time through inotify filesystem events with a configurable idle timeout,
computes exact git line-of-code deltas, links multi-repo lineages under one
aggregate project, and persists everything in an embedded SQLite WAL database
(~/.config/dometrics/dometrics.db, mode 0600).

%prep
%autosetup

%build
# Release build with the hardened flag set from CMakeLists.txt
# (-fstack-protector-strong, -D_FORTIFY_SOURCE=2, ...).
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=%{_prefix}
cmake --build build --parallel

%install
cmake --install build --prefix "%{buildroot}%{_prefix}"

%files
%license LICENSE
%{_bindir}/dometrics

%changelog
* Wed Sep 24 2026 ardis007UwU <ardis007@users.noreply.github.com> - 1.0.0-1
- Initial RPM/Copr package (upstream v1.0.0)
