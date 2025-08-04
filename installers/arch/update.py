#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import re
import json

from pathlib import Path

MYPATH = Path(__file__).parent
ROOT = MYPATH.parent.parent

def write_sources(deps: dict):
    ret = '    "wsddn-v$pkgver.tgz::https://github.com/gershnik/wsdd-native/tarball/v$pkgver"\n'
    for dep, data in deps.items():
        version = data['version']
        url: str = data['url']
        url = url.replace('${version}', version)
        ret += f'    "{dep}-{version}.tgz::{url}"\n'
    return ret

def main():
    version = (ROOT / "VERSION").read_text().splitlines()[0]

    with open(ROOT / 'dependencies.json', 'rt', encoding='utf-8') as f:
        deps: dict = json.load(f)

    spec = (ROOT / "installers/arch/PKGBUILD").read_text()
    new_spec = ''
    sources_seen = False
    for line in spec.splitlines():
        if not sources_seen:
            if re.match(r"pkgver='\d+(?:\.\d+)*'", line):
                new_spec += f"pkgver='{version}'\n"
            elif re.match(r'pkgrel=(\d+)', line):
                new_spec += 'pkgrel=1\n'
            elif re.match(r'source=\(', line):
                sources_seen = True
            else:
                new_spec += line + '\n'
        else:
            if re.match(r'^\)', line):
                new_spec += 'source=(\n'
                new_spec += write_sources(deps)
                new_spec += ')\n'
                sources_seen = False
    
    (ROOT / "installers/arch/PKGBUILD").write_text(new_spec)


if __name__ == '__main__':
    main()
