#!/bin/bash
cd "$(dirname "$0")"
S=../../source
g++ -std=gnu++17 -O0 -w -I fake -I $S $S/ui/views/view.cpp $S/ui/views/scroll.cpp $S/ui/views/vertical_list.cpp $S/variables.cpp $S/ui/nav_draw.cpp input_test.cpp stub.cpp -o input_test && ./input_test
