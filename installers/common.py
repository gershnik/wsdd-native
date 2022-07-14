import subprocess

VERSION = '0.1'

verRes = subprocess.run(['git', 'describe', '--tags', '--match', 'v*', '--abbrev=0'], capture_output=True, encoding='utf-8')
if verRes.returncode == 0:
    VERSION = verRes.stdout[1:]


def buildCode(builddir):
    subprocess.run(['cmake', '--build', builddir], check=True)

def installCode(builddir, stagedir):
    subprocess.run(['cmake', '--install', builddir, '--prefix', stagedir], check=True)


def copyTemplated(src, dst, map):
    dstdir = dst.parent
    dstdir.mkdir(parents=True, exist_ok=True)
    dst.write_text(src.read_text().format_map(map))
