#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import sys
import os
import subprocess
import shutil
from pathlib import Path

RELEASE = '1'
ARCH = subprocess.run(['dpkg-architecture', '-q', 'DEB_HOST_ARCH'], check=True, capture_output=True, encoding="utf-8").stdout.strip()

mydir = Path(sys.argv[0]).parent

sys.path.append(str(mydir.absolute().parent))

from common import VERSION, parseCommandLine, buildCode, installCode, copyTemplated, uploadResults

args = parseCommandLine()
srcdir: Path = args.srcdir
builddir: Path = args.builddir

buildCode(builddir)

workdir = builddir / 'stage/deb-systemd'
stagedir = workdir / f'wsddn_{VERSION}-{RELEASE}_{ARCH}'
shutil.rmtree(workdir, ignore_errors=True)
stagedir.mkdir(parents=True)

installCode(builddir, stagedir / 'usr')

shutil.copytree(srcdir / 'config/systemd/usr', stagedir / 'usr', dirs_exist_ok=True)

docdir = stagedir / 'usr/share/doc/wsddn'
docdir.mkdir(parents=True)
shutil.copy(mydir / 'copyright', docdir / 'copyright')

copyTemplated(mydir.parent / 'wsddn.conf', stagedir / "etc/wsddn.conf", {
    'SAMPLE_IFACE_NAME': "eth0",
    'RELOAD_INSTRUCTIONS': f"""
# sudo systemctl restart wsddn
""".lstrip()
})

debiandir = stagedir/ 'DEBIAN'
debiandir.mkdir()

(stagedir/ 'debian').symlink_to(debiandir.absolute())

control = debiandir / 'control'

control.write_text(
f"""
Package: wsddn
Source: wsddn
Version: {VERSION}
Architecture: {ARCH}
Depends: systemd, {{shlibs_Depends}}
Conflicts: wsdd
Replaces: wsdd
Maintainer: Eugene Gershnik <gershnik@hotmail.com>
Homepage: https://github.com/gershnik/wsdd-native
Description: WS-Discovery Host Daemon
 Allows your Linux machine to be discovered by Windows 10 and above systems and displayed by their Explorer "Network" views. 

""".lstrip())

#shutil.copy(mydir / 'pre',          debiandir / 'preinst')
shutil.copy(mydir / 'prerm',        debiandir / 'prerm')
shutil.copy(mydir / 'postinst',     debiandir / 'postinst')
shutil.copy(mydir / 'postrm',       debiandir / 'postrm')
shutil.copy(mydir / 'copyright',    debiandir / 'copyright')

(debiandir / 'conffiles').write_text("""
/etc/wsddn.conf
""".lstrip())

deps = subprocess.run(['dpkg-shlibdeps', '-O', '-eusr/bin/wsddn'], check=True, cwd=stagedir, capture_output=True, encoding="utf-8").stdout.strip()
key, val = deps.split('=', 1)
key = key.replace(':', "_")
control.write_text(control.read_text().format_map({key: val}))
subprocess.run(['dpkg-deb', '--build', '--root-owner-group', stagedir, workdir], check=True)

subprocess.run(['gzip', '--keep', '--force', builddir / 'wsddn'], check=True)

deb = list(workdir.glob('*.deb'))[0]

shutil.move(builddir / 'wsddn.gz', workdir / f'wsddn-deb-systemd-{VERSION}.gz')

if args.uploadResults:
    repo = builddir / 'apt-repo'
    subprocess.run(['aws', 's3', 'sync', 's3://gershnik.com/apt-repo', repo], check=True)
    (repo / 'pool/main').mkdir(parents=True, exist_ok=True)
    (repo / f'dists/stable/main/binary-{ARCH}').mkdir(parents=True, exist_ok=True)
    shutil.copy(deb, repo / 'pool/main')
    with open(repo / f'dists/stable/main/binary-{ARCH}/Packages', "w") as packages:
        subprocess.run(['apt-ftparchive', '--arch', ARCH, 'packages', 'pool'], stdout=packages, cwd=repo, check=True)
    subprocess.run(['gzip', '--keep', '--force', repo / f'dists/stable/main/binary-{ARCH}/Packages'], check=True)
    (repo / f'dists/stable/Release').unlink(missing_ok=True)
    relaseContent = subprocess.run(['apt-ftparchive', '--arch', ARCH, 'release', repo / 'dists/stable'], capture_output=True, encoding='utf-8', check=True).stdout
    (repo / f'dists/stable/Release').write_text(f"""
Origin: github.com/gershnik repository
Label: github.com/gershnik
Suite: stable
Codename: stable
Version: 1.0
Architectures: amd64
Components: main
Description: Software repository for github.com/gershnik
{relaseContent}
""".lstrip())
        
    (repo / f'dists/stable/Release.pgp').unlink(missing_ok=True)
    subprocess.run(['gpg', '--batch', '--pinentry-mode=loopback',
                    '--default-key', os.environ['PGP_KEY_NAME'], '-abs',  
                    '--passphrase', os.environ['PGP_KEY_PASSWD'],
                    '-o', repo / f'dists/stable/Release.pgp', repo / f'dists/stable/Release'], check=True)
    (repo / f'dists/stable/InRelease').unlink(missing_ok=True)
    subprocess.run(['gpg', '--batch', '--pinentry-mode=loopback', 
                    '--default-key', os.environ['PGP_KEY_NAME'], '-abs', '--clearsign', 
                    '--passphrase', os.environ['PGP_KEY_PASSWD'],
                    '-o', repo / f'dists/stable/InRelease', repo / f'dists/stable/Release'], check=True)
    subprocess.run(['aws', 's3', 'sync', repo, 's3://gershnik.com/apt-repo'], check=True)

    uploadResults(deb, workdir / f'wsddn-deb-systemd-{VERSION}.gz')
