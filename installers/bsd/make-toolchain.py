#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import sys
import argparse
import subprocess
import json
import shutil
import hashlib
from pathlib import Path


parser = argparse.ArgumentParser()

parser.add_argument('platform', type=str)
parser.add_argument('dir', type=Path)

args = parser.parse_args()

platform = args.platform
outdir: Path = args.dir

if platform == 'arm64':
    arch = 'aarch64'
elif platform == 'amd64':
    arch = 'x86_64'
else:
    print(f'no platform->arch mapping for {platform}', file=sys.stderr)
    sys.exit(1)

verRes = subprocess.run(['freebsd-version'], capture_output=True, encoding='utf-8')
if verRes.returncode != 0:
    sys.exit(1)
fullVersion = verRes.stdout.strip()
version = fullVersion[:fullVersion.rfind('-')]

toolchain = f'''
set(CMAKE_SYSROOT ${{CMAKE_CURRENT_LIST_DIR}})
set(CMAKE_C_COMPILER_TARGET {arch}-unknown-freebsd{version})
set(CMAKE_CXX_COMPILER_TARGET {arch}-unknown-freebsd{version})

set(CMAKE_FIND_ROOT_PATH  ${{CMAKE_CURRENT_LIST_DIR}})

set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_EXE_LINKER_FLAGS "${{CMAKE_EXE_LINKER_FLAGS}} -stdlib=libc++")

set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES
    "${{CMAKE_CURRENT_LIST_DIR}}/usr/include"
    "${{CMAKE_CURRENT_LIST_DIR}}/usr/include/sys"
)
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
    "${{CMAKE_CURRENT_LIST_DIR}}/usr/include/c++/v1"
    ${{CMAKE_C_STANDARD_INCLUDE_DIRECTORIES}}
)
'''.lstrip()
toolchainHash = hashlib.sha256(toolchain.encode('utf-8')).hexdigest()

newInfo = {
    'version': version,
    'platform': platform,
    'toolchainHash': toolchainHash
}

try:
    info = json.loads((outdir / '.info.json').read_text())
    if info == newInfo:
        print(f'{outdir} already contains the requested toolchain')
        sys.exit(0)
    print(f'{outdir} contains non-matching existing toolchain: \n{json.dumps(info, indent=4)}\nwill overwrite')
except (FileNotFoundError):
    pass
    
if outdir.exists():
    shutil.rmtree(outdir)
outdir.mkdir(parents=True, exist_ok=False)

procCurl = subprocess.Popen(['curl', '-LSs', 
                             f'https://download.freebsd.org/ftp/releases/{platform}/{fullVersion}/base.txz'
                        ], stdout=subprocess.PIPE)
procTar =  subprocess.Popen(['tar', 'Jxf', '-', 
                             '--include', './usr/include/*', 
                             '--include', './usr/lib/*', 
                             '--include', './lib/*'
                        ], cwd=outdir, stdin=procCurl.stdout)
procCurl.stdout.close()
procTar.wait()
procCurl.wait()
if procTar.returncode != 0 or procCurl.returncode != 0:
    print(f'unpacking failed', file=sys.stderr)
    sys.exit(1)


(outdir / 'toolchain.cmake').write_text(toolchain)
(outdir / '.info.json').write_text(json.dumps(newInfo, indent = 4))
