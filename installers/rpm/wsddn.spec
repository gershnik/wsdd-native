Name:           wsddn
Version:        1.21
Release:        1%{?dist}
Summary:        WS-Discovery Host Daemon

License:        BSD-3-Clause
URL:            https://github.com/gershnik/wsdd-native

Source:         https://github.com/gershnik/wsdd-native/tarball/v%{version}#/wsddn.tgz
Source:         https://github.com/gershnik/argum/tarball/v2.6#/argum.tgz
Source:         https://downloads.sourceforge.net/asio/asio-1.30.2.tar.gz#/asio.tgz
Source:         https://github.com/fmtlib/fmt/tarball/11.2.0#/fmt.tgz
Source:         https://github.com/gershnik/intrusive_shared_ptr/tarball/v1.9#/isptr.tgz
Source:         https://gitlab.gnome.org/GNOME/libxml2/-/archive/v2.14.5/libxml2-v2.14.5.tar.gz#/libxml2.tgz
Source:         https://github.com/gershnik/modern-uuid/tarball/v1.8#/modern-uuid.tgz
Source:         https://github.com/ned14/outcome/tarball/v2.2.12#/outcome.tgz
Source:         https://github.com/gershnik/ptl/tarball/v1.7#/ptl.tgz
Source:         https://github.com/gabime/spdlog/tarball/v1.15.3#/spdlog.tgz
Source:         https://github.com/gershnik/sys_string/tarball/v2.20#/sys_string.tgz
Source:         https://github.com/marzer/tomlplusplus/tarball/v3.4.0#/tomlplusplus.tgz

BuildRequires:  gcc-c++ git cmake >= 3.25 make coreutils curl unzip systemd-devel systemd-rpm-macros 

Requires(post): systemd
Requires(preun): systemd
Requires(postun): systemd

Requires: firewalld-filesystem

Conflicts:      wsdd

%description
Allows your Linux machine to be discovered by Windows 10 and above systems
and displayed by their Explorer "Network" views.

%global deps argum asio fmt isptr libxml2 modern-uuid outcome ptl spdlog sys_string tomlplusplus
%global _vpath_srcdir wsddn
%global _vpath_builddir wsddn/out

#OpenSUSE
%if 0%{?suse_version}
%global __sourcedir %{_vpath_srcdir}
%global __builddir %{_vpath_builddir}
%endif

%debug_package

%prep
for comp in wsddn %{deps}
do
    [ -d $comp ] || (mkdir $comp && tar -C $comp --strip-components=1 --warning=no-unknown-keyword -xzf %{_topdir}/SOURCES/$comp.tgz)
done

%build

fetch_sources='-DFETCHCONTENT_FULLY_DISCONNECTED=ON'
for comp in %{deps}
do
    upcomp=$(echo $comp | tr 'a-z' 'A-Z')
    fetch_sources+=" -DFETCHCONTENT_SOURCE_DIR_$upcomp=`pwd`/$comp"
done

#on some platforms cmake macros mess cwd
pushd `pwd`
%cmake $fetch_sources
%cmake_build
popd

cp %{_vpath_srcdir}/installers/wsddn.conf %{_vpath_builddir}/
sed -i "s/{RELOAD_INSTRUCTIONS}/# sudo systemctl restart wsddn\n/g" %{_vpath_builddir}/wsddn.conf
sed -i "s/{SAMPLE_IFACE_NAME}/eth0/g" %{_vpath_builddir}/wsddn.conf


%install

%cmake_install

mkdir -p %{buildroot}/usr/lib/systemd/system
install -m 0644 %{_vpath_srcdir}/config/systemd/usr/lib/systemd/system/%{name}.service \
                %{buildroot}/usr/lib/systemd/system/%{name}.service
mkdir -p %{buildroot}/%{_sysconfdir}
install -m 0644 %{_vpath_builddir}/wsddn.conf %{buildroot}/%{_sysconfdir}/wsddn.conf
mkdir -p %{buildroot}/usr/share/licenses/wsddn
install -m 0644 %{_vpath_srcdir}/LICENSE %{buildroot}/usr/share/licenses/wsddn/LICENSE
mkdir -p %{buildroot}/usr/lib/firewalld/services
install -m 0644 %{_vpath_srcdir}/config/firewalls/etc/firewalld/services/%{name}.xml \
                %{buildroot}/usr/lib/firewalld/services/%{name}.xml
install -m 0644 %{_vpath_srcdir}/config/firewalls/etc/firewalld/services/%{name}-http.xml \
                %{buildroot}/usr/lib/firewalld/services/%{name}-http.xml

%files
/usr/bin/wsddn
%doc /usr/share/man/man8/wsddn.8.gz
/usr/lib/systemd/system/wsddn.service
/usr/lib/firewalld/services/wsddn.xml
/usr/lib/firewalld/services/wsddn-http.xml
%config(noreplace) /etc/wsddn.conf
%dir /usr/share/licenses/wsddn
%license /usr/share/licenses/wsddn/LICENSE

%post
%systemd_post wsddn.service
if [ $1 -eq 1 ] ; then
    # Initial installation
    firewall-cmd --reload > /dev/null 2>&1 || :
    firewall-cmd --zone=public --add-service=wsddn --permanent > /dev/null 2>&1 || :
    firewall-cmd --reload > /dev/null 2>&1 || :
fi

%preun
%systemd_preun wsddn.service

%postun
%systemd_postun_with_restart wsddn.service
if [ $1 -eq 0 ] ; then
    # Package removal, not upgrade
    # Older versions opened ports explicitly without service
    firewall-cmd --zone=public --remove-port=5357/tcp --permanent > /dev/null 2>&1 || :
    firewall-cmd --zone=public --remove-port=3702/udp --permanent > /dev/null 2>&1  || :
    firewall-cmd --zone=public --remove-service=wsddn --permanent > /dev/null 2>&1  || :
    firewall-cmd --reload > /dev/null 2>&1 || :
fi

%changelog
* Mon Aug 04 2025 gershnik - 1.21-1
- Release 1.21

* Sat Jul 19 2025 gershnik - 1.20-1
- Release 1.20

* Sat Jun 28 2025 gershnik - 1.19-1
- Release 1.19

* Thu Mar 27 2025 gershnik - 1.18-1
- Release 1.18

* Sun Mar 09 2025 gershnik - 1.17-1
- Release 1.17

* Sun Jan 12 2025 gershnik - 1.16-1
- Release 1.16

* Thu Oct 03 2024 gershnik - 1.15-1
- Release 1.15

* Wed May 29 2024 gershnik - 1.14-1
- Release 1.14

* Thu May 16 2024 gershnik - 1.13-1
- Release 1.13

* Mon May 06 2024 gershnik - 1.12-1
- Release 1.12

* Tue Apr 30 2024 gershnik - 1.10-1
- Release 1.10

* Sat Apr 20 2024 gershnik - 1.9-1
- Release 1.9

* Tue Oct 17 2023 gershnik - 1.8-1
- Release 1.8

* Fri Sep 22 2023 gershnik - 1.7-1
- Release 1.7

* Sat Jul 29 2023 gershnik - 1.6-1
- Release 1.6

* Thu Jul 27 2023 gershnik - 1.5-4
- Fixing changelog

* Thu Jul 27 2023 gershnik - 1.5-3
- Fixing broken daemon startup

* Thu Jul 27 2023 gershnik - 1.5-2
- Now providing separate debuginfo and debugsource packages
- Added LICENSE

* Wed Jul 26 2023 gershnik - 1.5-1
- Release 1.5

