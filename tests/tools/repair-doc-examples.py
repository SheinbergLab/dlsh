#!/usr/bin/env python3
"""Repair the truncated variable references in docs.db example transcripts.

At some point in the history of these docs -- most likely a wiki import --
exactly one character was deleted immediately after every `$` and every `&`
in the example code. The declarations themselves were untouched, because a
name in `dl_local rands ...` does not follow a sigil:

    % dl_local rands [dl_zrand 1000]      <- intact
    &ands_0&                              <- was &rands_0&
    % dl_local hvals [dl_hist $ands ...]  <- was $rands
    &vals_1&                              <- was &hvals_1&

That asymmetry is what makes the damage repairable: every example declares
its own variables in plain text, so a reference can be matched back to the
declaration it must have come from.

Resolution is deliberately conservative. A reference is rewritten only when
exactly one declared name explains it; anything ambiguous is reported for
hand repair rather than guessed at. In particular a bare `$` (a one-character
name, eaten whole) is only restored when a single one-character name is in
scope -- `dl_append $ $` is `$a $i`, and no amount of string matching can
know that.

  repair-doc-examples.py <docs.db>            dry run: unified diffs + summary
  repair-doc-examples.py <docs.db> --apply    write, after backing up to .bak
  repair-doc-examples.py <docs.db> --slug X   restrict to one entry
  repair-doc-examples.py <docs.db> --infer    also attribute ambiguous bare
                                              `$` by position (a heuristic --
                                              counted and reported apart)
"""
import sys, re, sqlite3, shutil, difflib, collections

args = sys.argv[1:]
if not args:
    sys.exit(__doc__)
db = args[0]
apply_changes = '--apply' in args
only = args[args.index('--slug') + 1] if '--slug' in args else None
infer = '--infer' in args
verbose = '-v' in args

# A transcript line is a command only when the % prompt is followed by
# whitespace. Output lines can themselves start with % -- a generated list
# name prints as %list0% -- and must not be read as commands.
CMD = re.compile(r'^[ \t]*%[ \t]+(\S.*)$', re.M)

# Every form that binds a name. Loop and proc bindings matter as much as
# dl_local: `dl_dotimes i 10 { puts $ }` is `$i`, and `proc f { arg }` is
# where the `$rg` in dl_datatype comes from.
DECL = [
    re.compile(r'\bdl_local\s+([A-Za-z_][A-Za-z0-9_]*)'),
    re.compile(r'\bset\s+([A-Za-z_][A-Za-z0-9_]*)'),
    re.compile(r'\b(?:foreach|dl_foreach|dl_dotimes)\s+([A-Za-z_][A-Za-z0-9_]*)'),
    re.compile(r'\b(?:foreach|dl_foreach)\s+\{([^}]*)\}'),
    re.compile(r'\bproc\s+[A-Za-z_:][A-Za-z0-9_:]*\s*\{([^}]*)\}'),
    re.compile(r'\bfor\s*\{\s*set\s+([A-Za-z_][A-Za-z0-9_]*)'),
]


def declared_by_line(lines):
    """Names visible at each line index, in declaration order.

    Every line is scanned, not just prompted ones: a handful of examples have
    a declaration whose prompt is missing or was itself replaced by an `&`
    (dl_abs opens with `&dl_local a [...]`), and a wrapped command continues
    onto unprompted lines. Declarations are what make the repair possible, so
    it is worth finding all of them.
    """
    seen, out = [], []
    for ln in lines:
        m = CMD.match(ln)
        text = m.group(1) if m else ln.lstrip('%& \t')
        names = []
        for pat in DECL:
            for hit in pat.findall(text):
                names.extend(hit.split())
        # A loop or proc binds its variable for the body on that same line --
        # `foreach i { 0 4 8 } { dl_append $ $ }` is `$a $i`, so `i` has to be
        # in scope here. An assignment does not: it is still being computed.
        binders = [n for n in names
                   if re.search(r'\b(?:foreach|dl_foreach|dl_dotimes|proc|for)\b', text)]
        out.append(list(seen) + [b for b in binders if b not in seen])
        for name in names:
            if name not in seen:
                seen.append(name)
    return out, seen


def resolve(trunc, scope, allnames):
    """The declared name a truncated reference must have come from.

    Returns (name, reason). name is None when the reference cannot be
    attributed to exactly one declaration.
    """
    pool = scope or allnames
    if trunc == '':                                  # $ or & -- name eaten whole
        cands = [d for d in pool if len(d) == 1]
        if len(cands) == 1:
            return cands[0], 'sole one-character name in scope'
        return None, ('no one-character name in scope' if not cands
                      else 'ambiguous: %s' % ', '.join('$' + c for c in cands))
    if trunc in pool:                                # never corrupted
        return trunc, 'intact'
    cands = [d for d in pool if d[1:] == trunc]
    if len(cands) == 1:
        return cands[0], 'unique suffix match'
    if len(cands) > 1:
        return None, 'ambiguous: %s' % ', '.join(cands)
    return None, 'no declaration explains it'


def repair(code, infer=False):
    lines = code.split('\n')
    scopes, allnames = declared_by_line(lines)
    out, fixed, inferred, unresolved = [], 0, 0, []

    # name declared by the nearest preceding `% dl_local NAME ...` line
    prev_local, last = [], None
    for ln in lines:
        prev_local.append(last)
        m = CMD.match(ln)
        if m:
            d = re.search(r'\bdl_local\s+([A-Za-z_][A-Za-z0-9_]*)', m.group(1))
            last = d.group(1) if d else None

    for i, ln in enumerate(lines):
        scope = scopes[i]
        echoed = prev_local[i]
        nth = [0]          # which ambiguous bare reference this is on the line

        def guess(trunc, why):
            """Second tier: attribute an ambiguous bare reference by position.

            `dl_and $ $` with a and b in scope is `$a $b` -- the k-th eaten
            name is the k-th candidate in declaration order. This is a
            heuristic, not a deduction, so it is off unless --infer is given
            and every use is counted and reported separately.
            """
            if not (infer and trunc == '' and why.startswith('ambiguous')):
                return None
            cands = [d for d in (scope or allnames) if len(d) == 1]
            k = nth[0]
            nth[0] += 1
            return cands[k] if k < len(cands) else (cands[-1] if cands else None)

        def sub_dollar(m):
            nonlocal fixed, inferred
            trunc = m.group(1) or ''
            name, why = resolve(trunc, scope, allnames)
            if name is None:
                g = guess(trunc, why)
                if g is None:
                    unresolved.append(('$' + trunc, why, ln.strip()))
                    return m.group(0)
                inferred += 1
                return '$' + g
            if name == trunc:
                return m.group(0)
            fixed += 1
            return '$' + name

        def sub_amp(m):
            nonlocal fixed, inferred
            trunc, n = m.group(1) or '', m.group(2)
            # A &name_N& token on an output line is the value echoed back by
            # the dl_local just above it, so the declaration is known outright
            # -- no matching needed, and it settles cases that would otherwise
            # look ambiguous (`&_4&` under `dl_local b` is `&b_4&`).
            if echoed and not CMD.match(ln) and (trunc == '' or echoed[1:] == trunc):
                if echoed != trunc:
                    fixed += 1
                    return '&%s_%s&' % (echoed, n)
                return m.group(0)
            name, why = resolve(trunc, scope, allnames)
            if name is None:
                unresolved.append(('&%s_%s&' % (trunc, n), why, ln.strip()))
                return m.group(0)
            if name == trunc:
                return m.group(0)
            fixed += 1
            return '&%s_%s&' % (name, n)

        # &name_N& return tokens appear on output lines; $refs on both
        ln = re.sub(r'&([A-Za-z_][A-Za-z0-9_]*)?_([0-9]+)&', sub_amp, ln)
        ln = re.sub(r'\$([A-Za-z_][A-Za-z0-9_]*)?', sub_dollar, ln)
        out.append(ln)

    return '\n'.join(out), fixed, inferred, unresolved


con = sqlite3.connect(db)
rows = con.execute("""select x.id, e.slug, coalesce(x.code,'')
                        from examples x join entries e on e.id = x.entry_id
                       order by e.slug""").fetchall()

n_seen = n_changed = n_fixed = n_inferred = 0
needs_hand = []
reasons = collections.Counter()
updates = []

for xid, slug, code in rows:
    if only and slug != only:
        continue
    if not CMD.search(code or ''):
        continue
    n_seen += 1
    new, fixed, inf, unresolved = repair(code, infer)
    if unresolved:
        needs_hand.append((slug, unresolved))
        for _, why, _ in unresolved:
            reasons[why.split(':')[0]] += 1
    if new != code:
        n_changed += 1
        n_fixed += fixed
        n_inferred += inf
        updates.append((xid, new))
        if not apply_changes:
            diff = difflib.unified_diff(code.split('\n'), new.split('\n'),
                                        '%s (current)' % slug,
                                        '%s (repaired)' % slug, lineterm='', n=1)
            body = [d for d in diff][2:]
            if body:
                print('--- %s' % slug)
                for d in body:
                    print('   %s' % d)
                print()

print('=' * 66)
print('transcripts examined            : %d' % n_seen)
print('transcripts changed             : %d' % n_changed)
print('references restored (certain)    : %d' % n_fixed)
if infer:
    print('references inferred (positional): %d' % n_inferred)
print('transcripts still needing hands : %d' % len(needs_hand))
for why, n in reasons.most_common():
    print('    %-34s %d' % (why, n))

if needs_hand and verbose:
    print('\nunresolved references:')
    for slug, items in needs_hand:
        print('  %s' % slug)
        for ref, why, ctx in items[:4]:
            print('    %-10s %-34s %s' % (ref, why, ctx[:44]))

if apply_changes:
    shutil.copy(db, db + '.bak')
    con.executemany('update examples set code = ? where id = ?',
                    [(new, xid) for xid, new in updates])
    con.commit()
    print('\napplied to %s  (previous copy at %s.bak)' % (db, db))
else:
    print('\ndry run -- nothing written. Re-run with --apply to write.')
con.close()
