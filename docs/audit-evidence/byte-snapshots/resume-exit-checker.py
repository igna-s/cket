from datetime import datetime, timezone
from pathlib import Path
import json
import sys

candidate = Path(__file__).resolve().parent / 'byte-snapshot-prototype'
status = int(sys.argv[1])
with (candidate / 'build/byte-snapshot-resume-exit.json').open('x') as output:
    json.dump({'exit_code': status, 'completed_utc': datetime.now(timezone.utc).isoformat()}, output, indent=2)
    output.write('\n')
