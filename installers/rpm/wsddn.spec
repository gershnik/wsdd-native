Name:           wsddn
Version:        1.15
Release:        2%{?dist}
Summary:        WS-Discovery Host Daemon

License:        BSD-3-Clause
URL:            https://github.com/gershnik/wsdd-native
Source0:        https://github.com/gershnik/wsdd-native/archive/refs/tags/v%{version}.zip

BuildRequires:  gcc-c++ git make curl unzip systemd-devel systemd-rpm-macros

Requires(post): systemd
Requires(preun): systemd
Requires(postun): systemd

Requires: firewalld-filesystem

Conflicts:      wsdd

%description
Allows your Linux machine to be discovered by Windows 10 and above systems
and displayed by their Explorer "Network" views. 

%debug_package

%prep
if [ ! -d cmake ]; then
    curl -sS -L https://github.com/Kitware/CMake/releases/download/v3.27.1/cmake-3.27.1-linux-%{_arch}.sh -o cmake-3.27.1.sh
    chmod a+x cmake-3.27.1.sh
    mkdir cmake
    ./cmake-3.27.1.sh --prefix=cmake --skip-license
fi
[ -d wsdd-native-%{version} ] || unzip -qq %{_topdir}/SOURCES/v%{version}.zip

%build
export PATH=`pwd`/cmake/bin:$PATH
cd wsdd-native-%{version}
cmake -S . -B out -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build out -- %{?_smp_mflags}
cp installers/wsddn.conf out/
sed -i "s/{RELOAD_INSTRUCTIONS}/# sudo systemctl restart wsddn\n/g" out/wsddn.conf
sed -i "s/{SAMPLE_IFACE_NAME}/eth0/g" out/wsddn.conf


%install
export PATH=`pwd`/cmake/bin:$PATH
cd wsdd-native-%{version}
cmake --install out --prefix %{buildroot}/usr
mkdir -p %{buildroot}/usr/lib/systemd/system
install -m 0644 config/systemd/usr/lib/systemd/system/%{name}.service \
                  %{buildroot}/usr/lib/systemd/system/%{name}.service
mkdir -p %{buildroot}/%{_sysconfdir}
install -m 0644 out/wsddn.conf %{buildroot}/%{_sysconfdir}/wsddn.conf
mkdir -p %{buildroot}/usr/share/licenses/wsddn
install -m 0644 LICENSE %{buildroot}/usr/share/licenses/wsddn/LICENSE
mkdir -p %{buildroot}/usr/lib/firewalld/services
install -m 0644 config/firewalls/etc/firewalld/services/%{name}.xml \
                %{buildroot}/usr/lib/firewalld/services/%{name}.xml
install -m 0644 config/firewalls/etc/firewalld/services/%{name}-http.xml \
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
    firewall-cmd --zone=public --remove-port=5357/tcp --permanent > /dev/null 2>&1 || :
    firewall-cmd --zone=public --remove-port=3702/udp --permanent > /dev/null 2>&1  || :
    firewall-cmd --zone=public --remove-service=wsddn --permanent > /dev/null 2>&1  || :
    firewall-cmd --reload > /dev/null 2>&1 || :
fi

%changelog
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

