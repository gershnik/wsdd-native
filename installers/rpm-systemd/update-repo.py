#! /usr/bin/env -S python3 -u

# Copyright (c) 2022, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

import sys
import os
import shutil
import subprocess

from pathlib import Path

builddir = Path(sys.argv[1])
builddir.mkdir(parents=True, exist_ok=True)

repo = builddir / 'rpm-repo'
if repo.exists():
    shutil.rmtree(repo)

subprocess.run(['aws', 's3', 'sync', 's3://gershnik.com/rpm-repo', repo], check=True)

subprocess.run(['createrepo', '--update', '.'], cwd=repo, check=True)
(repo / 'repodata/repomd.xml.asc').unlink(missing_ok=True)
subprocess.run(['gpg', '--batch', '--pinentry-mode=loopback', 
                '--detach-sign', '--armor', 
                '--default-key', os.environ['PGP_KEY_NAME'],
                '--passphrase', os.environ['PGP_KEY_PASSWD'],
                repo / 'repodata/repomd.xml'], check=True)

(repo / 'gershnik.repo').write_text("""
[gershnik-repo]
name=github.com/gershnik repository
baseurl=https://www.gershnik.com/rpm-repo
enabled=1
gpgcheck=1
gpgkey=https://www.gershnik.com/rpm-repo/pgp-key.public
""".lstrip())
    
subprocess.run(['aws', 's3', 'sync', repo, 's3://gershnik.com/rpm-repo'], check=True)
