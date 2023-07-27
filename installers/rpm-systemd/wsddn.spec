Name:           wsddn
Version:        1.5
Release:        3%{?dist}
Summary:        WS-Discovery Host Daemon

License:        BSD-3-Clause
URL:            https://github.com/gershnik/wsdd-native
Source0:        https://github.com/gershnik/wsdd-native/archive/refs/tags/v%{version}.zip

BuildRequires:  g++ git make systemd-devel systemd-rpm-macros
Requires:       systemd

Obsoletes:      wsdd
Conflicts:      wsdd

%description
Allows your Linux machine to be discovered by Windows 10 and above systems and displayed by their Explorer "Network" views. 

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
export PATH=%{_topdir}/BUILD/cmake/bin:$PATH
cd %{_topdir}/BUILD/wsdd-native-%{version}
cmake -S . -B out -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build out -- %{?_smp_mflags}
cp installers/wsddn.conf out/
sed -i "s/{RELOAD_INSTRUCTIONS}/ sudo systemctl restart wsddn\n/g" out/wsddn.conf
sed -i "s/{SAMPLE_IFACE_NAME}/eth0/g" out/wsddn.conf


%install
export PATH=%{_topdir}/BUILD/cmake/bin:$PATH
cd %{_topdir}/BUILD/wsdd-native-%{version}
cmake --install out --prefix %{buildroot}/usr
mkdir -p %{buildroot}/usr/lib/systemd/system
install -m 0644 config/systemd/usr/lib/systemd/system/%{name}.service \
                     %{buildroot}/usr/lib/systemd/system/%{name}.service
mkdir -p %{buildroot}/%{_sysconfdir}
install -m 0644 out/wsddn.conf %{buildroot}/%{_sysconfdir}/wsddn.conf
mkdir -p %{buildroot}/usr/share/licenses/wsddn
install -m 0644 LICENSE %{buildroot}/usr/share/licenses/wsddn/LICENSE

%clean
rm -rf $RPM_BUILD_ROOT

%files
/usr/bin/wsddn
%doc /usr/share/man/man8/wsddn.8.gz
/usr/lib/systemd/system/wsddn.service
%config(noreplace) /etc/wsddn.conf
%license /usr/share/licenses/wsddn/LICENSE

%post
%systemd_post wsddn.service
if [ $1 -eq 1 ] ; then 
    # Initial installation 
    firewall-cmd --zone=public --add-port=5357/tcp --permanent > /dev/null 2>&1 || :
    firewall-cmd --zone=public --add-port=3702/udp --permanent > /dev/null 2>&1  || :
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
    firewall-cmd --reload > /dev/null 2>&1 || :
fi

%changelog
* Wed Jul 26 2023 gershnik - 1.5-1
- Release 1.5

* Thu Jul 27 2023 gershnik - 1.5-2
- Now providing separate debuginfo and debugsource packages
- Added LICENSE

* Thu Jul 27 2023 gershnik - 1.5-3
- Fixing broken daemon startup
