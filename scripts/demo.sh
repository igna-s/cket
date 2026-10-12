#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
build/cket examples/find.c --emit-ast -o build/demo.ast
build/cket examples/find.c --emit-ir -o build/demo.qir.json
build/cket examples/find.c --emit-qasm -o build/demo.qasm
build/cket examples/find.c --resources -o build/demo.resources.txt
build/cket examples/find.c --run --seed 7 -o build/demo
cat build/demo.resources.txt
