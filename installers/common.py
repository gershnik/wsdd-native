import subprocess
import argparse
from pathlib import Path

VERSION = '0.1'

verRes = subprocess.run(['git', 'describe', '--tags', '--match', 'v*', '--abbrev=0'], capture_output=True, encoding='utf-8')
if verRes.returncode == 0:
    VERSION = verRes.stdout.strip()[1:]


def parseCommandLine() -> argparse.Namespace :
    parser = argparse.ArgumentParser()

    parser.add_argument('srcdir', type=Path)
    parser.add_argument('builddir', type=Path)
    parser.add_argument('--sign', dest='sign', action='store_true', required=False)
    parser.add_argument('--upload-results', dest='uploadResults', action='store_true', required=False)

    args = parser.parse_args()

    if args.uploadResults:
        args.sign = True
    
    return args


def buildCode(builddir):
    subprocess.run(['cmake', '--build', builddir], check=True)

def installCode(builddir, stagedir):
    subprocess.run(['cmake', '--install', builddir, '--prefix', stagedir, '--strip'], check=True)


def copyTemplated(src, dst, map):
    dstdir = dst.parent
    dstdir.mkdir(parents=True, exist_ok=True)
    dst.write_text(src.read_text().format_map(map))

def uploadResults(installer, symfile):
    subprocess.run(['aws', 's3', 'cp', symfile, f's3://wsddn-symbols/{symfile.name}'], check=True)
    subprocess.run(['gh', 'release', 'upload', f'v{VERSION}', installer], check=True)
