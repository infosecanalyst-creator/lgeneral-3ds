#!/usr/bin/env python3
"""docs/manual.txt -> platform/dsui/dsui_manual.h  (pages start with '== TITLE'; edit the .txt, then run this)"""
import re, sys, os
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = open(os.path.join(root, 'docs', 'manual.txt'), encoding='utf-8').read()
pages = []
for block in src.split('\n== ')[0:]:
    block = block[3:] if block.startswith('== ') else block
    lines = block.rstrip('\n').split('\n')
    pages.append((lines[0].strip(), '#'.join(l.rstrip() for l in lines[1:])))
def cnum(t):
    m = re.match(r'(\d+)\.', t)
    return int(m.group(1)) if m else 0
chapters = []          # (name, first page)
for i, (t, _) in enumerate(pages):
    n = cnum(t)
    if not chapters or n != cnum(pages[chapters[-1][1]][0]):
        name = re.sub(r'\s*\(\d+/\d+\).*$', '', t).strip()
        if name.isupper():
            name = name.lower()
            name = re.sub(r'[a-z]', lambda m: m.group(0).upper(), name, count=1)
        if n == 0: name = 'Welcome'
        chapters.append((name, i))
def c(s):
    s = s.encode('latin-1').decode('latin-1')
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'
out = ['/* generated from docs/manual.txt by tools/gen_manual.py - edit the .txt */',
       '#define MANUAL_PAGES %d' % len(pages), '#define MANUAL_CHAPTERS %d' % len(chapters),
       'static const char *const manual_title[MANUAL_PAGES] = {']
out += ['  %s,' % c(t) for t, _ in pages] + ['};', 'static const char *const manual_body[MANUAL_PAGES] = {']
out += ['  %s,' % c(b) for _, b in pages] + ['};', 'static const char *const manual_chapter_name[MANUAL_CHAPTERS] = {']
out += ['  %s,' % c(n) for n, _ in chapters] + ['};', 'static const int manual_chapter_page[MANUAL_CHAPTERS] = {']
out += ['  ' + ', '.join(str(p) for _, p in chapters), '};', '']
open(os.path.join(root, 'platform', 'dsui', 'dsui_manual.h'), 'w', encoding='latin-1').write('\n'.join(out))
print(len(pages), 'pages,', len(chapters), 'chapters')
