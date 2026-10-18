from pathlib import Path
import json
import subprocess
import sys

root=Path(__file__).resolve().parent.parent
terminal=json.loads((root/'build/coherent-cpu-branch-v3/build/sanitized-v1/exit.json').read_text())
assert terminal['exit_code']==0 and terminal['native_executions']==180
source=(root/'build/run-maintained-pointer-differences.py').read_text()
edits=[
    ("CANDIDATE = ROOT / 'build/pointer-difference-maintained'", "CANDIDATE = ROOT / 'build/coherent-cpu-maintained'"),
    ("BASE = ROOT / 'build/pointer-difference-prototype'", "BASE = ROOT / 'build/coherent-cpu-branch-v3'"),
    ("'build/difference-sanitized-v1/stage/build'", "'build/sanitized-v1/stage/build'"),
    ('150 pointer difference checks passed;', '170 pointer difference checks passed;'),
    ('PASS 80 native pointer distances and 50 rejection contracts; 40 ordinary C executions separate;',
     'PASS 90 native pointer distances and 40 rejection contracts; 45 ordinary C executions separate;'),
    ('PASS 20 pointer-distance action/inverse circuits:', 'PASS 40 pointer-distance action/inverse circuits:'),
    ("'checks': 150, 'native_checks': 80, 'diagnostic_checks': 50, 'action_and_inverse_circuits': 20,",
     "'checks': 170, 'native_checks': 90, 'diagnostic_checks': 40, 'action_and_inverse_circuits': 40,"),
    ("'state_comparisons_inside_circuits': 40, 'ordinary_c_executions_separate': 40,",
     "'state_comparisons_inside_circuits': 80, 'ordinary_c_executions_separate': 45,"),
]
for before,after in edits:
    assert source.count(before)==1,before
    source=source.replace(before,after)
start=source.index("        'scope': 'Newly compiled maintained C validator,")
end=source.index("}\nexcept BaseException",start)
source=source[:start]+"        'scope': 'Newly compiled recurrent C validator on 137 frozen inputs with the scoped v3 normal/sanitizer compiler tools reused. Ninety native checks, forty specific diagnostics, forty complete action/inverse circuits and forty-five separate ordinary C references per configuration. The former CPU-scalar coherent-control rejection is now positive and a second phased fixture binds its explicit source-level distance-one precondition. This finite proof is separate from a full broad campaign and does not certify general memory lifetimes or all C11. No main application or cybersecurity change.'"+source[end:]
path=root/'build/run-coherent-maintained-checks.py'
assert not path.exists()
compile(source,str(path),'exec')
path.write_text(source)
subprocess.run([sys.executable,str(path)],cwd=root,check=True)
