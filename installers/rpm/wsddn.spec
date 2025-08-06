Name:           wsddn
Version:        1.21
Release:        3%{?dist}
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

BuildRequires:  gcc-c++ git cmake >= 3.25 make coreutils curl unzip bsdtar systemd-devel systemd-rpm-macros 

Requires(post): systemd
Requires(preun): systemd
Requires(postun): systemd

%if 0%{?suse_version} == 0
Requires: firewalld-filesystem
%endif

Conflicts:      wsdd

%description
Allows your Linux machine to be discovered by Windows 10 and above systems
and displayed by their Explorer "Network" views.

%global _vpath_srcdir wsddn
%global _vpath_builddir wsddn/out

#OpenSUSE
%if 0%{?suse_version}
%global __sourcedir %{_vpath_srcdir}
%global __builddir %{_vpath_builddir}
%endif

%debug_package

%prep
_unpack_source() {
    local name=$1
    local tgz=$2

    [ -d $name ] && rm -rf $name; mkdir -p $name
    bsdtar -C $name --strip-components=1 -xzf "%{_topdir}/SOURCES/$tgz"
}

_unpack_source wsddn wsddn.tgz
_unpack_source wsddn/external/argum argum.tgz
_unpack_source wsddn/external/asio asio.tgz
_unpack_source wsddn/external/fmt fmt.tgz
_unpack_source wsddn/external/isptr isptr.tgz
_unpack_source wsddn/external/libxml2 libxml2.tgz
_unpack_source wsddn/external/modern-uuid modern-uuid.tgz
_unpack_source wsddn/external/outcome outcome.tgz
_unpack_source wsddn/external/ptl ptl.tgz
_unpack_source wsddn/external/spdlog spdlog.tgz
_unpack_source wsddn/external/sys_string sys_string.tgz
_unpack_source wsddn/external/tomlplusplus tomlplusplus.tgz

%build

#on some platforms cmake macros mess cwd
mydir=`pwd`
pushd $mydir
%cmake -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
    "-DFETCHCONTENT_SOURCE_DIR_ARGUM=$mydir/wsddn/external/argum" \
    "-DFETCHCONTENT_SOURCE_DIR_ASIO=$mydir/wsddn/external/asio" \
    "-DFETCHCONTENT_SOURCE_DIR_FMT=$mydir/wsddn/external/fmt" \
    "-DFETCHCONTENT_SOURCE_DIR_ISPTR=$mydir/wsddn/external/isptr" \
    "-DFETCHCONTENT_SOURCE_DIR_LIBXML2=$mydir/wsddn/external/libxml2" \
    "-DFETCHCONTENT_SOURCE_DIR_MODERN-UUID=$mydir/wsddn/external/modern-uuid" \
    "-DFETCHCONTENT_SOURCE_DIR_OUTCOME=$mydir/wsddn/external/outcome" \
    "-DFETCHCONTENT_SOURCE_DIR_PTL=$mydir/wsddn/external/ptl" \
    "-DFETCHCONTENT_SOURCE_DIR_SPDLOG=$mydir/wsddn/external/spdlog" \
    "-DFETCHCONTENT_SOURCE_DIR_SYS_STRING=$mydir/wsddn/external/sys_string" \
    "-DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS=$mydir/wsddn/external/tomlplusplus"
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

%triggerin -- firewalld
if [ $1 -eq 1 ] && [ $2 -eq 1 ] ; then
    # Initial installation
    if [ -x %{_bindir}/firewall-cmd ]; then
        %{_bindir}/firewall-cmd --reload > /dev/null 2>&1 || :
        %{_bindir}/firewall-cmd --zone=public --add-service=wsddn --permanent > /dev/null 2>&1 || :
        %{_bindir}/firewall-cmd --reload > /dev/null 2>&1 || :
    fi
fi

%preun
%systemd_preun wsddn.service

%postun
%systemd_postun_with_restart wsddn.service
if [ $1 -eq 0 ] ; then
    # Package removal, not upgrade
    if [ -x %{_bindir}/firewall-cmd ]; then
        # Older versions opened ports explicitly without service
        %{_bindir}/firewall-cmd --zone=public --remove-port=5357/tcp --permanent > /dev/null 2>&1 || :
        %{_bindir}/firewall-cmd --zone=public --remove-port=3702/udp --permanent > /dev/null 2>&1  || :
        %{_bindir}/firewall-cmd --zone=public --remove-service=wsddn --permanent > /dev/null 2>&1  || :
        %{_bindir}/firewall-cmd --reload > /dev/null 2>&1 || :
    fi
fi

%changelog
* Mon Aug 04 2025 gershnik - 1.21-2
- RPM package portability fixes

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

