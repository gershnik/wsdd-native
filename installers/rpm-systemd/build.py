#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import sys
import os
import subprocess
import shutil
from pathlib import Path

RELEASE = '1'
ARCH = subprocess.run(['uname', '-m'], check=True, capture_output=True, encoding="utf-8").stdout.strip()

mydir = Path(sys.argv[0]).parent

sys.path.append(str(mydir.absolute().parent))

from common import VERSION, parseCommandLine, buildCode, installCode, copyTemplated, uploadResults

args = parseCommandLine()
srcdir: Path = args.srcdir
builddir: Path = args.builddir

buildCode(builddir)

workdir = builddir / 'stage/rpm-systemd'
stagedir = workdir / 'stage'
shutil.rmtree(workdir, ignore_errors=True)
stagedir.mkdir(parents=True)

installCode(builddir, stagedir / f'wsddn-{VERSION}/usr')

shutil.copytree(srcdir / 'config/systemd/usr', stagedir / f'wsddn-{VERSION}/usr', dirs_exist_ok=True)

copyTemplated(mydir.parent / 'wsddn.conf', stagedir / f'wsddn-{VERSION}/etc/wsddn.conf', {
    'SAMPLE_IFACE_NAME': "eth0",
    'RELOAD_INSTRUCTIONS': f"""
# sudo systemctl restart wsddn
""".lstrip()
})

rpmbuild = workdir / 'rpmbuild'
sources = rpmbuild / 'SOURCES'
sources.mkdir(parents=True)

subprocess.run(['tar', '-cvzf', sources / 'wsddn.tar.gz', '-C', stagedir, '--owner=0', '--group=0', f'wsddn-{VERSION}'], check=True)


specs = rpmbuild / 'SPECS'
specs.mkdir(parents=True)
spec = specs / 'wsddn.spec'
spec.write_text(f"""
Name:           wsddn
Version:        {VERSION}
Release:        {RELEASE}%{{?dist}}
Summary:        WS-Discovery Host Daemon

License:        BSD-3-Clause
URL:            https://github.com/gershnik/wsdd-native
Source0:        %{{name}}.tar.gz

BuildRequires:  systemd-rpm-macros
Requires:       systemd

Obsoletes:      wsdd
Conflicts:      wsdd

%description
Allows your Linux machine to be discovered by Windows 10 and above systems and displayed by their Explorer "Network" views. 

%prep
%autosetup

%install
rm -rf $RPM_BUILD_ROOT
cp -r . $RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
/usr/bin/wsddn
%doc /usr/share/man/man8/wsddn.8.gz
/usr/lib/systemd/system/wsddn.service
%config(noreplace) /etc/wsddn.conf

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

""".lstrip())

subprocess.run(['rpmbuild', '--define', f'_topdir {rpmbuild.absolute()}', '-bb', spec], check=True)

subprocess.run(['gzip', '--keep', '--force', builddir / 'wsddn'], check=True)

rpm = list((rpmbuild / f'RPMS/{ARCH}').glob('*.rpm'))[0]

shutil.move(builddir / 'wsddn.gz', workdir / f'wsddn-rpm-systemd-{VERSION}.gz')

if args.sign:
    (Path.home() / '.rpmmacros').write_text(f"""
    %_signature gpg
    %_gpg_name {os.environ['PGP_KEY_NAME']}
    %__gpg_sign_cmd %{{__gpg}} gpg --force-v3-sigs --batch --pinentry-mode=loopback --verbose --no-armor --passphrase {os.environ['PGP_KEY_PASSWD']} -u "%{{_gpg_name}}" -sbo %{{__signature_filename}} --digest-algo sha256 %{{__plaintext_filename}}
    """.lstrip())

    subprocess.run(['rpm', '--addsign', rpm], check=True)



if args.uploadResults:
    repo = workdir / 'rpm-repo'
    subprocess.run(['aws', 's3', 'sync', 's3://gershnik.com/rpm-repo', repo], check=True)
    repo.mkdir(parents=True, exist_ok=True)
    shutil.copy(rpm, repo)
    subprocess.run(['createrepo', '--update', '.'], cwd=repo, check=True)
    (repo / 'repodata/repomd.xml.asc').unlink(missing_ok=True)
    subprocess.run(['gpg', '--batch', '--pinentry-mode=loopback', 
                    '--detach-sign', '--armor', 
                    '--default-key', os.environ['PGP_KEY_NAME'],
                    '--passphrase', os.environ['PGP_KEY_PASSWD'],
                    repo / 'repodata/repomd.xml'], check=True)

    (repo / 'gershnik.repo').write_text("""
[gershnik-repo]
name=github.com/gershnik repository
baseurl=https://www.gershnik.com/rpm-repo
enabled=1
gpgcheck=1
gpgkey=https://www.gershnik.com/rpm-repo/pgp-key.public
""".lstrip())
    subprocess.run(['aws', 's3', 'sync', repo, 's3://gershnik.com/rpm-repo'], check=True)
    uploadResults(rpm, workdir / f'wsddn-rpm-systemd-{VERSION}.gz')
