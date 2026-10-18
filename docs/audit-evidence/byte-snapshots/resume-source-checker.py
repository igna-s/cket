"""Check campaign endpoints and live WSL processes before resuming verification."""
from pathlib import Path
import hashlib
import json
import os

root = Path(__file__).resolve().parent.parent
candidate = root / 'build/byte-snapshot-prototype'
main = json.loads((root / 'docs/audit-evidence/strncmp/integration-summary.json').read_text())
frozen = json.loads((candidate / 'build/byte-snapshot-verified-sources.json').read_text())
for folder, hashes in ((root, main['source_sha256']), (candidate, frozen)):
    for name, digest in hashes.items():
        assert hashlib.sha256((folder / name).read_bytes()).hexdigest() == digest, (folder, name)
    print(f'{folder.name}: {len(hashes)} source hashes still match.')
own = os.getpid()
for proc in sorted(Path('/proc').iterdir()):
    if not proc.name.isdecimal() or int(proc.name) == own:
        continue
    try:
        command = (proc / 'cmdline').read_bytes().replace(b'\0', b' ').decode(errors='replace')
    except (FileNotFoundError, PermissionError, ProcessLookupError):
        continue
    if any(label in command for label in ('byte-snapshot', 'validate_', 'scripts/verify.sh', 'scripts/sanitize-all.sh')):
        print(f'LIVE {proc.name}: {command[:1500]}')
