#!/bin/bash
cd "$(dirname "$0")"
./harness && for f in out/*.json; do python3 render.py "$f" "${f%.json}.png"; done
echo "Pictures are in tools/ui_preview/out/"
