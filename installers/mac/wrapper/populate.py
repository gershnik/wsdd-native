#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import sys
import argparse
import shutil
import plistlib
from pathlib import Path

mydir = Path(sys.argv[0]).parent

parser = argparse.ArgumentParser()

parser.add_argument('bundledir', type=Path)

args = parser.parse_args()


bundledir: Path = args.bundledir

resdir = bundledir / "Contents/Resources"
rootDir = mydir.parent.parent.parent

if resdir.exists():
    shutil.rmtree(resdir)
resdir.mkdir(parents=True)

with open(rootDir / 'config/mac/Library/LaunchDaemons/io.github.gershnik.wsddn.plist', "rb") as src:
    daemonPlist = plistlib.load(src, fmt=plistlib.FMT_XML)
daemonPlist['ProgramArguments'][0] = 'wsddn'
daemonPlist['Program'] = '/usr/local/bin/wsddn'
daemonPlist['AssociatedBundleIdentifiers'] = 'io.github.gershnik.wsddn.wrapper'

with open(resdir / 'io.github.gershnik.wsddn.plist', "wb") as dst:
    plistlib.dump(daemonPlist, dst, fmt=plistlib.FMT_XML)


