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
#CODENAME = subprocess.run(['lsb_release', '-sc'], check=True, capture_output=True, encoding="utf-8").stdout.strip()

mydir = Path(sys.argv[0]).parent

sys.path.append(str(mydir.absolute().parent))

from common import parseCommandLine, getVersion, buildCode, installCode, copyTemplated

args = parseCommandLine()
srcdir: Path = args.srcdir
builddir: Path = args.builddir

buildCode(builddir)

VERSION = getVersion(builddir)

workdir = builddir / 'stage/deb'
stagedir = workdir / f'wsddn_{VERSION}-{RELEASE}_{ARCH}'
shutil.rmtree(workdir, ignore_errors=True)
stagedir.mkdir(parents=True)

installCode(builddir, stagedir / 'usr')

shutil.copytree(srcdir / 'config/systemd/usr',             stagedir / 'usr', dirs_exist_ok=True)
shutil.copytree(srcdir / 'config/firewalls/etc/ufw',       stagedir / 'etc/ufw', dirs_exist_ok=True)
shutil.copytree(srcdir / 'config/firewalls/etc/firewalld', stagedir / 'usr/lib/firewalld', dirs_exist_ok=True)
shutil.copytree(srcdir / 'config/sysv/etc',                stagedir / 'etc', dirs_exist_ok=True)

docdir = stagedir / 'usr/share/doc/wsddn'
docdir.mkdir(parents=True)
shutil.copy(mydir / 'copyright', docdir / 'copyright')

copyTemplated(mydir.parent / 'wsddn.conf', stagedir / "etc/wsddn.conf", {
    'SAMPLE_IFACE_NAME': "eth0",
    'RELOAD_INSTRUCTIONS': f"""
# sudo systemctl reload wsddn
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
Depends: {{shlibs_Depends}}
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

(stagedir/ 'debian').unlink()
subprocess.run(['dpkg-deb', '--build', '--root-owner-group', stagedir, workdir], check=True)

subprocess.run(['gzip', '--keep', '--force', builddir / 'wsddn'], check=True)

deb = list(workdir.glob('*.deb'))[0]

if args.uploadResults:
    subprocess.run(['aws', 's3', 'cp', deb, 's3://gershnik-builds/apt/'], check=True)
    subprocess.run(['aws', 's3', 'cp', builddir / 'wsddn.gz', f's3://wsddn-symbols/wsddn-deb-{VERSION}-{ARCH}.gz'], check=True)
    subprocess.run(['gh', 'release', 'upload', f'v{VERSION}', deb], check=True)
