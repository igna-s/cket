"""Complete preparation after an ambiguous line anchor; do not copy sources again."""
from pathlib import Path
import hashlib
import json

root = Path('/tmp/quantum-audit-isolated-20261017')
path = root / 'build/prepare-maintained-controlled-operands.py'
source = path.read_text()
candidate = root / 'build/coherent-body-maintained'
(candidate / 'build/preparation-original.py').write_text(source)
record = {'status': 'preparation-anchor-ambiguity-preserved', 'returncode': 1,
          'original_runner_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
          'reason': 'The unanchored execution line also matched the output path in the compile command. No compiler/test failure occurred; only preparation had stopped before formatting and freezing.'}
(candidate / 'build/preparation-failure.json').write_text(json.dumps(record, indent=2) + '\n')
before = "    '\"$san_root/build/validate_pointer_differences\"\\n',"
after = "    '\\n\"$san_root/build/validate_pointer_differences\"\\n',"
assert source.count(before) == 1
source = source.replace(before, after)
prefix = source[:source.index('candidate.mkdir()')]
namespace = {}
exec(compile(prefix, 'maintained-preparation-imports', 'exec'), namespace)
start = source.index('def insert_after(')
end = source.index("\n\ninsert_after('scripts/build.sh'", start)
exec(compile(source[start:end], 'maintained-preparation-helper', 'exec'), namespace)
start = source.index("insert_after('scripts/sanitize-all.sh',\n    '\\n")
tail = source[start:]
exec(compile(tail, 'maintained-preparation-continuation', 'exec'), namespace)
