#!/bin/bash
cd "$(dirname "$0")"
S=../../source
g++ -std=gnu++17 -O0 -Wall -I $S text_test.cpp -o text_test && ./text_test
