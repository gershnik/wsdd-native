#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import sys
import subprocess
import shutil
from pathlib import Path

IDENTIFIER='io.github.gershnik.wsddn'

mydir = Path(sys.argv[0]).parent
srcdir = Path(sys.argv[1])
builddir = Path(sys.argv[2])

sys.path.append(str(mydir.absolute().parent))

from common import VERSION, buildCode, installCode, copyTemplated

buildCode(builddir)

workdir = builddir / 'stage/mac'
stagedir = workdir / f'root'
shutil.rmtree(workdir, ignore_errors=True)
stagedir.mkdir(parents=True)

installCode(builddir, stagedir / 'usr/local')

ignoreCrap = shutil.ignore_patterns('.DS_Store')

shutil.copytree(srcdir / 'config/mac', stagedir, dirs_exist_ok=True, ignore=ignoreCrap)

shutil.copy(mydir / 'wsddn-uninstall', stagedir / 'usr/local/bin')

copyTemplated(mydir.parent / 'wsddn.conf', stagedir / 'etc/wsddn.conf.sample', {
    'SAMPLE_IFACE_NAME': "en0",
    'RELOAD_INSTRUCTIONS': f"""
# sudo launchctl kickstart -k system/{IDENTIFIER}
# or
# sudo kill -HUP $(</var/run/wsddn/wsddn.pid)  
""".lstrip()
})

copyTemplated(mydir / 'distribution.xml', workdir / 'distribution.xml', {
    'IDENTIFIER':IDENTIFIER, 
    'VERSION': VERSION
})


packagesdir = workdir / 'packages'
packagesdir.mkdir()
subprocess.run(['pkgbuild', 
                '--root',       str(stagedir), 
                '--scripts',    str(mydir / 'scripts'),
                '--identifier', IDENTIFIER, 
                '--version',    VERSION,
                '--ownership',  'recommended',
                str(packagesdir/'output.pkg')
            ], check=True)

subprocess.run(['productbuild', 
                '--distribution', workdir / 'distribution.xml',
                '--package-path', str(packagesdir),
                '--resources',    str(mydir / 'html'),
                '--version',      VERSION,
                str(workdir / 'wsddn.pkg')
            ], check=True)
