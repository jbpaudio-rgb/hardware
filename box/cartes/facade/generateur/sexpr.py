"""Minimal s-expression reader/writer for KiCad and Specctra files."""
import re

TOKEN = re.compile(r'\s*(?:(\()|(\))|"((?:[^"\\]|\\.)*)"|([^\s()"]+))')


class Q(str):
    """A string that was quoted in the source (written back with quotes)."""


def parse(text):
    stack, cur = [], []
    pos = 0
    while True:
        m = TOKEN.match(text, pos)
        if not m or m.end() == pos:
            break
        pos = m.end()
        if m.group(1):
            stack.append(cur)
            cur = []
        elif m.group(2):
            done = cur
            cur = stack.pop()
            cur.append(done)
        elif m.group(3) is not None:
            cur.append(Q(m.group(3).replace('\\"', '"')))
        else:
            cur.append(m.group(4))
    return cur[0] if len(cur) == 1 else cur


def dump(x, ind=0):
    if isinstance(x, list):
        inner = [dump(e, ind + 1) for e in x]
        line = "(" + " ".join(inner) + ")"
        if len(line) < 120 and "\n" not in line:
            return line
        out = "(" + inner[0]
        for e in inner[1:]:
            out += "\n" + "  " * (ind + 1) + e
        return out + ")"
    if isinstance(x, Q):
        return '"' + x.replace('"', '\\"') + '"'
    if isinstance(x, float):
        s = f"{x:.4f}".rstrip("0").rstrip(".")
        return s if s not in ("-0", "") else "0"
    return str(x)


def find(x, key):
    return [e for e in x if isinstance(e, list) and e and e[0] == key]


def first(x, key):
    r = find(x, key)
    return r[0] if r else None
