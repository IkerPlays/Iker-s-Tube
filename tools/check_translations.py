#!/usr/bin/env python3
"""Checks source/data_io/translations.cpp against the tr("English", "Spanish") calls of the sources.
Prints the English texts that have no row (they would show in English in fr/de/it/pt/ja), duplicate keys and rows with a wrong
number of strings. Exit code 1 when something is wrong. Run from anywhere: python3 tools/check_translations.py"""
import os, re, subprocess, sys
from collections import Counter
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
src = subprocess.run(["grep", "-rhoE", r'tr\("([^"\\]|\\.)*", *"([^"\\]|\\.)*"\)', "source", "--include=*.cpp", "--include=*.hpp"],
                     cwd=root, capture_output=True, text=True).stdout.split("\n")
keys = set()
for line in src:
    m = re.match(r'tr\("((?:[^"\\]|\\.)*)",', line)
    if m:
        keys.add(m.group(1))
table = open(os.path.join(root, "source/data_io/translations.cpp"), encoding="utf-8").read()
rows = re.findall(r'^\t\{"((?:[^"\\]|\\.)*)", \{(.*)\}\},$', table, re.M)
have = [k for k, _ in rows]
missing = sorted(k for k in keys if k not in set(have))
dups = sorted(k for k, c in Counter(have).items() if c > 1)
bad = [k for k, body in rows if len(re.findall(r'"(?:[^"\\]|\\.)*"', body)) != 5]
print(f"{len(keys)} texts in the sources, {len(rows)} rows")
for title, items in (("missing", missing), ("duplicate", dups), ("not 5 strings", bad)):
    if items:
        print(title + ":")
        for k in items:
            print("  ", repr(k))
sys.exit(1 if (missing or dups or bad) else 0)
