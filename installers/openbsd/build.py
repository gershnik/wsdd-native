#! /usr/bin/env python3

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import sys
import subprocess
import shutil
import re
import argparse
from pathlib import Path

ARCH = subprocess.run(['uname', '-m'], check=True, capture_output=True, encoding="utf-8").stdout.strip()

mydir = Path(sys.argv[0]).parent

sys.path.append(str(mydir.absolute().parent))

from common import getSrcVersion, buildCode, installCode, copyTemplated

parser = argparse.ArgumentParser()

parser.add_argument('srcdir', type=Path)
parser.add_argument('builddir', type=Path)
parser.add_argument('--upload-results', dest='uploadResults', action='store_true', required=False)
#parser.add_argument('--arch', required=False)

args = parser.parse_args()

srcdir: Path = args.srcdir
builddir: Path = args.builddir

buildCode(builddir)

VERSION = getSrcVersion(srcdir)

workdir = builddir / 'stage/openbsd'
stagedir = workdir / 'root'
shutil.rmtree(workdir, ignore_errors=True)
stagedir.mkdir(parents=True)

installCode(builddir, stagedir / 'usr/local')

shutil.copytree(srcdir / 'config/openbsd', stagedir, dirs_exist_ok=True)

copyTemplated(mydir.parent / 'wsddn.conf', stagedir / 'etc/wsddn/wsddn.conf.sample', {
    'SAMPLE_IFACE_NAME': "em0",
    'RELOAD_INSTRUCTIONS': """
# sudo rcctl reload wsddn
# or
# sudo kill -HUP $(</var/run/wsddn.pid)  
""".lstrip()
})

libs = []
ldd_out = subprocess.run(['ldd', stagedir / 'usr/local/bin/wsddn'],
                         stdout=subprocess.PIPE, encoding='utf-8', check=True).stdout
for line in iter(ldd_out.splitlines()):
    m = re.match(r'^\t[0-9a-f]+ [0-9a-f]+ ([^ ]+) +[^ ]+ +[^ ]+ +[^ ]+ +(.*)$', line)
    if not m:
        continue
    if m.group(1) != 'rlib':
        continue
    lib = Path(m.group(2))
    m = re.match(r'^lib([^.]+)\.so\.(.*)', lib.name)
    if not m:
        continue
    libs += ['-W', f'{m.group(1)}.{m.group(2)}']
    

COMMENT = 'WS-Discovery Host Daemon'
DESC = 'Allows your  machine to be discovered by Windows 10 and above systems and displayed by their Explorer "Network" views.'
MAINTAINER = 'Eugene Gershnik <gershnik@hotmail.com>'
HOMEPAGE = 'https://github.com/gershnik/wsdd-native'

LOGCONF = '/var/log/wsddn.log                      644  5     1000 *     Z      /var/run/wsddn.pid'

(workdir / 'packinglist').write_text(
f"""
@owner 0
@group 0

@unexec         if rcctl check wsddn > /dev/null 2>&1; then rcctl stop wsddn; fi
@unexec-delete  rm -rf /var/run/wsddn.pid
@unexec-delete  rm -rf /var/log/wsddn.*

@mode 755
@bin            usr/local/bin/wsddn
@rcscript       etc/rc.d/wsddn
@dir            etc/wsddn

@mode 644
@man            usr/local/man/man8/wsddn.8.gz
@file           etc/wsddn/wsddn.conf.sample
@sample         etc/wsddn/wsddn.conf

@newgroup       _wsddn:
@newuser        _wsddn::_wsddn:daemon:WS-Discovery Daemon:/var/empty:/sbin/nologin

@exec-add       grep -qxF '/var/log/wsddn.log' /etc/newsyslog.conf || echo '{LOGCONF}' >> /etc/newsyslog.conf
@unexec-delete  sed -i '/^\/var\/log\/wsddn.log/d' /etc/newsyslog.conf

""".lstrip())

subprocess.run(['pkg_create', '-v',
                     '-A', ARCH,
                     '-d', f'-{DESC}',
                     '-f', 'packinglist',
                     '-D', 'FULLPKGPATH=net/wsddn',
                     '-D', f'COMMENT={COMMENT}',
                     '-D', f'MAINTAINER={MAINTAINER}',
                     '-D', f'HOMEPAGE={HOMEPAGE}',
                     '-B', stagedir.resolve(),
                     '-p', '/'] + 
                     libs + [
                     f'wsddn-{VERSION}.tgz'], cwd=workdir, check=True)
                     
subprocess.run(['gzip', '--keep', '--force', builddir / 'wsddn'], check=True)

if args.uploadResults:
    subprocess.run(['aws', 's3', 'cp', 
                    workdir / f'wsddn-{VERSION}.tgz', f's3://gershnik-builds/openbsd/wsddn-{VERSION}-{ARCH}.tgz'], 
                   check=True)
    subprocess.run(['aws', 's3', 'cp', 
                    builddir / 'wsddn.gz', f's3://wsddn-symbols/wsddn-openbsd-{VERSION}-{ARCH}.tgz'], check=True)
    
    shutil.move(workdir / f'wsddn-{VERSION}.tgz', workdir / f'wsddn-{VERSION}-OpenBSD-{ARCH}.tgz')
    subprocess.run(['gh', 'release', 'upload', f'v{VERSION}', workdir / f'wsddn-{VERSION}-OpenBSD-{ARCH}.tgz'], 
                   check=True)
    
