Name:           libjournal
Version:        0.3
Release:        1%{?dist}
Summary:        C library for writing to systemd journald without libsystemd

License:        BSD-3-Clause
URL:            https://github.com/darkexplosiveqwx/libjournal
Source0:        %{url}/archive/refs/tags/v%{version}.tar.gz

BuildRequires:  cmake >= 3.21
BuildRequires:  gcc
BuildRequires:  glibc-devel

%description
C library for writing to systemd journald without linking against libsystemd.
Implements the native journal protocol over a Unix datagram socket.
No mmap, no journal file access, no libsystemd dependency -- just the write
path. Thread-safe with auto-reconnect and large message support via memfd.

%package devel
Summary:        Development files for %{name}
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description devel
Header file and pkg-config metadata for developing applications that use
%{name}.

%package static
Summary:        Static library for %{name}
Requires:       %{name}-devel%{?_isa} = %{version}-%{release}

%description static
Static archive of %{name} for statically linked applications.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -DBUILD_TESTS=ON
%cmake_build

%check
%ctest

%install
%cmake_install

%files
%license LICENSE
%doc README.md
%{_libdir}/%{name}.so.0*

%files devel
%{_includedir}/journal.h
%{_libdir}/%{name}.so
%{_libdir}/pkgconfig/%{name}.pc

%files static
%{_libdir}/%{name}.a

%changelog
* Tue Jul 14 2026 darkexplosiveqwx <darkexplosiveqwx@users.noreply.github.com> - 0.1-1
- Initial packaging
