#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import sys
import subprocess
import shutil
import re
import argparse
from pathlib import Path

ARCH = subprocess.run(['uname', '-m'], check=True, capture_output=True, encoding="utf-8").stdout.strip()

ABI = None
for line in subprocess.run(['pkg', '-vv'], check=True, encoding="utf-8", capture_output=True).stdout.splitlines():
    m = re.match(r'ABI\s*=\s*"([^"]+)";', line)
    if m:
        ABI = m.group(1)
        break

if ABI is None:
    print("Unable to determine ABI", file=sys.stderr)
    sys.exit(1)

mydir = Path(sys.argv[0]).parent

sys.path.append(str(mydir.absolute().parent))

from common import getVersion, getSrcVersion, buildCode, installCode, copyTemplated

parser = argparse.ArgumentParser()

parser.add_argument('srcdir', type=Path)
parser.add_argument('builddir', type=Path)
parser.add_argument('--upload-results', dest='uploadResults', action='store_true', required=False)
parser.add_argument('--arch', required=False)

args = parser.parse_args()

srcdir: Path = args.srcdir
builddir: Path = args.builddir

buildCode(builddir)

if not args.arch is None:
    ABI = ABI[:ABI.rfind(':') + 1] + args.arch
    ARCH = args.arch
    VERSION = getSrcVersion(srcdir)
else:
    VERSION = getVersion(builddir)

workdir = builddir / 'stage/bsd'
stagedir = workdir / 'root'
shutil.rmtree(workdir, ignore_errors=True)
stagedir.mkdir(parents=True)

installCode(builddir, stagedir / 'usr/local')

shutil.copytree(srcdir / 'config/bsd/usr', stagedir / 'usr', dirs_exist_ok=True)

copyTemplated(mydir.parent / 'wsddn.conf', stagedir / 'usr/local/etc/wsddn.conf.sample', {
    'SAMPLE_IFACE_NAME': "hn0",
    'RELOAD_INSTRUCTIONS': """
# sudo service wsddn reload
# or
# sudo kill -HUP $(</var/run/wsddn/wsddn.pid)  
""".lstrip()
})

(workdir / '+MANIFEST').write_text(
f"""
name: wsddn
version: "{VERSION}"
arch: {ARCH}
origin: sysutils/wsddn
conflict: py*-wsdd-*
maintainer: Eugene Gershnik <gershnik@hotmail.com>
www: https://github.com/gershnik/wsdd-native
comment: WS-Discovery Host Daemon
desc: Allows your  machine to be discovered by Windows 10 and above systems and displayed by their Explorer "Network" views. 
prefix: /
""".lstrip())

with open(workdir / 'plist', 'w') as plist:
    for item in stagedir.glob('**/*'):
        if not item.is_dir():
    	    print(str(item.relative_to(stagedir)), file=plist)

shutil.copy(mydir / 'pre',            workdir / '+PRE_INSTALL')
shutil.copy(mydir / 'post_install',   workdir / '+POST_INSTALL')
shutil.copy(mydir / 'pre',            workdir / '+PRE_DEINSTALL')
shutil.copy(mydir / 'post_deinstall', workdir / '+POST_DEINSTALL')

subprocess.run(['pkg', 'create', '--verbose', '-m', workdir, '-r',  stagedir, '-p', workdir/'plist', '-o', workdir], check=True)


subprocess.run(['gzip', '--keep', '--force', builddir / 'wsddn'], check=True)

if args.uploadResults:
    ostype, oslevel, osarch = ABI.split(':')
    abiMarker = '-'.join((ostype, oslevel, osarch))

    subprocess.run(['aws', 's3', 'cp', workdir / f'wsddn-{VERSION}.pkg', f's3://gershnik-builds/freebsd/wsddn-{VERSION}-{ARCH}-{oslevel}.pkg'], 
                   check=True)
    subprocess.run(['aws', 's3', 'cp', builddir / 'wsddn.gz', f's3://wsddn-symbols/wsddn-{VERSION}-{abiMarker}.gz'], check=True)
    
    shutil.move(workdir / f'wsddn-{VERSION}.pkg', workdir / f'wsddn-{VERSION}-{abiMarker}.pkg')
    subprocess.run(['gh', 'release', 'upload', f'v{VERSION}', workdir / f'wsddn-{VERSION}-{abiMarker}.pkg'], check=True)
