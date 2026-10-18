"""Start a detached local verification job with a persistent PID identity."""
from datetime import datetime, timezone
from pathlib import Path
import hashlib
import json
import subprocess

root = Path(__file__).resolve().parent.parent
candidate = root / 'build/byte-snapshot-prototype'
runner = root / 'build/run-byte-snapshot-resume.sh'
state = candidate / 'build/byte-snapshot-resume-job.json'
assert not state.exists(), 'Resume job already recorded; poll it instead of starting a duplicate.'
assert not (candidate / 'build/byte-snapshot-resume-exit.json').exists()
subprocess.run(['bash', '-n', str(runner)], check=True)
subprocess.run(['python3', str(root / 'build/check-byte-snapshot-resume.py')], check=True)
for proc in Path('/proc').iterdir():
    if not proc.name.isdecimal():
        continue
    try:
        arguments = (proc / 'cmdline').read_bytes().split(b'\0')
    except (FileNotFoundError, PermissionError, ProcessLookupError):
        continue
    assert str(runner).encode() not in arguments, f'Resume worker already live: {proc.name}'
prefix = candidate / 'build/byte-snapshot-full-verify.log'
with (candidate / 'build/byte-snapshot-interrupted-prefix.log').open('xb') as output:
    output.write(prefix.read_bytes())
frozen_path = candidate / 'build/byte-snapshot-verified-sources.json'
metadata = {
    'started_utc': datetime.now(timezone.utc).isoformat(),
    'runner': str(runner), 'cwd': str(candidate),
    'source_manifest_sha256': hashlib.sha256(frozen_path.read_bytes()).hexdigest(),
    'interrupted_prefix_sha256': hashlib.sha256(prefix.read_bytes()).hexdigest(),
    'scope': 'Normal prefix completed through strncmp. The interrupted 650-check snapshot suite is restarted, followed by every remaining normal suite and the complete sanitizer pipeline. Original normal binaries are retained from the frozen source build. Fresh sanitizer binaries are built by sanitize-all.sh. This is a segmented campaign, not an uninterrupted verify.sh invocation.',
    'interruption_evidence': 'Old unified-exec handle 74110 missing; a /proc scan found no verification worker. No final completion marker was present. Source endpoints matched 127 main and 129 candidate hashes before resumption.',
    'toolchain_at_resume': subprocess.check_output(['gcc-11', '-dumpfullversion'], text=True).strip(),
    'system_at_resume': subprocess.check_output(['uname', '-srmo'], text=True).strip(),
    'libc_at_resume': subprocess.check_output(['getconf', 'GNU_LIBC_VERSION'], text=True).strip(),
}
with state.open('x') as output, (candidate / 'build/byte-snapshot-resume.log').open('xb') as log:
    process = subprocess.Popen(['bash', str(runner)], cwd=candidate,
                               stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT,
                               start_new_session=True)
    metadata['pid'] = process.pid
    metadata['linux_start_ticks'] = int((Path('/proc') / str(process.pid) / 'stat').read_text().rsplit(')', 1)[1].split()[19])
    json.dump(metadata, output, indent=2)
    output.write('\n')
print(json.dumps(metadata, indent=2))
