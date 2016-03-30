%define booking_repo base

Name:				  i7z_short
Summary:			Show actual processor clock

Version:			0.2
Release:			1%{?dist}

Group:				Applications/Internet
License:			BSD with advertising
URL:				  http://code.google.com/p/i7z

Source0:			%{name}-%{version}.tar.gz
#Source0:     https://github.com/teunis90/%{name}/archive/%{version}.tar.gz#/%{name}-%{version}.tar.gz

BuildRoot:    %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
Show actual processor clock

%prep

%setup -q

%build
make

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT BINDIR_INSTALL=%{_sbindir}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%define _unpackaged_files_terminate_build 0
%{_sbindir}/i7z

%changelog
* Wed Mar 30 2016 Teun Ouwehand <teun.ouwehand@booking.com> 0.2
- Package i7z-short from https://github.com/teunis90/i7z-short
* Tue Mar 29 2016 Teun Ouwehand <teun.ouwehand@booking.com> 0.1
- Package i7z-short from https://github.com/teunis90/i7z-short
* Tue Mar 22 2016 Teun Ouwehand <teun.ouwehand@booking.com> 0.1
- Package i7z from https://github.com/ajaiantilal/i7z

