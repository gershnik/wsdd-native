#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import re
import json
import datetime

from pathlib import Path

MYPATH = Path(__file__).parent
ROOT = MYPATH.parent.parent

def write_sources(deps: dict, gap: str):
    ret = []
    for dep, data in deps.items():
        version = data['version']
        url: str = data['url']
        url = url.replace('${version}', version)
        ret.append(f'Source:{gap}{url}#/{dep}.tgz\n')
    return ret

def write_unpack(deps: dict):
    ret = ['_unpack_source wsddn wsddn.tgz\n']
    for dep in deps.keys():
        ret.append(f'_unpack_source wsddn/external/{dep} {dep}.tgz\n')
    return ret

def write_fetch_content(deps: dict):
    ret = []
    for idx, dep in enumerate(deps.keys()):
        ending = ' \\' if idx < len(deps) - 1 else ''
        ret.append(f'    "-DFETCHCONTENT_SOURCE_DIR_{dep.upper()}=$mydir/wsddn/external/{dep}"{ending}\n')
    return ret

def main():
    version = (ROOT / "VERSION").read_text().splitlines()[0]

    today = datetime.date.today().strftime("%a %b %d %Y")

    with open(ROOT / 'dependencies.json', 'rt', encoding='utf-8') as f:
        deps: dict = json.load(f)
    

    spec = (ROOT / "installers/rpm/wsddn.spec").read_text()
    new_lines = []
    sources_written = False
    unpack_seen = False
    fetch_content_seen = False
    for line in spec.splitlines(keepends=True):
        if unpack_seen:
            if not re.match(r'\s*_unpack_source ', line):
                new_lines += write_unpack(deps)
                new_lines.append(line)
                unpack_seen = False
        elif fetch_content_seen:
            if not re.match(r'\s*"-DFETCHCONTENT_SOURCE_DIR_', line):
                new_lines += write_fetch_content(deps)
                new_lines.append(line)
                fetch_content_seen = False
        else:
            if (m := re.match(r'Version:(\s*)\d+(?:\.\d+)*', line)):
                new_lines.append(f'Version:{m.group(1)}{version}\n')
            elif (m := re.match(r'Release:(\s*)\d+%\{\?dist\}', line)):
                new_lines.append(f'Release:{m.group(1)}1%{{?dist}}\n')
            elif (m := re.match(r'Source:(\s*)\S+', line)):
                if not sources_written:
                    new_lines.append(line)
                    new_lines += write_sources(deps, m.group(1))
                    sources_written = True
            elif re.match(r'_unpack_source wsddn', line):
                unpack_seen = True
            elif re.match(r'\s*"-DFETCHCONTENT_SOURCE_DIR_', line):
                fetch_content_seen = True
            elif (m := re.match(r'^%changelog$', line)):
                new_lines.append(line)
                new_lines.append(f'* {today} gershnik - {version}-1\n- Release {version}\n\n')
            else:
                new_lines.append(line)

    with open(ROOT / "installers/rpm/wsddn.spec", 'wt', encoding='utf-8') as f:
        f.writelines(new_lines)

if __name__ == '__main__':
    main()
