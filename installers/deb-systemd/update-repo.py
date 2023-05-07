#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import sys
import os
import shutil
import subprocess
from pathlib import Path


builddir = Path(sys.argv[1])

CODENAMES = ('bullseye', 'focal', 'jammy')
ARCHS = ('amd64', 'arm64')

builddir.mkdir(parents=True, exist_ok=True)
repo = builddir / 'apt-repo'
if repo.exists():
    shutil.rmtree(repo)
tmp = builddir / 'apt-repo-tmp'
if tmp.exists():
    shutil.rmtree(tmp)
tmp.mkdir(parents=True)

subprocess.run(['aws', 's3', 'sync', 's3://gershnik.com/apt-repo', repo], check=True)
for f in list((repo / 'pool/main').iterdir()):
    f.rename(tmp / f.name)

for codename in CODENAMES:
    for f in list(tmp.iterdir()):
        if f.name.startswith(f'{codename}-'):
            f.rename(repo / 'pool/main' / f.name)

    for arch in ARCHS:
        archDir = repo / f'dists/{codename}/main/binary-{arch}'
        archDir.mkdir(parents=True, exist_ok=True)
        
        packagesFilePath = archDir / 'Packages'
        with open(packagesFilePath, "w") as packages:
            subprocess.run(['apt-ftparchive', '--arch', arch, 'packages', 'pool'], stdout=packages, cwd=repo, check=True)
        if packagesFilePath.stat().st_size > 0:
            subprocess.run(['gzip', '--keep', '--force', repo / f'dists/{codename}/main/binary-{arch}/Packages'], check=True)
        else:
            shutil.rmtree(archDir)

    for f in list((repo / 'pool/main').iterdir()):
        f.rename(tmp / f.name)

    (repo / f'dists/{codename}/Release').unlink(missing_ok=True)
    relaseContent = subprocess.run(['apt-ftparchive', 'release', repo / f'dists/{codename}'], capture_output=True, encoding='utf-8', check=True).stdout
    (repo / f'dists/{codename}/Release').write_text(f"""
Origin: github.com/gershnik repository
Label: github.com/gershnik
Suite: {codename}
Codename: {codename}
Version: 1.0
Architectures: {','.join(ARCHS)}
Components: main
Description: Software repository for github.com/gershnik
{relaseContent}
""".lstrip())
        
    (repo / f'dists/{codename}/Release.pgp').unlink(missing_ok=True)
    subprocess.run(['gpg', '--batch', '--pinentry-mode=loopback',
                    '--default-key', os.environ['PGP_KEY_NAME'], '-abs',  
                    '--passphrase', os.environ['PGP_KEY_PASSWD'],
                    '-o', repo / f'dists/{codename}/Release.pgp', repo / f'dists/{codename}/Release'], check=True)
    (repo / f'dists/{codename}/InRelease').unlink(missing_ok=True)
    subprocess.run(['gpg', '--batch', '--pinentry-mode=loopback', 
                    '--default-key', os.environ['PGP_KEY_NAME'], '-abs', '--clearsign', 
                    '--passphrase', os.environ['PGP_KEY_PASSWD'],
                    '-o', repo / f'dists/{codename}/InRelease', repo / f'dists/{codename}/Release'], check=True)

for f in list(tmp.iterdir()):
    f.rename(repo / 'pool/main' / f.name)

subprocess.run(['aws', 's3', 'sync', repo, 's3://gershnik.com/apt-repo'], check=True)
