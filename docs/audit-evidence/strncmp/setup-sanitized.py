"""Instrument the candidate frontend and reuse byte-identical instrumented core."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
candidate = root / 'build/strncmp-prototype'
sanitized = candidate / 'build/sanitized'
integrated = '--integrated' in sys.argv
sanitized.mkdir(exist_ok=integrated)
binary = sanitized / 'build'
binary.mkdir(exist_ok=integrated)
final = json.loads((root / 'build/memcmp-verified-sources.json').read_text())
for name, digest in final.items():
    if integrated and not name.startswith(('src/', 'include/')):
        continue
    if name in ('src/quantum_memory.inc', 'include/string.h'):
        continue
    assert hashlib.sha256((candidate / name).read_bytes()).hexdigest() == digest, name
    assert hashlib.sha256((root / name).read_bytes()).hexdigest() == digest, name
for directory in ('include', 'examples'):
    link = sanitized / directory
    if link.exists():
        assert integrated and link.resolve() == (candidate / directory).resolve()
    else:
        link.symlink_to(candidate / directory, target_is_directory=True)
shutil.copy2(candidate / 'cket', sanitized / 'cket')
copied = {}
for name in ('cket', 'cket_backend', 'cket_runtime.o', 'libcket_quantum.a', 'gcc-wrapper', 'gcc-version.txt'):
    source = root / 'build/sanitized/build' / name
    assert source.stat().st_mtime_ns >= (root / 'build/memcmp-verified-sources.json').stat().st_mtime_ns, name
    shutil.copy2(source, binary / name)
    copied[name] = hashlib.sha256(source.read_bytes()).hexdigest()
plugin = subprocess.check_output(['gcc-11', '-print-file-name=plugin'], text=True).strip()
headers = Path(plugin) / 'include'
if not (headers / 'gcc-plugin.h').exists():
    headers = candidate / '.deps/gcc' / str(headers).lstrip('/')
assert (headers / 'gcc-plugin.h').exists()
extra = []
if (candidate / '.deps/gcc/usr/include').is_dir():
    extra = ['-I.deps/gcc/usr/include', '-I.deps/gcc/usr/include/x86_64-linux-gnu']
plugin_command = ['g++-11', '-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Wno-unused-parameter', '-Wno-format-diag', '-fPIC', '-shared', '-fno-rtti', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I' + str(headers), *extra, 'src/cket_plugin.cpp', '-o', str(binary / 'cket_plugin.so')]
subprocess.run(plugin_command, cwd=candidate, check=True, timeout=180)
flags = ['gcc-11', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Isrc/engine', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
subprocess.run([*flags, 'tests/validate_strncmp.c', 'tests/validation.c', str(binary / 'libcket_quantum.a'), '-lm', '-o', str(binary / 'validate_strncmp')], cwd=candidate, check=True, timeout=120)
subprocess.run([*flags, 'tests/validate_memcmp.c', 'tests/validation.c', str(binary / 'libcket_quantum.a'), '-lm', '-o', str(binary / 'validate_memcmp')], cwd=candidate, check=True, timeout=120)
prefix = 'strncmp-integrated' if integrated else 'strncmp-candidate'
setup = {'scope': 'Candidate plugin rebuilt with ASan/UBSan. Engine, driver, runtime and wrapper reused from the active main-tree instrumented build after verifying corresponding src/include hashes and build timestamps; only the two frontend/header files differ among src/include inputs. Integration-mode tests, scripts and CI changes belong to the supplied source snapshot.', 'reused_binary_sha256': copied, 'new_plugin_sha256': hashlib.sha256((binary / 'cket_plugin.so').read_bytes()).hexdigest(), 'source_manifest': 'build/strncmp-integrated-sources.json' if integrated else 'build/strncmp-candidate-sources.json'}
(candidate / 'build' / (prefix + '-sanitized-setup.json')).write_text(json.dumps(setup, indent=2) + '\n')
environment = dict(os.environ, CC=str(binary / 'gcc-wrapper'), CKET_REAL_GCC='gcc-11', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
for name in ('validate_strncmp', 'validate_memcmp'):
    with (root / 'build' / (prefix + '-' + name + '-sanitized.log')).open('x') as log:
        subprocess.run([str(binary / name)], cwd=candidate, env=environment, stdout=log, stderr=subprocess.STDOUT, check=True)
print('Candidate strncmp and memcmp validators passed with ASan/UBSan, including the instrumented candidate plugin and generated C programs.')
