#!/usr/bin/env python3
"""Compare per-test results of several runs.
usage: cmp_runs.py RUN1 RUN2 ...   where each RUN is a log containing 'RUN/OK/FAIL/SKIP' lines under
'== AREA <n>' markers (a runner transcript), or a results directory holding <area>.txt files.
Prints the tests whose status differs between runs, and per-area pass counts. Exit 1 if any differ."""
import sys, os, re, glob
def load(p):
    res = {}
    def feed(area, lines):
        cur = area
        for l in lines:
            m = re.match(r'^(OK  |FAIL|SKIP) (\S+)\s*$', l)
            if m: res[(cur, m.group(2))] = m.group(1).strip()
    if os.path.isdir(p):
        for f in glob.glob(os.path.join(p, '*.txt')):
            feed(os.path.basename(f)[:-4], open(f, errors='replace').read().splitlines())
    else:
        area = '?'; buf = {}
        for l in open(p, errors='replace').read().splitlines():
            m = re.match(r'^== AREA (\S+)', l)
            if m: area = m.group(1); continue
            feed(area, [l])
    return res
runs = [load(p) for p in sys.argv[1:]]
keys = sorted(set().union(*[set(r) for r in runs]))
diff = [k for k in keys if len({r.get(k, 'MISSING') for r in runs}) > 1]
areas = sorted({k[0] for k in keys})
print('%-20s ' % 'area' + ' '.join('%8s' % ('run%d' % (i+1)) for i in range(len(runs))))
for a in areas:
    print('%-20s ' % a + ' '.join('%8s' % ('%d/%d' % (sum(1 for k, v in r.items() if k[0] == a and v != 'FAIL'), sum(1 for k in r if k[0] == a))) for r in runs))
print('differing tests: %d' % len(diff))
for k in diff: print('  %s/%s: %s' % (k[0], k[1], ' '.join(r.get(k, 'MISSING') for r in runs)))
sys.exit(1 if diff else 0)
