#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import sys
import subprocess
import shutil
from pathlib import Path

ARCH = subprocess.run(['uname', '-m'], check=True, capture_output=True, encoding="utf-8").stdout.strip()

mydir = Path(sys.argv[0]).parent
srcdir = Path(sys.argv[1])
builddir = Path(sys.argv[2])

sys.path.append(str(mydir.absolute().parent))

from common import VERSION, buildCode, installCode, copyTemplated

buildCode(builddir)

workdir = builddir / 'stage/bsd'
stagedir = workdir / 'root'
shutil.rmtree(workdir, ignore_errors=True)
stagedir.mkdir(parents=True)

installCode(builddir, stagedir / 'usr/local')

shutil.copytree(srcdir / 'config/bsd/usr', stagedir / 'usr', dirs_exist_ok=True)

copyTemplated(mydir.parent / 'wsddn.conf', stagedir / 'usr/local/etc/wsddn.conf.sample', {
    'SAMPLE_IFACE_NAME': "hn0",
    'RELOAD_INSTRUCTIONS': f"""
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
maintainer: Eugene Gershnik <gershnik@hotmail.com>
www: https://github.com/gershnik/wsddn
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

