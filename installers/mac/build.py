#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import sys
import os
import subprocess
import shutil
from pathlib import Path

IDENTIFIER='io.github.gershnik.wsddn'

mydir = Path(sys.argv[0]).parent

sys.path.append(str(mydir.absolute().parent))

from common import VERSION, parseCommandLine, buildCode, installCode, copyTemplated, uploadResults

args = parseCommandLine()
srcdir = args.srcdir
builddir = args.builddir

buildCode(builddir)

workdir = builddir / 'stage/mac'
stagedir = workdir / f'root'
shutil.rmtree(workdir, ignore_errors=True)
stagedir.mkdir(parents=True)

installCode(builddir, stagedir / 'usr/local')

if args.sign:
    subprocess.run(['codesign', '--force', '--sign', 'Developer ID Application', '-o', 'runtime', '--timestamp', 
                     stagedir / 'usr/local/bin/wsddn'], check=True)

ignoreCrap = shutil.ignore_patterns('.DS_Store')

shutil.copytree(srcdir / 'config/mac', stagedir, dirs_exist_ok=True, ignore=ignoreCrap)

shutil.copy(mydir / 'wsddn-uninstall', stagedir / 'usr/local/bin')

copyTemplated(mydir.parent / 'wsddn.conf', stagedir / 'etc/wsddn.conf.sample', {
    'SAMPLE_IFACE_NAME': "en0",
    'RELOAD_INSTRUCTIONS': f"""
# sudo launchctl kill HUP system/{IDENTIFIER}
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
                str(workdir/'wsddn.pkg')
            ], check=True)

installer = workdir / f'wsddn-macos-{VERSION}.pkg'

if args.sign:
    subprocess.run(['productsign', '--sign', 'Developer ID Installer', workdir / 'wsddn.pkg', installer], check=True)
    subprocess.run([mydir / 'notarize', '--user', os.environ['NOTARIZE_USER'], '--password', '@env:NOTARIZE_PWD', installer], check=True)
    print('Signature Info')
    res1 = subprocess.run(['pkgutil', '--check-signature', installer])
    print('\nAssesment')
    res2 = subprocess.run(['spctl', '--assess', '-vvv', '--type', 'install', installer])
    if res1.returncode != 0 or res2.returncode != 0:
        sys.exit(1)

if args.uploadResults:
    subprocess.run(['tar', '-C', builddir, '-czf', workdir.absolute() / f'wsddn-macos-{VERSION}.dSYM.tgz', 'wsddn.dSYM'], check=True)
    uploadResults(installer, workdir / f'wsddn-macos-{VERSION}.dSYM.tgz')
