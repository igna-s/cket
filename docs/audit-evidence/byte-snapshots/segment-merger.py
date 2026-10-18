"""Merge complete source-identical segments while retaining the interruption."""
from pathlib import Path
import hashlib
import json

candidate = Path(__file__).resolve().parent / 'byte-snapshot-prototype'
build = candidate / 'build'
job = json.loads((build / 'byte-snapshot-resume-job.json').read_text())
terminal = json.loads((build / 'byte-snapshot-resume-exit.json').read_text())
assert terminal['exit_code'] == 0, terminal
prefix = (build / 'byte-snapshot-interrupted-prefix.log').read_bytes()
assert hashlib.sha256(prefix).hexdigest() == job['interrupted_prefix_sha256']
assert hashlib.sha256((build / 'byte-snapshot-verified-sources.json').read_bytes()).hexdigest() == job['source_manifest_sha256']
tail = (build / 'byte-snapshot-resume.log').read_bytes()
closing = b'All compiler, runtime, example, synthesis and independent QASM checks passed.'
assert closing not in prefix and tail.rstrip().endswith(closing)
assert b'650 CPU byte snapshot checks passed' not in prefix
assert b'246 strncmp checks passed' in prefix
destination = build / 'byte-snapshot-resumed-verify.log'
with destination.open('xb') as output:
    output.write(prefix)
    output.write(b'\nSEGMENT BOUNDARY: interrupted normal run; matching frozen sources; resumed normal tail and full sanitizer pipeline below.\n')
    output.write(tail)
print('Prepared a segmented verification transcript; original prefix and resume records remain intact.')
