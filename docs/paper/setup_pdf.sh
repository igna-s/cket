#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
python3 -m venv .deps/paper-venv
.deps/paper-venv/bin/python -m pip install --disable-pip-version-check -r docs/paper/requirements-paper.txt
mkdir -p .deps/paper-poppler
cd .deps/paper-poppler
if apt-get download poppler-utils; then
    for package in poppler-utils*.deb; do dpkg-deb -x "$package" .; done
    ./usr/bin/pdftoppm -v || true
fi
# build_paper.py also renders every page with PyMuPDF for visual inspection.
