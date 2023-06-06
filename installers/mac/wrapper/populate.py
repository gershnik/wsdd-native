#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import sys
import os
import argparse
import subprocess
import shutil
import plistlib
from pathlib import Path

mydir = Path(sys.argv[0]).parent

parser = argparse.ArgumentParser()

parser.add_argument('bundledir', type=Path)
parser.add_argument('--identifier', required=True)
parser.add_argument('--wsddn-dir', required=True, type=Path, dest='wsddnDir')
parser.add_argument('--doc-dir', required=True, type=Path, dest='docDir')

args = parser.parse_args()


bundledir: Path = args.bundledir
wsddnDir: Path = args.wsddnDir
docDir: Path = args.docDir

resdir = bundledir / "Contents/Resources"
rootDir = mydir.parent.parent.parent

if resdir.exists():
    shutil.rmtree(resdir)


wsddnDest = resdir / 'usr/local/bin/wsddn'
wsddnDest.parent.mkdir(parents=True, exist_ok=True)
subprocess.run(['/usr/bin/strip', '-u', '-r', '-no_code_signature_warning', '-o', wsddnDest, wsddnDir/'wsddn'],
               check=True)
subprocess.run(['codesign', '--force', '--sign', '-', '--timestamp=none',  wsddnDest], check=True)

docDest = resdir / 'usr/local/share/man/man8/wsddn.8.gz'
docDest.parent.mkdir(parents=True, exist_ok=True)
shutil.copy(docDir / 'wsddn.8.gz', docDest)


with open(rootDir / 'config/mac/Library/LaunchDaemons/io.github.gershnik.wsddn.plist', "rb") as src:
    daemonPlist = plistlib.load(src, fmt=plistlib.FMT_XML)
daemonPlist['ProgramArguments'][0] = 'wsddn'
oldDaemonPlist = daemonPlist.copy()
oldDaemonPlist['Program'] = '/Library/Application Support/wsddn/wsddn.app/Contents/Resources/usr/local/bin/wsddn'
oldDaemonPlist['AssociatedBundleIdentifiers'] = 'io.github.gershnik.wsddn.wrapper'
daemonPlist['BundleProgram'] = 'Contents/Resources/usr/local/bin/wsddn'

plists = {
    resdir / 'Library/LaunchDaemons': oldDaemonPlist, 
    bundledir / 'Contents/Library/LaunchDaemons': daemonPlist
}

for dest, plist in plists.items():
    dest.mkdir(parents=True, exist_ok=True)
    with open(dest / 'io.github.gershnik.wsddn.plist', "wb") as dst:
        plistlib.dump(plist, dst, fmt=plistlib.FMT_XML)

confSrc = rootDir / 'installers/wsddn.conf'
confDest = resdir / 'etc/wsddn.conf.sample'
confDest.parent.mkdir(parents=True, exist_ok=True)
confDest.write_text(confSrc.read_text().format_map({
    'SAMPLE_IFACE_NAME': "en0",
    'RELOAD_INSTRUCTIONS': f"""
# sudo launchctl kill HUP system/{args.identifier}
""".lstrip()
}))

