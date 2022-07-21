
from distutils.command.build import build
import os
import io
import sys
import subprocess
import argparse
import json
import tempfile
import base64

from pathlib import Path
from stat import S_IWUSR, S_IRUSR

SSH_FLAGS = ['-o', 'StrictHostKeyChecking=no', '-o', 'UserKnownHostsFile=/dev/null', '-o', 'ConnectionAttempts=3']

os.environ['AWS_DEFAULT_OUTPUT'] = 'json'

parser = argparse.ArgumentParser()

parser.add_argument('--type', required=True)
parser.add_argument('--instance', required=True)
parser.add_argument('--instance-user', dest='instanceUser', required=True)
parser.add_argument('--ref', dest='ref', required=False)
parser.add_argument('--upload-results', dest='uploadResults', action='store_true', required=False)

args = parser.parse_args()

ref = args.ref if args.ref else subprocess.run(['git', 'rev-parse', 'HEAD'], capture_output=True, check=True, encoding='utf-8').stdout.strip()

pgpKey = os.environ.get('PGP_KEY')
pgpKeyName = os.environ.get('PGP_KEY_NAME')
pgpKeyPasswd = os.environ.get('PGP_KEY_PASSWD')

rsaKey = os.environ.get('RSA_KEY')

if args.uploadResults:
    buildOpts = f'--upload-results'
    uploadTokens = f"""
export AWS_ACCESS_KEY_ID={os.environ["AWS_ACCESS_KEY_ID"]}
export AWS_SECRET_ACCESS_KEY={os.environ["AWS_SECRET_ACCESS_KEY"]}
export AWS_DEFAULT_REGION={os.environ["AWS_DEFAULT_REGION"]}
export GH_TOKEN={os.environ["GH_TOKEN"]}
""".strip()
    if pgpKey and pgpKeyName and pgpKeyPasswd:
        uploadTokens += f"""
export PGP_KEY_NAME={pgpKeyName}
export PGP_KEY_PASSWD={pgpKeyPasswd}
""".rstrip()

else:
    buildOpts = ''
    uploadTokens = ''

BUILD_COMMAND = f"""
set -e
cd work/wsdd-native
git fetch --all
git fetch -f --prune --tags
git reset --hard {ref}
[ -d "out" ] || mkdir out
cd out
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFETCHCONTENT_QUIET=OFF -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" ..
{uploadTokens}
../installers/{args.type}/build.py {buildOpts} .. .
""".lstrip()


def main():
    global SSH_FLAGS
    started = False
    try:
        host = None
        while True:
            output = json.loads(subprocess.run(['aws', 'ec2', 'describe-instances', '--instance-ids', args.instance,
                                                '--query=Reservations[0].Instances[0].[State.Name, PublicIpAddress]'], 
                                                capture_output=True, check=True, encoding='utf-8').stdout)
            if output[0] == "running":
                print("Instance is running")
                host = output[1]
                break
            if output[0] == "stopped":
                print("Instance is stopped, starting")
                subprocess.run(['aws', 'ec2', 'start-instances', '--instance-ids', args.instance],
                                 capture_output=True, check=True, encoding='utf-8')
                started = True
                continue
            if not (output[0] in ("pending", "stopping")):
                print(f"Unexpected state: {output[0]}", file=sys.stderr)
                return 1

        host = f'{args.instanceUser}@{host}'

        keyFile = tempfile.NamedTemporaryFile()
        os.chmod(keyFile.name, S_IRUSR | S_IWUSR)
        keyFile.write(base64.b64decode(os.environ['AWS_KEY']))
        keyFile.flush()

        SSH_FLAGS += ['-i', keyFile.name]

        if pgpKey and pgpKeyName and pgpKeyPasswd:
            subprocess.run(['ssh'] + SSH_FLAGS + [host, 'rm -rf ~/.gnupg'], check=True)
            pipe = subprocess.Popen(['ssh'] + SSH_FLAGS + [host, 'cat | gpg --import --batch --pinentry-mode=loopback'], 
                                    stdin=subprocess.PIPE)
            pipe.communicate(base64.b64decode(pgpKey))
            if pipe.returncode != 0:
                sys.exit(1)

        if rsaKey:
            pipe = subprocess.Popen(['ssh'] + SSH_FLAGS + [host, 'cat > ~/.ssh/bsd-repo-key'], stdin=subprocess.PIPE)
            pipe.communicate(base64.b64decode(rsaKey))
            if pipe.returncode != 0:
                sys.exit(1)
        
        pipe = subprocess.Popen(['ssh'] + SSH_FLAGS + [host, '. /dev/stdin'], stdin=subprocess.PIPE)
        pipe.communicate(BUILD_COMMAND.encode("utf-8"))
        if pipe.returncode != 0:
            sys.exit(1)

        return 0
    except subprocess.CalledProcessError as ex:
        if isinstance(ex.stdout, str) and len(ex.stdout) > 0:
            print(ex.stdout)
        if isinstance(ex.stderr, str) and len(ex.stderr) > 0:
            print(ex.stderr, file=sys.stderr)
        print(f"Command: {ex.cmd} failed. Exit code {ex.returncode}", file=sys.stderr)
        return 1
    finally:
        if started:
            print("Stopping instance")
            subprocess.run(['aws', 'ec2', 'stop-instances', '--instance-ids', args.instance], 
                            capture_output=True, check=True, encoding='utf-8')
        

sys.exit(main())
