#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import re
import json

from pathlib import Path

MYPATH = Path(__file__).parent
ROOT = MYPATH.parent.parent

def write_sources(deps: dict):
    ret = ['    "wsddn-v$pkgver.tgz::https://github.com/gershnik/wsdd-native/tarball/v$pkgver"\n']
    for dep, data in deps.items():
        version = data['version']
        url: str = data['url']
        url = url.replace('${version}', version)
        ret.append(f'    "{dep}-{version}.tgz::{url}"\n')
    return ret

def write_unpack(deps: dict):
    ret = ['    _unpack_source wsddn wsddn-v$pkgver.tgz\n']
    for dep, data in deps.items():
        version = data['version']
        ret.append(f'    _unpack_source wsddn/external/{dep} {dep}-{version}.tgz\n')
    return ret

def write_fetch_content(deps: dict):
    ret = []
    for idx, dep in enumerate(deps.keys()):
        ending = ' \\' if idx < len(deps) - 1 else ''
        ret.append(f'        -DFETCHCONTENT_SOURCE_DIR_{dep.upper()}=external/{dep}{ending}\n')
    return ret

def main():
    version = (ROOT / "VERSION").read_text().splitlines()[0]

    with open(ROOT / 'dependencies.json', 'rt', encoding='utf-8') as f:
        deps: dict = json.load(f)

    spec = (ROOT / "installers/arch/PKGBUILD").read_text()
    new_lines = []
    sources_seen = False
    unpack_seen = False
    fetch_content_seen = False
    for line in spec.splitlines(keepends=True):
        if sources_seen:
            if re.match(r'^\)', line):
                new_lines.append('source=(\n')
                new_lines += write_sources(deps)
                new_lines.append(line)
                sources_seen = False
        elif unpack_seen:
            if not re.match(r'\s*_unpack_source ', line):
                new_lines += write_unpack(deps)
                new_lines.append(line)
                unpack_seen = False
        elif fetch_content_seen:
            if not re.match(r'\s*-DFETCHCONTENT_SOURCE_DIR_', line):
                new_lines += write_fetch_content(deps)
                new_lines.append(line)
                fetch_content_seen = False
        else:
            if re.match(r"pkgver='\d+(?:\.\d+)*'", line):
                new_lines.append(f"pkgver='{version}'\n")
            elif re.match(r'pkgrel=(\d+)', line):
                new_lines.append('pkgrel=1\n')
            elif re.match(r'source=\(', line):
                sources_seen = True
            elif re.match(r'\s*_unpack_source wsddn', line):
                unpack_seen = True
            elif re.match(r'\s*-DFETCHCONTENT_SOURCE_DIR_', line):
                fetch_content_seen = True
            else:
                new_lines.append(line)
    
    with open(ROOT / "installers/arch/PKGBUILD", 'wt', encoding='utf-8') as f:
        f.writelines(new_lines)


if __name__ == '__main__':
    main()
