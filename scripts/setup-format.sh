#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

python3 -m venv .deps/format-venv
.deps/format-venv/bin/python -m pip install -r scripts/requirements-format.txt

# Keep the formatter local; it is not a compiler or runtime dependency.
tools_dir="$PWD/.deps/format-tools"
mkdir -p "$tools_dir"
case "$(uname -m)" in
    x86_64)
        platform=linux_amd64
        checksum=1f57a384d59542f8fac5f503da1f3ea44242f46dff969569e80b524d64b71dbc
        ;;
    aarch64)
        platform=linux_arm64
        checksum=9d23013d56640e228732fd2a04a9ede0ab46bc2d764bf22a4a35fb1b14d707a8
        ;;
    *)
        echo "The formatter setup supports Linux/WSL on x86_64 or aarch64." >&2
        exit 1
        ;;
esac
curl --fail --location --retry 2 --max-time 120 \
    "https://github.com/mvdan/sh/releases/download/v3.10.0/shfmt_v3.10.0_$platform" \
    -o "$tools_dir/shfmt"
printf '%s  %s\n' "$checksum" "$tools_dir/shfmt" | sha256sum --check --status
chmod +x "$tools_dir/shfmt"
echo "Formatting tools installed under .deps/."
