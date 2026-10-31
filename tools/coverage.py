#!/usr/bin/env python3
"""Generate COVERAGE.md: which named user32 exports are referenced by tests."""
import re, glob, collections
names = [l.strip() for l in open('tools/user32_exports.txt') if l.strip() and not l.startswith('#')]
names = [n.split()[-1] for n in names]
ref = collections.defaultdict(set); fl = collections.defaultdict(set)
for f in glob.glob('tests/*/*.c') + glob.glob('tests/*/*.h'):
    area = f.split('/')[1]
    txt = open(f, errors='replace').read()
    ids = set(re.findall(r'[A-Za-z_][A-Za-z0-9_]*', txt))
    for n in names:
        if n in ids or (n + '_pfn') in ids: ref[n].add(area); fl[n].add(f)
weak = ('tests/strings_misc/exports_data.c', 'tests/strings_misc/exports.c', 'tests/gap_01/exports.c', 'tests/gap_03/stubs.c')
unc = [n for n in names if n not in ref or all(x in weak for x in fl[n])]
cov = [n for n in names if n in ref and n not in unc]
out = ['# user32 export coverage', '',
       f'Named exports: {len(names)}; functionally exercised: {len(cov)}; presence/stub-only or unreferenced: {len(unc)}.',
       'Ordinal-only exports (~160) are not tracked here.', '',
       '## Not functionally covered (export-presence / stub checks only, or unreferenced)', '',
       'The weak files only verify the export exists / is callable safely. Reasons: unsafe on a live desktop (input injection, session/desktop switching, shutdown, lock), undocumented/',
       'internal with no stable contract, or thin kernel forwarders awaiting tests.', '']
out += ['- ' + n for n in unc]
out += ['', '## Covered (export: areas)', '']
out += [f'- {n}: {", ".join(sorted(ref[n]))}' for n in cov]
open('COVERAGE.md', 'w').write('\n'.join(out) + '\n')
print(len(names), len(cov), len(unc))
