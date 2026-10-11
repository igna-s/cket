#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
build/cket_tests "$PWD"
exec build/test_examples
