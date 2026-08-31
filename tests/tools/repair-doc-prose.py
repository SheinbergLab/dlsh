#!/usr/bin/env python3
"""Turn the LaTeX left in docs.db prose into the markup the viewer renders.

The command reference began life as a LaTeX document, went through a wiki,
and came out with its macros intact but its backslashes escaped -- twice over,
in two different wiki spellings:

    %%\\%%dlsh%%\\%%                    ->  \\dlsh\\
    <nowiki> \\</nowiki>dlsh<nowiki>\\</nowiki>  ->  \\dlsh\\

Unescape those and what is underneath is ordinary LaTeX: \\dlsh for the
product name, {\\tt x} for code, {\\em x} for emphasis, ``x'' for quotes, \\par
for a paragraph break. None of it means anything to the HTML viewer, which
renders it literally.

The macro set is small and closed -- eight of them across 29 entries -- so
each is translated explicitly rather than by a general LaTeX parser, and
anything not recognised is reported instead of being dropped. Deleting markup
you have not identified is how prose quietly loses a word.

  repair-doc-prose.py <docs.db>            dry run: diffs and a summary
  repair-doc-prose.py <docs.db> --apply    write, after backing up to .bak
"""
import sys, re, sqlite3, shutil, difflib, collections

args = sys.argv[1:]
if not args:
    sys.exit(__doc__)
db = args[0]
apply_changes = '--apply' in args
verbose = '-v' in args

FIELDS = ('content', 'summary', 'syntax')


def unescape(t):
    """Undo the wiki's two spellings of an escaped backslash."""
    t = t.replace('%%\\%%', '\\')
    t = re.sub(r'</?nowiki>', '', t)
    # a couple of the nowiki spans hold an escaped backslash rather than a
    # bare one, leaving {\\em ...} where the rest of the file has {\em ...}
    t = t.replace('\\\\', '\\')
    return t


def delatex(t, field='content'):
    # \dlsh is the product name; the trailing \  is LaTeX's forced space
    t = re.sub(r'\\dlsh\\(?=\s)', 'dlsh', t)
    t = re.sub(r'\\dlsh\\?', 'dlsh', t)
    # {\tt x} is code, {\em x} and {\it x} are emphasis. The space after the
    # macro is sometimes gone -- {\emcolour} -- eaten by the same import that
    # took a character after every $ and &, so allow for it.
    t = re.sub(r'\{\\tt\s*([^{}]*?)\s*\}', lambda m: '`%s`' % m.group(1), t)
    emph = (lambda m: m.group(1)) if field == 'syntax' else (lambda m: '*%s*' % m.group(1))
    t = re.sub(r'\{\\(?:em|it)\s*([^{}]*?)\s*\}', emph, t)
    # {\.dgz} is a {\tt .dgz} that lost its tt the same way
    t = re.sub(r'\{\\(\.[^{}]*?)\}', lambda m: '`%s`' % m.group(1), t)
    t = re.sub(r'\\prompt\s*', '% ', t)          # the interpreter prompt
    t = re.sub(r'\\par\b\s*', '\n\n', t)         # paragraph break
    t = re.sub(r'``\s*(.*?)\s*\'\'', lambda m: '"%s"' % m.group(1), t)
    # one opening quote came through as a single backtick: `entry''
    t = re.sub(r'`([^`\n]{1,40})\'\'', lambda m: '"%s"' % m.group(1), t)
    # inline math: no renderer here, so unwrap it and spell the letter out
    t = re.sub(r'\\frac\{([^{}]*)\}\{([^{}]*)\}',
               lambda m: '(%s)/(%s)' % (m.group(1), m.group(2)), t)
    # Only unwrap a $...$ span that is actually math. Tcl prose is full of
    # `$xy1 $xy2`, which looks exactly like inline math to a naive pattern and
    # loses its dollar signs -- so require a LaTeX token inside the span.
    t = re.sub(r'\$((?=[^$\n]*(?:\\|_\{|\^\{))[^$\n]{1,60})\$',
               lambda m: m.group(1), t)
    t = t.replace('\\alpha', 'alpha')
    if field == 'syntax':
        # A signature is a code span, so emphasis inside it is noise, and the
        # `{\tt plot}` that lost its opening left a stray brace behind:
        #     `plot} markers {\em dataset [args]`  ->  `plot markers dataset [args]`
        t = re.sub(r'\{\\(?:em|it)\s*([^{}]*?)\s*\}', lambda m: m.group(1), t)
        # ... and the matching {\em dataset [args]} lost its closing brace to
        # the same damage, so it runs to the end of the code span
        t = re.sub(r'\{\\(?:em|it)\s*([^{}`]*)', lambda m: m.group(1), t)
        t = re.sub(r'`([A-Za-z_][A-Za-z0-9_]*)\}', lambda m: '`' + m.group(1), t)
        # \\ is a LaTeX line break. It separates two signatures in some of
        # these fields and merely wraps one in others -- a following command
        # name is what tells them apart.
        t = re.sub(r'\\[ \t]+(?=(?:dl|dg|dlp|dlg)_)', '\\n', t)
        t = re.sub(r'\\[ \t]+', ' ', t)
        t = re.sub(r'[ \t]{2,}', ' ', t)
    t = re.sub(r'\\(?=\s)', '', t)               # forced space
    return t


# A factual error, not markup: dl_gaussian's recorded formula does not match
# fitfuncs.tcl, where amp multiplies and the exponent is fixed at 2.
FORMULA = [('dl_gaussian',
            'e^{-((x-mean)/(sd))^{amp}}',
            'amp * e^(-((x-mean)/sd)^2)')]

LEFTOVER = re.compile(r'\\[A-Za-z]+|%%|<nowiki>|``|\'\'')

con = sqlite3.connect(db)
rows = con.execute("""select id, slug, coalesce(content,''), coalesce(summary,''),
                             coalesce(syntax,'') from entries order by slug""").fetchall()

changed, updates, residue = 0, [], collections.Counter()
for eid, slug, *vals in rows:
    new = []
    for f, v in zip(FIELDS, vals):
        t = delatex(unescape(v), f)
        for s, old, rep in FORMULA:
            if s == slug:
                t = t.replace(old, rep)
        new.append(t)
    if new == list(vals):
        continue
    changed += 1
    updates.append((eid, slug, new))
    for t in new:
        for m in LEFTOVER.findall(t):
            residue[m] += 1
    if not apply_changes:
        for f, before, after in zip(FIELDS, vals, new):
            if before == after:
                continue
            d = [x for x in difflib.unified_diff(before.split('\n'), after.split('\n'),
                                                 '%s.%s' % (slug, f), '', lineterm='', n=0)][2:]
            if d:
                print('--- %s.%s' % (slug, f))
                for line in d:
                    print('   %s' % line[:110])
print('=' * 68)
print('entries rewritten : %d' % changed)
if residue:
    print('\nmarkup left behind (not recognised, so not touched):')
    for m, n in residue.most_common():
        print('    %-14s %d' % (m, n))
else:
    print('no unrecognised markup remains')

if apply_changes:
    shutil.copy(db, db + '.prose.bak')
    con.executemany('update entries set content=?, summary=?, syntax=? where id=?',
                    [(c, s, y, i) for i, _, (c, s, y) in updates])
    con.commit()
    print('\napplied to %s  (previous copy at %s.prose.bak)' % (db, db))
else:
    print('\ndry run -- nothing written. Re-run with --apply to write.')
con.close()
