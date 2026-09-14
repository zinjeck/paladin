"""Temporary, exact-source transport for the user-authorized PR26 continuation.
Applies only the locally built/tested source delta, recreates authored assets,
verifies a complete content fingerprint, and fast-forwards only the same PR.
No main writes, force pushes, secret output, or workflow modifications.
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
BASE = 'ae9e2d4cdf0436e5a0d6a70601a18f1eddc9d221'
XZ_SHA = '4066c2ad875c18738a2c58584b6758d4ea36686c8abd03878ef5ed007d69c45c'
PATCH_SHA = 'b53ef8b2fc9544d315561effa6c9e808aabdc01f56b1072b006e9b37b897e39e'
CONTENT_SHA = 'bc28494b368b6361925c7ac0fda386ea7fd050fdeead54a18cd54dbb3021b8b1'
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
require(git('rev-parse', 'HEAD^') == BASE, 'Unexpected publication base; do not overwrite newer work')
require(git('ls-remote', 'origin', 'refs/heads/' + BRANCH).split()[0] == head, 'PR branch advanced')
require(not git('status', '--porcelain'), 'Checkout is not clean')
compressed = b''.join((PARTS / ('part' + str(i) + '.xz')).read_bytes() for i in range(8))
require(len(compressed) == 38060 and hashlib.sha256(compressed).hexdigest() == XZ_SHA, 'Transport checksum mismatch')
patch = lzma.decompress(compressed)
require(len(patch) == 180878 and hashlib.sha256(patch).hexdigest() == PATCH_SHA, 'Source patch checksum mismatch')
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
require(len(paths) == 135 and all(allowed(p) for p in paths), 'Unexpected changed-file set')
fingerprint = hashlib.sha256()
for path in sorted(paths):
    data = subprocess.check_output(['git', 'show', ':' + path])
    if path.endswith('.png'):
        image = Image.open(BytesIO(data)).convert('RGBA')
        data = struct.pack('>II', *image.size) + image.tobytes()
    fingerprint.update(path.encode() + b'\0' + hashlib.sha256(data).digest())
require(fingerprint.hexdigest() == CONTENT_SHA, 'Published sources/pixels differ from local verification')
print('Verified 135 implementation files, source byte hashes and all authored RGBA pixels:', CONTENT_SHA)
# The transport is not a deliverable. Remove it in the actual source commit.
shutil.rmtree(PARTS)
subprocess.run(['git', 'add', '-u', '--', 'tools/pr26-publication'], check=True)
subprocess.run(['git', '-c', 'user.name=github-actions[bot]',
                '-c', 'user.email=41898282+github-actions[bot]@users.noreply.github.com',
                'commit', '-m', 'feat: complete connected UI, animated art, military units and food supply chain',
                '-m', 'Publish the user-authorized locally verified PR26 continuation. Includes real source, tests, authored sprite exports and art sources; removes temporary payload. Source and pixel fingerprint: ' + CONTENT_SHA], check=True)
require(git('ls-remote', 'origin', 'refs/heads/' + BRANCH).split()[0] == head, 'PR branch advanced before publication')
subprocess.run(['git', 'push', 'origin', 'HEAD:refs/heads/' + BRANCH], check=True)
print('Published source commit:', git('rev-parse', 'HEAD'))
