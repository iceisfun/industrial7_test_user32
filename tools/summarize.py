#!/usr/bin/env python3
"""Aggregate results/<area>.txt (or a directory given as argv[1]).
If results/<area>.tsv exists (written by the harness) it is the source of truth for per-test status."""
import sys, glob, os, re
d = sys.argv[1] if len(sys.argv) > 1 else 'results'
tot = [0]*5
print('%-20s %6s %6s %6s %8s %8s' % ('area','tests','failed','skip','chk_ok','chk_fail'))
for f in sorted(glob.glob(os.path.join(d, '*.txt'))):
    if os.path.basename(f).startswith('_'): continue
    lines = open(f, errors='replace').read().splitlines()
    n = os.path.basename(f)[:-4]
    s = [l for l in lines if l.startswith('SUMMARY ')]
    m = re.search(r'tests=(\d+) failed=(\d+) skipped=(\d+) checks_pass=(\d+) checks_fail=(\d+)', s[-1]) if s else None
    if not m: print('%-20s NO SUMMARY (crash/hang?)' % n); continue
    v = [int(x) for x in m.groups()]
    tot = [a+b for a,b in zip(tot,v)]
    print('%-20s %6d %6d %6d %8d %8d' % ((n,)+tuple(v)))
    for l in lines:
        if l.startswith('FAIL '): print('    ' + l)
print('%-20s %6d %6d %6d %8d %8d' % (('TOTAL',)+tuple(tot)))
