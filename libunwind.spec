# Define this if you want to skip the strip step and preserve debug info.
# Useful for testing. 
#define __spec_install_post /usr/lib/rpm/brp-compress || :
Summary: An unwinding library for ia64.
Name: libunwind
# Latest libunwind release.
Version: 0.92
Release: 1
License: BSD
Group: Development/Debuggers
Source: ftp://ftp.hpl.hp.com/pub/linux-ia64/libunwind-0.92.tar.gz
Buildroot: %{_tmppath}/%{name}-%{version}-root
URL: http://www.hpl.hp.com/research/linux/libunwind/index.php4/
ExclusiveArch: ia64

BuildRequires: glibc gcc make tar gzip
Prereq: info

%description
Libunwind provides a C ABI to determine the call-chain of a program.
This version of libunwind is targetted for the ia64 platform.

%prep
%setup

%build
%configure
make

%install
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%post
%preun

%files
%defattr(-,root,root)
%doc COPYING README NEWS
%{_mandir}/*/*
/usr/lib/*
/usr/include/*

%changelog
* Mon Oct 06 2003  Jeff Johnston <jjohnstn@redhat.com>	0.92.1
- Initial release

