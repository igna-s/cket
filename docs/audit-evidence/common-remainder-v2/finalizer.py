"""Finish metadata with the formatter's installed Python environment.

The initial archiver completed and verified data, but system Python lacked black.
Reuse those original archive bytes; never rerun or relabel any compiler test.
"""
from pathlib import Path

root = Path('/tmp/quantum-audit-isolated-20261017')
source = (root / 'build/archive-common-remainder-v2.py').read_text()
prefix = source.split('\ncapsule.mkdir()')[0]
suffix = source[source.index('\nspec = importlib.util.spec_from_file_location'):]
middle = '''
selected = load(capsule / 'evidence-files.json')
shutil.copy2(Path(__file__), capsule / 'finalizer.py')
'''
exec(compile(prefix + middle + suffix, str(Path(__file__)), 'exec'), globals())
