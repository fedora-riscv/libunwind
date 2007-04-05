# Define this if you want to skip the strip step and preserve debug info.
# Useful for testing. 
#define __spec_install_post /usr/lib/rpm/brp-compress || :
Summary: An unwinding library
Name: libunwind
# Latest libunwind release.
Version: 0.99
%define frysksnap 20070405cvs
%define upstreamsnap 070224
Release: 0.1.frysk%{frysksnap}%{?dist}
License: BSD
Group: Development/Debuggers
Source: http://download.savannah.nongnu.org/releases/libunwind/libunwind-snap-%{upstreamsnap}.tar.gz
Patch1: libunwind-snap-%{upstreamsnap}-frysk%{frysksnap}.patch
Buildroot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
URL: http://savannah.nongnu.org/projects/libunwind
ExclusiveArch: ia64 x86_64 i386 ppc64

BuildRequires: glibc gcc make tar gzip
BuildRequires: automake libtool autoconf

%description
Libunwind provides a C ABI to determine the call-chain of a program.
This version of libunwind is targetted for the ia64 platform.

%package devel
Summary: Development package for libunwind
Group: Development/Debuggers
Requires: libunwind = %{PACKAGE_VERSION}
%description devel
The libunwind-devel package includes the libraries and header files for
libunwind.

%prep
%setup -q -n %{name}-%{version}-alpha

%patch1 -p1 -E
# New files from Patch1:
chmod +x tests/run-ptrace-stepper
chmod +x tests/run-ptrace-signull

%build
mkdir -p config
aclocal
libtoolize
autoheader
automake --add-missing
autoconf
%configure --disable-static --enable-shared
make

%install
%makeinstall
rm -f $RPM_BUILD_ROOT/%{_libdir}/libunwind*.la

%check
make check || true

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%doc COPYING README NEWS
%{_libdir}/libunwind*.so.*

%files devel
%defattr(-,root,root)
%{_libdir}/libunwind*.so
%{_mandir}/*/*
%{_includedir}/*

%changelog
* Thu Apr  5 2007 Jan Kratochvil <jan.kratochvil@redhat.com> - 0.99-0.1.frysk20070405cvs
- Update to the upstream snapshot snap-070224.
- Use the Frysk's modified version, currently snapshot 20070405cvs.
- Extend the supported architectures from ia64 also to x86_64, i386 and ppc64.
- Spec file fixups.
- Split the package to its base and the `devel' part.
- Drop the statically built libraries.

* Sun Oct 01 2006 Jesse Keating <jkeating@redhat.com> - 0.98.5-3
- rebuilt for unwind info generation, broken in gcc-4.1.1-21

* Sat Sep 22 2006 Jan Kratochvil <jan.kratochvil@redhat.com> - 0.98.5-2
- SELinux compatibility fix - stack is now non-exec (Jakub Jelinek suggestion).

* Wed Jul 12 2006 Jesse Keating <jkeating@redhat.com> - 0.98.5-1.1
- rebuild

* Sat May 27 2006 Alexandre Oliva <aoliva@redhat.com> - 0.98.5-1
- Import version 0.98.5.

* Thu Feb 09 2006 Florian La Roche <laroche@redhat.com>
- remove empty scripts

* Tue Feb 07 2006 Jesse Keating <jkeating@redhat.com> - 0.98.2-3.2
- rebuilt for new gcc4.1 snapshot and glibc changes

* Fri Dec 09 2005 Jesse Keating <jkeating@redhat.com>
- rebuilt

* Tue Mar 01 2005 Jeff Johnston <jjohnstn@redhat.com>	0.98.2.3
- Bump up release number

* Thu Nov 11 2004 Jeff Johnston <jjohnstn@redhat.com>	0.98.2.2
- Import version 0.98.2.

* Wed Nov 10 2004 Jeff Johnston <jjohnstn@redhat.com>	0.97.6
- Bump up release number

* Thu Aug 19 2004 Jeff Johnston <jjohnstn@redhat.com>	0.97.3
- Remove debug file from files list.

* Fri Aug 13 2004 Jeff Johnston <jjohnstn@redhat.com>	0.97.2
- Import version 0.97.

* Tue Jun 15 2004 Elliot Lee <sopwith@redhat.com>
- rebuilt

* Wed Jun 09 2004  Elena Zannoni <ezannoni@redhat.com>	0.96.4
- Bump release number.

* Mon Feb 23 2004  Elena Zannoni <ezannoni@redhat.com>	0.96.3
- Bump release number.

* Fri Feb 13 2004 Elliot Lee <sopwith@redhat.com>
- rebuilt

* Thu Jan 29 2004  Jeff Johnston <jjohnstn@redhat.com>	0.96.1
- Import version 0.96.

* Tue Jan 06 2004  Jeff Johnston <jjohnstn@redhat.com>	0.92.2
- Bump release number.

* Mon Oct 06 2003  Jeff Johnston <jjohnstn@redhat.com>	0.92.1
- Initial release

