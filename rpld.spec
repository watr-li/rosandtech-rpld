## -*- mode: rpm-spec; -*-
##
## $Id: $
##
## rpld.spec.
##

%define debug_package %{nil}

%define _without_docs 1
%{?_without_docs: %define _with_docs 0}


Summary: Router Protocol for Low-Power and Lossy Network (RPL) Daemon.
Name: rpld
Version: 1.00
Release: 001
URL: http://www.rosand-tech.com/downloads/rpld
Source0: %{name}-%{version}.tar.gz
License: GPL
Group: Applications/Communications
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
Router Protocol for Low-Power and Lossy Network (RPL) Daemon.
Low-Power and Lossy Networks (LLNs) are class of network in which both
routers and their interconnect are constrained. RPL provides a
mechanism whereby multipoint-to-point traffic from devices inside
the LLN towards a central control point as well as point-to-multipoint
traffic from the central point to the devices inside the LLNs
are supported.

On Linux this program is called rpld, which stands for RPL Daemon.
This daemon acts as the central control point of the protocol.

%if %{_with_docs}
## The rpld-docs subpackage
%package docs
Summary: Documentation for rpld.
Group: Documentation
%description docs
Documentation for rpld in info, html, postscript and pdf formats.
%endif

%prep
%setup -q

%build

make all

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/usr/share/man/man8

make prefix=$RPM_BUILD_ROOT%{_prefix} \
	sysconfdir=$RPM_BUILD_ROOT/etc \
	mandir=$RPM_BUILD_ROOT%{_mandir} \
	infodir=$RPM_BUILD_ROOT%{_infodir} \
	install

%clean
rm -rf $RPM_BUILD_ROOT

%if %{_with_docs}
%post docs
[ -f %{_infodir}/rpld.info ] && \
	/sbin/install-info %{_infodir}/rpld.info %{_infodir}/dir || :
[ -f %{_infodir}/rpld.info.gz ] && \
	/sbin/install-info %{_infodir}/rpld.info.gz %{_infodir}/dir || :

%preun docs
if [ $1 = 0 ]; then
	[ -f %{_infodir}/rpld.info ] && \
		/sbin/install-info --delete %{_infodir}/rpld.info %{_infodir}/dir || :
	[ -f %{_infodir}/rpld.info.gz ] && \
		/sbin/install-info --delete %{_infodir}/rpld.info.gz %{_infodir}/dir || :
fi
%endif

%files
%defattr(-,root,root)
%{_prefix}/bin/rpld
%{_mandir}/man8/rpld.8.gz

%if %{_with_docs}
%files docs
%doc %{_infodir}/*info*
%doc doc/rpld-html/*.html
%doc doc/rpld.ps
%doc doc/rpld.pdf
%endif

%changelog
* Mon Jul 09 2012 Zafi Ramarosandratana <zramaro@rosand-tech.com>
- Initial build.


