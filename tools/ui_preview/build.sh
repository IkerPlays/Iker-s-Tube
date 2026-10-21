#!/bin/bash
# Host-side preview of the interface : compiles the real view classes against fake 3DS headers, draws sample screens into
# out/*.json and renders them to out/*.png with Python (Pillow + the Inter font). Run from this folder: ./build.sh && ./run.sh
cd "$(dirname "$0")"
S=../../source
mkdir -p out
g++ -std=gnu++17 -O0 -g -w -I fake -I $S $S/ui/views/view.cpp $S/ui/views/scroll.cpp $S/ui/views/vertical_list.cpp $S/variables.cpp $S/ui/nav_draw.cpp harness.cpp stub.cpp -o harness
