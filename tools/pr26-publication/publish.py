"""Temporary exact-source transport for the user-authorized PR26 continuation.
Verifies the locally tested source delta and authored pixels, then fast-forwards
only the same PR. No main writes, force pushes, or secret output.
"""
from pathlib import Path
from io import BytesIO
import hashlib
import lzma
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from PIL import Image

BRANCH = 'feat/complete-ui-military-food-chain'
EXPECTED_PARENT = '2b9164a85657ec319f365f2ec463af7128126599'
XZ_SHA = '4066c2ad875c18738a2c58584b6758d4ea36686c8abd03878ef5ed007d69c45c'
PATCH_SHA = 'b53ef8b2fc9544d315561effa6c9e808aabdc01f56b1072b006e9b37b897e39e'
CONTENT_SHA = 'd1d41a0319de7601002a8b351a6438d9e73e85f23c038cf97e1e57af53b8b252'
ROOT = Path(__file__).resolve().parents[2]
PARTS = Path(__file__).resolve().parent
os.chdir(ROOT)

def git(*args):
    return subprocess.check_output(['git', *args]).decode().strip()

def require(condition, message):
    if not condition:
        raise RuntimeError(message)

def allowed(path):
    return path in ('CMakeLists.txt', 'assets/sprites/sprites.catalog',
                    'config/city-objects.catalog', 'tools/asset_compiler/SourceImporter.cpp') or path.startswith((
        'src/', 'tests/', 'assets/sprites/ui-military-v1/',
        'handoffs/art-direction/ui-military-v1/'))

require(os.environ.get('GITHUB_REPOSITORY') == 'zinjeck/paladin', 'Wrong repository')
head = git('rev-parse', 'HEAD')
require(git('rev-parse', 'HEAD^') == EXPECTED_PARENT, 'Unexpected publication parent; preserve newer work')
require(git('ls-remote', 'origin', 'refs/heads/' + BRANCH).split()[0] == head, 'PR branch advanced')
require(not git('status', '--porcelain'), 'Checkout is not clean')
compressed = b''.join((PARTS / ('part' + str(i) + '.xz')).read_bytes() for i in range(8))
require(len(compressed) == 38060 and hashlib.sha256(compressed).hexdigest() == XZ_SHA, 'Transport checksum mismatch')
patch = lzma.decompress(compressed)
require(len(patch) == 180878 and hashlib.sha256(patch).hexdigest() == PATCH_SHA, 'Source patch checksum mismatch')
# The recovered Windows tar uses CRLF; canonical Git blobs use LF. Normalize
# only line endings after validating the original transport. A second local
# verification applied this normalized patch to an LF checkpoint and compared
# all 135 output source files/pixel buffers against the tested implementation.
patch = patch.replace(b'\r\n', b'\n')
with tempfile.NamedTemporaryFile(suffix='.patch', delete=False) as stream:
    stream.write(patch)
    patch_path = stream.name
try:
    subprocess.run(['git', 'apply', '--check', '--index', patch_path], check=True)
    subprocess.run(['git', 'apply', '--index', '--whitespace=nowarn', patch_path], check=True)
finally:
    Path(patch_path).unlink()
for generator in ('generate_art.py', 'serif-glyph-source.py'):
    subprocess.run([sys.executable, str(ROOT / 'handoffs/art-direction/ui-military-v1' / generator)], check=True)
subprocess.run(['git', 'add', '--', 'CMakeLists.txt', 'assets/sprites', 'config/city-objects.catalog',
                'src', 'tests', 'tools/asset_compiler', 'handoffs/art-direction/ui-military-v1'], check=True)
paths = git('diff', '--cached', '--name-only').splitlines()
require(len(paths) == 135 and all(allowed(p) for p in paths), 'Unexpected changed-file set: ' + str(len(paths)))
fingerprint = hashlib.sha256()
for path in sorted(paths):
    data = subprocess.check_output(['git', 'show', ':' + path])
    if path.endswith('.png'):
        image = Image.open(BytesIO(data)).convert('RGBA')
        data = struct.pack('>II', *image.size) + image.tobytes()
    else:
        data = data.replace(b'\r\n', b'\n')
    fingerprint.update(path.encode() + b'\0' + hashlib.sha256(data).digest())
require(fingerprint.hexdigest() == CONTENT_SHA, 'Source/pixel fingerprint mismatch: ' + fingerprint.hexdigest())
print('Verified 135 implementation files, normalized source bytes and all authored RGBA pixels:', CONTENT_SHA)
# The transport is not a deliverable. Remove it in the actual source commit.
shutil.rmtree(PARTS)
subprocess.run(['git', 'add', '-u', '--', 'tools/pr26-publication'], check=True)
subprocess.run(['git', '-c', 'user.name=github-actions[bot]',
                '-c', 'user.email=41898282+github-actions[bot]@users.noreply.github.com',
                'commit', '-m', 'feat: complete connected UI, animated art, military units and food supply chain',
                '-m', 'Publish the user-authorized locally verified PR26 continuation. Includes source, tests, authored sprite exports and art sources; removes temporary payload. Normalized source and pixel fingerprint: ' + CONTENT_SHA], check=True)
require(git('ls-remote', 'origin', 'refs/heads/' + BRANCH).split()[0] == head, 'PR branch advanced before publication')
subprocess.run(['git', 'push', 'origin', 'HEAD:refs/heads/' + BRANCH], check=True)
print('Published source commit:', git('rev-parse', 'HEAD'))
