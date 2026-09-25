#!/usr/bin/env bash
# Optional: extract already downloaded Ubuntu development packages locally.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .deps/gcc
for package in gcc-*-plugin-dev_*.deb libgmp-dev_*.deb libmpfr-dev_*.deb libmpc-dev_*.deb; do
    [[ -f "$package" ]] && dpkg-deb -x "$package" .deps/gcc
done

