#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import sys
import os
import subprocess
import shutil
import plistlib
import re
from pathlib import Path

IDENTIFIER='io.github.gershnik.wsddn'

mydir = Path(sys.argv[0]).parent

sys.path.append(str(mydir.absolute().parent))

from common import parseCommandLine, getVersion, buildCode, installCode, copyTemplated, uploadResults

args = parseCommandLine()
srcdir: Path = args.srcdir
builddir: Path = args.builddir

buildCode(builddir)

VERSION = getVersion(builddir)

workdir = builddir / 'stage/mac'
stagedir = workdir / f'root'
shutil.rmtree(workdir, ignore_errors=True)
stagedir.mkdir(parents=True)

installCode(builddir, stagedir / 'usr/local')

ignoreCrap = shutil.ignore_patterns('.DS_Store')

shutil.copytree(srcdir / 'config/mac', stagedir, dirs_exist_ok=True, ignore=ignoreCrap)

(stagedir / 'usr/local/bin').mkdir(parents=True, exist_ok=True)
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

with open(stagedir / 'Library/LaunchDaemons/io.github.gershnik.wsddn.plist', "rb") as src:
    daemonPlist = plistlib.load(src, fmt=plistlib.FMT_XML)
daemonPlist['ProgramArguments'][0] = 'wsddn'
daemonPlist['Program'] = '/usr/local/bin/wsddn'
with open(stagedir / 'Library/LaunchDaemons/io.github.gershnik.wsddn.plist', "wb") as dst:
    plistlib.dump(daemonPlist, dst, fmt=plistlib.FMT_XML)


if args.sign:
    subprocess.run(['codesign', '--force', '--sign', 'Developer ID Application', '-o', 'runtime', '--timestamp', 
                        stagedir / 'usr/local/bin/wsddn'], check=True)
else:
    subprocess.run(['codesign', '--force', '--sign', '-', '-o', 'runtime', '--timestamp=none', 
                        stagedir / 'usr/local/bin/wsddn'], check=True)


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
    pattern = re.compile(r'^\s*1. Developer ID Installer: .*\(([0-9A-Z]{10})\)$')
    teamId = None
    for line in subprocess.run(['pkgutil', '--check-signature', installer], 
                               check=True, stdout=subprocess.PIPE).stdout.decode('utf-8').splitlines():
        m = pattern.match(line)
        if m:
            teamId = m.group(1)
            break
    if teamId is None:
        print('Unable to find team ID from signature', file=sys.stderr)
        sys.exit(1)
    subprocess.run([mydir / 'notarize', '--user', os.environ['NOTARIZE_USER'], '--password', os.environ['NOTARIZE_PWD'], 
                    '--team', teamId, installer], check=True)
    print('Signature Info')
    res1 = subprocess.run(['pkgutil', '--check-signature', installer])
    print('\nAssesment')
    res2 = subprocess.run(['spctl', '--assess', '-vvv', '--type', 'install', installer])
    if res1.returncode != 0 or res2.returncode != 0:
        sys.exit(1)

if args.uploadResults:
    subprocess.run(['tar', '-C', builddir, '-czf', workdir.absolute() / f'wsddn-macos-{VERSION}.dSYM.tgz', 'wsddn.dSYM'], check=True)
    uploadResults(installer, workdir / f'wsddn-macos-{VERSION}.dSYM.tgz', VERSION)
