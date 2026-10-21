#!/usr/bin/env python3
"""Put a blank line before and after if/for/while/switch/do blocks that sit among other statements.

clang-format cannot do this, so it is a second pass: run clang-format, then this, then clang-format again.
usage: tools/space_blocks.py FILE...
"""
import re
import sys

CONTROL = re.compile(r'^\s*(if|for|while|switch|do)\b')
BLOCK_END = re.compile(r'^(\s*)\}\s*$')
NO_AFTER = re.compile(r'^\s*(\}|case\b|default\b|else\b|while\b|#)')


def indent(line):
    return len(line) - len(line.lstrip())


def process(lines):
    out = []
    openers = []  # (indent, is_control) of every open '{' line
    for i, line in enumerate(lines):
        s = re.sub(r'\s*/\*.*\*/$', '', line.strip())
        if CONTROL.match(line) and s.endswith('{') and not line.rstrip().endswith('\\') and out:
            prev = out[-1].strip()
            if prev and prev[-1] in ';}' and not prev.startswith(('/*', '*', '//', '#')):
                out.append('')
        end = BLOCK_END.match(line)
        if end and openers and openers[-1][0] == indent(line):
            was_control = openers.pop()[1]
            out.append(line)
            nxt = lines[i + 1] if i + 1 < len(lines) else ''
            if was_control and nxt.strip() and indent(nxt) == indent(line) and not NO_AFTER.match(nxt):
                out.append('')
            continue
        if s.endswith('{') and not s.startswith('#'):
            openers.append((indent(line), bool(CONTROL.match(line))))
        out.append(line)
    return out


for path in sys.argv[1:]:
    text = open(path).read().split('\n')
    open(path, 'w').write('\n'.join(process(text)))
