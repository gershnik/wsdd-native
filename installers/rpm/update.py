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
    ret = ''
    for dep, data in deps.items():
        version = data['version']
        url: str = data['url']
        url = url.replace('${version}', version)
        ret += f'Source:{gap}{url}#/{dep}.tgz\n'
    return ret

def main():
    version = (ROOT / "VERSION").read_text().splitlines()[0]

    today = datetime.date.today().strftime("%a %b %d %Y")

    with open(ROOT / 'dependencies.json', 'rt', encoding='utf-8') as f:
        deps: dict = json.load(f)
    

    spec = (ROOT / "installers/rpm/wsddn.spec").read_text()
    new_spec = ''
    sources_written = False
    for line in spec.splitlines():
        if (m := re.match(r'Version:(\s*)\d+(?:\.\d+)*', line)):
            new_spec += f'Version:{m.group(1)}{version}\n'
        elif (m := re.match(r'Release:(\s*)\d+%\{\?dist\}', line)):
            new_spec += f'Release:{m.group(1)}1%{{?dist}}\n'
        elif (m := re.match(r'Source:(\s*)\S+', line)):
            if not sources_written:
                new_spec += line + '\n'
                new_spec += write_sources(deps, m.group(1))
                sources_written = True
        elif (m := re.match(r'%global\s+deps\s+', line)):
            line = m.group(0) + ' '.join(deps.keys())
            new_spec += line + '\n'
        elif (m := re.match(r'^%changelog$', line)):
            new_spec += line + '\n'
            new_spec += f'* {today} gershnik - {version}-1\n- Release {version}\n\n'
        else:
            new_spec += line + '\n'

    (ROOT / "installers/rpm/wsddn.spec").write_text(new_spec)

if __name__ == '__main__':
    main()
