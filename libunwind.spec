# Define this if you want to skip the strip step and preserve debug info.
# Useful for testing. 
#define __spec_install_post /usr/lib/rpm/brp-compress || :
Summary: An unwinding library for ia64.
Name: libunwind
# Latest libunwind release.
Version: 0.98.5
Release: 2
License: BSD
Group: Development/Debuggers
Source: ftp://ftp.hpl.hp.com/pub/linux-ia64/libunwind-%{version}.tar.gz
Patch1: libunwind-0.98.5-stack-nonexec.patch
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

# Fix-up SELinux compatibility for Bug 201888.
%patch1 -p1 -b .nonexec

%build
%configure
make

%install
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc COPYING README NEWS
%{_mandir}/*/*
/usr/lib/libunwind*
/usr/include/*

%changelog
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

