#!/usr/bin/env python3
"""Decide the ambiguous variable references in doc examples by running them.

repair-doc-examples.py restores every reference that exactly one declaration
explains. What it cannot settle is a bare `$` -- a one-character name eaten
whole -- when more than one such name is in scope: `dl_collapse $` with both
`a` and `b` around could be either.

Guessing by position does not work. It was tried, and it is wrong more often
than not: in dl_collapse the first bare `$` is `b` and the second is `a`, and
in dl_llist every one of them is `b` even though `a` is declared first.

But these transcripts record the output each command produced, which makes
the right answer checkable rather than arguable. So enumerate the possible
assignments, run each one, and keep the assignment that reproduces the
recorded output. An example that no assignment satisfies is left alone and
reported -- that is a different problem than an ambiguous name.

  solve-doc-examples.py <docs.db> <dlsh> <dlsh.zip> [--apply]
"""
import sys, re, os, base64, sqlite3, shutil, subprocess, itertools, collections

if len(sys.argv) < 4:
    sys.exit(__doc__)
db, dlsh, zipf = sys.argv[1:4]
apply_changes = '--apply' in sys.argv
HERE = os.path.dirname(os.path.abspath(__file__))
MAXCOMBOS = 2048

# Readings that running cannot settle, each checked by hand against the
# library and recorded here with what settles it. These are decisions, not
# guesses, and every one was confirmed in a live interpreter.
HAND = {
    # dl_collapse $b flattens to the four lists shown; dl_collapse $a then
    # raises exactly the recorded "bad operand (&a_3&)", since a is flat
    'dl_collapse':       (['b', 'a'], 'second call is meant to fail, on a'),
    # dl_tcllist $b is the three-element llist in the transcript, and
    # dl_append $b:1 4 reproduces the {2 3} {2 3 4} {2 3} that follows
    'dl_llist':          (['b', 'b', 'b'], 'a is the element, b the llist'),
    # dl_means [dl_llist $b] gives the recorded {{3.5 3.5} {2.5 2.0}}
    'dl_means':          (['b', 'b', 'b', 'b'], 'b is the nested list'),
    # a and b are the two string lists; their interleaving is the recorded
    # line. It no longer runs -- dl_interleave now rejects unequal lengths --
    # so this is drift in the library, not a doubt about the names.
    'dl_interleave':     (['a', 'b'], 'unequal-length interleave has since been rejected'),
    # x and y are the only lists, and the call draws without error
    'dlg_disjointLines': (['x', 'y'], 'the only two lists in scope'),
}

CMD = re.compile(r'^[ \t]*%[ \t]+(\S.*)$', re.M)
DECL = [re.compile(r'\bdl_local\s+([A-Za-z_][A-Za-z0-9_]*)'),
        re.compile(r'\bset\s+([A-Za-z_][A-Za-z0-9_]*)'),
        re.compile(r'\b(?:foreach|dl_foreach|dl_dotimes)\s+([A-Za-z_][A-Za-z0-9_]*)')]
BARE = re.compile(r'\$(?![A-Za-z0-9_{:])')


def declared(code):
    names = []
    for ln in code.split('\n'):
        m = CMD.match(ln)
        text = m.group(1) if m else ln.lstrip('%& \t')
        for pat in DECL:
            for hit in pat.findall(text):
                for n in hit.split():
                    if n not in names:
                        names.append(n)
    return names


def number_map(code):
    """&name_N& tokens seen intact, keyed by N.

    A dl_local return token carries the allocation number, so a damaged
    `&_3&` elsewhere in the same transcript is the same list as an intact
    `&a_3&`. That settles the tokens on output lines, which running cannot:
    the checker normalizes generated names away before comparing.
    """
    out = {}
    for name, n in re.findall(r'&([A-Za-z_][A-Za-z0-9_]*)_([0-9]+)&', code):
        out[n] = name
    return out


def variants(code):
    """Every candidate reading of this transcript, most-likely first.

    Each slot only offers the names already in scope where it appears, which
    is both correct -- a reference cannot be to a list declared later -- and
    what keeps the search small enough to run. dl_append has six slots and
    five names, but scoping cuts 15625 readings to 480.
    """
    lines = code.split('\n')
    seen, per_line = [], []
    for ln in lines:
        per_line.append([d for d in seen if len(d) == 1])
        m = CMD.match(ln)
        text = m.group(1) if m else ln.lstrip('%& \t')
        for pat in DECL:
            for hit in pat.findall(text):
                for n in hit.split():
                    if n not in seen:
                        seen.append(n)
        # a loop variable is bound for the body on its own line
        if re.search(r'\b(?:foreach|dl_foreach|dl_dotimes)\b', text):
            for pat in DECL:
                for hit in pat.findall(text):
                    for n in hit.split():
                        if len(n) == 1 and n not in per_line[-1]:
                            per_line[-1].append(n)

    cands = [d for d in declared(code) if len(d) == 1]
    slot_names, total = [], 1
    for i, ln in enumerate(lines):
        for _ in BARE.findall(ln):
            names = per_line[i] or cands
            # later-declared first: a lone `$` usually means the list the
            # example has just built up to, not the first one it introduced
            slot_names.append(list(reversed(names)))
            total *= len(names)
    if not slot_names or total > MAXCOMBOS:
        return cands, len(slot_names), []

    out = []
    for combo in itertools.product(*slot_names):
        it = iter(combo)
        out.append((combo, BARE.sub(lambda m: '$' + next(it), code)))
    return cands, len(slot_names), out


def b64(s):
    return base64.b64encode(s.encode('utf-8')).decode('ascii')


con = sqlite3.connect(db)
rows = con.execute("""select x.id, e.slug, coalesce(e.namespace,''),
                             coalesce(x.code,'')
                        from examples x join entries e on e.id = x.entry_id
                       order by e.slug""").fetchall()

work, skipped = [], []
for xid, slug, ns, code in rows:
    if not CMD.search(code) or not BARE.search(code):
        continue
    cands, slots, vs = variants(code)
    if not vs:
        skipped.append((slug, 'no one-character name in scope' if not cands
                        else '%d slots x %d names is too many' % (slots, len(cands))))
        continue
    work.append((xid, slug, ns, code, cands, vs))

print('  transcripts with an undecided reference : %d' % (len(work) + len(skipped)))
print('  solvable by running                     : %d' % len(work))

# one file holding every variant of every example, run in a single pass
with open('/tmp/solve_variants.tcl', 'w') as fh:
    fh.write('set ::DOC_EXAMPLES {}\n')
    for _, slug, ns, _, _, vs in work:
        for i, (_, text) in enumerate(vs):
            fh.write('lappend ::DOC_EXAMPLES [list %s %s %s]\n'
                     % (b64('%s#%d' % (slug, i)), b64(ns), b64(text)))

env = dict(os.environ, DLSH_ZIP=zipf)
out = subprocess.run([dlsh, os.path.join(HERE, 'score-doc-variants.tcl'),
                      '/tmp/solve_variants.tcl'],
                     capture_output=True, text=True, env=env).stdout
score = {}
for line in out.split('\n'):
    f = line.split()
    if len(f) == 4 and f[1].isdigit():
        score[f[0]] = (int(f[1]), int(f[2]), int(f[3]))


def rank(slug, i, combo, decl):
    '''Best reading: most recorded lines reproduced, then fewest errors, then
    the one that follows declaration order -- among readings that do equally
    well, `$a $b` is likelier to be what was written than `$b $a`.'''
    m, c, e = score.get('%s#%d' % (slug, i), (0, 0, 99))
    idx = [decl.index(x) if x in decl else 99 for x in combo]
    inversions = sum(1 for j in range(len(idx)) for k in range(j + 1, len(idx))
                     if idx[j] > idx[k])
    return (-m, e, inversions, i)

solved, unsolved, handed, updates = [], [], [], []
for xid, slug, ns, code, cands, vs in work:
    ranked = sorted(range(len(vs)), key=lambda i: rank(slug, i, vs[i][0], cands))
    best = ranked[0]
    m, c, e = score.get('%s#%d' % (slug, best), (0, 0, 99))
    # a reading has to actually explain something, and has to be better than
    # its rivals; a tie means the transcript does not distinguish them
    rivals = [i for i in ranked[1:]
              if score.get('%s#%d' % (slug, i), (0, 0, 99))[0] == m
              and set(vs[i][0]) != set(vs[best][0])]
    if m == 0 or (rivals and m < c):
        if slug in HAND:
            names, why = HAND[slug]
            it = iter(names)
            combo = tuple(names)
            text = BARE.sub(lambda mm: '$' + next(it), code)
            handed.append((slug, combo, why))
        else:
            unsolved.append((slug, len(vs), m, c,
                             'tie' if rivals else 'nothing matched'))
            continue
    else:
        combo, text = vs[best]
    nm = number_map(code)
    text = re.sub(r'&_([0-9]+)&',
                  lambda m: '&%s_%s&' % (nm[m.group(1)], m.group(1))
                  if m.group(1) in nm else m.group(0), text)
    if slug not in [h[0] for h in handed]:
        solved.append((slug, combo, m, c))
    updates.append((xid, text))

print('  decided by matching recorded output     : %d' % len(solved))
print('  no reading reproduces the output        : %d' % len(unsolved))
print()
for slug, combo, m, c in solved:
    print('    %-22s %-18s reproduces %d/%d recorded lines'
          % (slug, ' '.join('$' + x for x in combo), m, c))
if handed:
    print('\n  decided by hand (checked in a live interpreter):')
    for slug, combo, why in handed:
        print('    %-22s %-18s %s' % (slug, ' '.join('$' + x for x in combo), why))
if unsolved:
    print('\n  undecided (needs a person):')
    for slug, n, m, c, why in unsolved:
        print('    %-22s %d readings, best %d/%d (%s)' % (slug, n, m, c, why))
if skipped:
    print('\n  not attempted:')
    for slug, why in skipped:
        print('    %-22s %s' % (slug, why))

if apply_changes:
    shutil.copy(db, db + '.solve.bak')
    con.executemany('update examples set code = ? where id = ?',
                    [(t, i) for i, t in updates])
    con.commit()
    print('\napplied %d to %s  (previous copy at %s.solve.bak)'
          % (len(updates), db, db))
else:
    print('\ndry run -- nothing written. Re-run with --apply to write.')
con.close()
