#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import sys
import os
import subprocess
import shutil
import plistlib
from pathlib import Path

IDENTIFIER='io.github.gershnik.wsddn'

mydir = Path(sys.argv[0]).parent

sys.path.append(str(mydir.absolute().parent))

from common import VERSION, parseCommandLine, buildCode, installCode, copyTemplated, uploadResults

args = parseCommandLine()
srcdir: Path = args.srcdir
builddir: Path = args.builddir

buildCode(builddir)

workdir = builddir / 'stage/mac'
stagedir = workdir / f'root'
shutil.rmtree(workdir, ignore_errors=True)
stagedir.mkdir(parents=True)

ignoreCrap = shutil.ignore_patterns('.DS_Store')

supdir = stagedir / "Library/Application Support/wsddn"
supdir.mkdir(parents=True)

appDir = supdir / "wsddn.app"
shutil.copytree(builddir / "wrapper/wsddn.app", appDir, ignore=ignoreCrap)


(stagedir / 'usr/local/bin').mkdir(parents=True)
shutil.copy(mydir / 'wsddn-uninstall', stagedir / 'usr/local/bin')

copyTemplated(mydir / 'distribution.xml', workdir / 'distribution.xml', {
    'IDENTIFIER':IDENTIFIER, 
    'VERSION': VERSION
})

resdir = appDir / "Contents/Resources"
if args.sign or True:
    subprocess.run(['codesign', '--force', '--sign', 'Developer ID Application', '-o', 'runtime', '--timestamp', 
                        resdir / 'usr/local/bin/wsddn'], check=True)
    subprocess.run(['codesign', '--force', '--sign', 'Developer ID Application', '-o', 'runtime', '--timestamp', 
                    '--preserve-metadata=entitlements', 
                        appDir], check=True)


packagesdir = workdir / 'packages'
packagesdir.mkdir()

subprocess.run(['pkgbuild', 
                '--analyze', 
                '--root', str(stagedir),
                str(packagesdir/'component.plist')
            ], check=True)
with open(packagesdir/'component.plist', "rb") as src:
    components = plistlib.load(src, fmt=plistlib.FMT_XML)
for component in components:
    component['BundleIsRelocatable'] = False
with open(packagesdir/'component.plist', "wb") as dest:
    plistlib.dump(components, dest, fmt=plistlib.FMT_XML)
subprocess.run(['pkgbuild', 
                '--root',       str(stagedir), 
                '--component-plist', str(packagesdir/'component.plist'),
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
