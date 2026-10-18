#!/usr/bin/env python3
"""Generate font_vec.h from a TrueType font: printable ASCII as triangles, for flat-shaded text with MSAA.

usage: tools/gen_font.py fonts/Rajdhani-Bold.ttf > include/font_vec.h

Units: the cap height is 1, y runs up from the baseline. Curves are flattened by length, holes bridged into their
outer contour and the result ear-clipped. Only quadratic (glyf) outlines without overlapping contours are handled.
"""
import struct, sys


def tables(data):
    n = struct.unpack('>H', data[4:6])[0]
    return {data[12 + 16 * i:16 + 16 * i].decode('latin1'): struct.unpack('>II', data[20 + 16 * i:28 + 16 * i]) for i in range(n)}


def read_font(path):
    d = open(path, 'rb').read(); t = tables(d)
    tab = lambda k: d[t[k][0]:t[k][0] + t[k][1]]
    head, maxp, hhea, hmtx, loca, glyf, cmap = (tab(k) for k in ('head', 'maxp', 'hhea', 'hmtx', 'loca', 'glyf', 'cmap'))
    upem, locfmt = struct.unpack('>H', head[18:20])[0], struct.unpack('>h', head[50:52])[0]
    nglyph, nhm = struct.unpack('>H', maxp[4:6])[0], struct.unpack('>H', hhea[34:36])[0]
    if locfmt == 0: offs = [2 * v for v in struct.unpack('>%dH' % (nglyph + 1), loca[:2 * (nglyph + 1)])]
    else: offs = list(struct.unpack('>%dI' % (nglyph + 1), loca[:4 * (nglyph + 1)]))
    adv = [struct.unpack('>H', hmtx[4 * min(i, nhm - 1):4 * min(i, nhm - 1) + 2])[0] for i in range(nglyph)]
    cmapd = {}
    for i in range(struct.unpack('>H', cmap[2:4])[0]):
        pid, eid, off = struct.unpack('>HHI', cmap[4 + 8 * i:12 + 8 * i])
        if struct.unpack('>H', cmap[off:off + 2])[0] != 4 or (pid, eid) not in ((3, 1), (0, 3), (0, 4)): continue
        s = cmap[off:]; seg = struct.unpack('>H', s[6:8])[0] // 2
        ends = struct.unpack('>%dH' % seg, s[14:14 + 2 * seg]); p = 16 + 2 * seg
        starts = struct.unpack('>%dH' % seg, s[p:p + 2 * seg]); deltas = struct.unpack('>%dh' % seg, s[p + 2 * seg:p + 4 * seg])
        ro = p + 4 * seg; ranges = struct.unpack('>%dH' % seg, s[ro:ro + 2 * seg])
        for k in range(seg):
            for c in range(starts[k], ends[k] + 1):
                if c == 0xFFFF: continue
                if ranges[k] == 0: g = (c + deltas[k]) & 0xFFFF
                else:
                    a = ro + 2 * k + ranges[k] + 2 * (c - starts[k]); g = struct.unpack('>H', s[a:a + 2])[0]
                    if g: g = (g + deltas[k]) & 0xFFFF
                cmapd[c] = g
        break
    cap = upem * 0.7
    if 'OS/2' in t:
        os2 = tab('OS/2')
        if struct.unpack('>H', os2[0:2])[0] >= 2: cap = struct.unpack('>h', os2[88:90])[0]
    return dict(glyf=glyf, offs=offs, adv=adv, cmap=cmapd, cap=cap)


def contours(f, g, m=(1, 0, 0, 1, 0, 0)):
    """the glyph's contours as lists of (x, y, on_curve), transformed by m"""
    a, b = f['offs'][g], f['offs'][g + 1]
    if a == b: return []
    d = f['glyf'][a:b]; nc = struct.unpack('>h', d[0:2])[0]
    tr = lambda x, y: (m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5])
    if nc < 0:                                                   # composite: parts with offsets (and scales)
        out, p = [], 10
        while True:
            fl, gi = struct.unpack('>HH', d[p:p + 4]); p += 4
            if fl & 1: dx, dy = struct.unpack('>hh', d[p:p + 4]); p += 4
            else: dx, dy = struct.unpack('>bb', d[p:p + 2]); p += 2
            sx, sy, s01, s10 = 1, 1, 0, 0
            if fl & 8: sx = sy = struct.unpack('>h', d[p:p + 2])[0] / 16384; p += 2
            elif fl & 0x40: sx, sy = (v / 16384 for v in struct.unpack('>hh', d[p:p + 4])); p += 4
            elif fl & 0x80: sx, s01, s10, sy = (v / 16384 for v in struct.unpack('>hhhh', d[p:p + 8])); p += 8
            for c in contours(f, gi, (sx, s01, s10, sy, dx, dy)):
                out.append([(*tr(x, y), on) for x, y, on in c])
            if not fl & 0x20: return out
    ends = struct.unpack('>%dH' % nc, d[10:10 + 2 * nc]); n = ends[-1] + 1
    p = 10 + 2 * nc; p += 2 + struct.unpack('>H', d[p:p + 2])[0]
    flags = []
    while len(flags) < n:
        fl = d[p]; p += 1; flags.append(fl)
        if fl & 8: flags += [fl] * d[p]; p += 1
    pts = []
    for axis, short, same in ((0, 2, 16), (1, 4, 32)):
        v, vals = 0, []
        for fl in flags:
            if fl & short: dv = d[p]; p += 1; v += dv if fl & same else -dv
            elif not fl & same: v += struct.unpack('>h', d[p:p + 2])[0]; p += 2
            vals.append(v)
        pts.append(vals)
    out, s = [], 0
    for e in ends:
        out.append([(*tr(pts[0][i], pts[1][i]), bool(flags[i] & 1)) for i in range(s, e + 1)]); s = e + 1
    return out


def flatten(c, seg):
    """a quadratic contour as a closed polygon, curves cut into pieces about seg long"""
    full, n = [], len(c)
    for i in range(n):                                           # implied on-curve points between two off-curve ones
        full.append(c[i])
        if not c[i][2] and not c[(i + 1) % n][2]:
            full.append(((c[i][0] + c[(i + 1) % n][0]) / 2, (c[i][1] + c[(i + 1) % n][1]) / 2, True))
    k = next(i for i, p in enumerate(full) if p[2])
    full = full[k:] + full[:k]
    pts, i, n = [], 0, len(full)
    while i < n:
        p0, p1 = full[i], full[(i + 1) % n]
        pts.append((p0[0], p0[1]))
        if p1[2]: i += 1; continue
        p2 = full[(i + 2) % n]
        ln = ((p1[0] - p0[0]) ** 2 + (p1[1] - p0[1]) ** 2) ** 0.5 + ((p2[0] - p1[0]) ** 2 + (p2[1] - p1[1]) ** 2) ** 0.5
        steps = max(2, min(8, round(ln / seg)))
        for t in range(1, steps):
            u = t / steps
            pts.append(((1 - u) ** 2 * p0[0] + 2 * u * (1 - u) * p1[0] + u * u * p2[0], (1 - u) ** 2 * p0[1] + 2 * u * (1 - u) * p1[1] + u * u * p2[1]))
        i += 2
    out = []                                                     # drop repeats and straight-through points
    for j, p in enumerate(pts):
        a, b = out[-1] if out else pts[-1], pts[(j + 1) % len(pts)]
        if p == a or abs((p[0] - a[0]) * (b[1] - a[1]) - (p[1] - a[1]) * (b[0] - a[0])) < 1e-6: continue
        out.append(p)
    return out


def area(p): return sum(p[i][0] * p[(i + 1) % len(p)][1] - p[(i + 1) % len(p)][0] * p[i][1] for i in range(len(p))) / 2


def inside(pt, poly):
    x, y, r = pt[0], pt[1], False
    for i in range(len(poly)):
        (x0, y0), (x1, y1) = poly[i], poly[i - 1]
        if (y0 > y) != (y1 > y) and x < x0 + (x1 - x0) * (y - y0) / (y1 - y0): r = not r
    return r


def cross(o, a, b): return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])


def seg_hit(p, q, a, b):
    """do segments pq and ab cross properly (not just touch at an end)?"""
    d1, d2, d3, d4 = cross(a, b, p), cross(a, b, q), cross(p, q, a), cross(p, q, b)
    return d1 * d2 < 0 and d3 * d4 < 0


def bridge(outer, hole, others):
    """join a hole into its outer contour through the shortest visible link from the hole's rightmost point"""
    m = max(range(len(hole)), key=lambda i: hole[i][0])
    M = hole[m]
    edges = [(poly[i], poly[(i + 1) % len(poly)]) for poly in [outer, hole] + others for i in range(len(poly))]
    best = None
    for j, P in enumerate(outer):
        if any(seg_hit(M, P, a, b) for a, b in edges): continue
        d = (P[0] - M[0]) ** 2 + (P[1] - M[1]) ** 2
        if best is None or d < best[0]: best = (d, j)
    j = best[1]
    return outer[:j + 1] + hole[m:] + hole[:m + 1] + outer[j:]


def earclip(poly):
    """counter-clockwise simple polygon (bridges allowed) -> triangles"""
    v, tris = list(poly), []
    while len(v) > 3:
        n = len(v)
        for i in range(n):
            a, b, c = v[i - 1], v[i], v[(i + 1) % n]
            if cross(a, b, c) <= 0: continue
            if any(p not in (a, b, c) and cross(a, b, p) >= 0 and cross(b, c, p) >= 0 and cross(c, a, p) >= 0 for p in v): continue
            if any(seg_hit(a, c, v[k], v[(k + 1) % n]) for k in range(n)): continue      # an edge through a pinch point
            tris.append((a, b, c)); del v[i]; break
        else:                                                    # nothing clean left (rounding): cut the flattest
            i = min(range(n), key=lambda i: abs(cross(v[i - 1], v[i], v[(i + 1) % n])))
            del v[i]
    if len(v) == 3 and cross(*v) > 0: tris.append(tuple(v))
    return tris


def split_pinches(p):
    """a contour that touches itself at a point, as separate loops"""
    seen = {}
    for i, q in enumerate(p):
        if q in seen:
            j = seen[q]
            return split_pinches(p[j:i]) + split_pinches(p[i:] + p[:j])
        seen[q] = i
    return [p]


def triangles(polys):
    polys = [q for p in polys for q in split_pinches(p)]
    polys = [p for p in polys if len(p) > 2 and abs(area(p)) > 1e-9]
    if not polys: return []
    big = max(polys, key=lambda p: abs(area(p)))
    outers = [p if area(p) > 0 else p[::-1] for p in polys if (area(p) > 0) == (area(big) > 0)]
    holes = [p if area(p) < 0 else p[::-1] for p in polys if (area(p) > 0) != (area(big) > 0)]
    tris = []
    for o in outers:
        mine = [h for h in holes if inside(h[0], o) and not any(o2 is not o and inside(h[0], o2) and inside(o2[0], o) for o2 in outers)]
        poly = o
        for h in sorted(mine, key=lambda h: -max(p[0] for p in h)):
            poly = bridge(poly, h, [x for x in mine if x is not h])
        tris += earclip(poly)
    return tris


def main(path):
    f = read_font(path); cap = f['cap']
    glyphs, data = [], []
    for ch in range(32, 127):
        g = f['cmap'].get(ch, 0)
        tr = triangles([flatten(c, cap * 0.06) for c in contours(f, g) if len(c) > 2])
        glyphs.append((len(data) // 6, len(tr), f['adv'][g] / cap))
        for t in tr: data += [v / cap for p in t for v in p]
    print('/* font_vec.h - generated by tools/gen_font.py from %s: printable ASCII as triangles (x0, y0, x1, y1, x2, y2),' % path.rsplit('/', 1)[-1])
    print(' * cap height 1, y up from the baseline. Do not edit. */')
    print('static const struct { unsigned short first, n; float adv; } FV_GLYPH[95] = {')
    print(',\n'.join('    {%d, %d, %.4ff}' % g for g in glyphs) + '\n};')
    print('static const float FV_TRI[] = {')
    for i in range(0, len(data), 12): print('    ' + ','.join('%.4g' % v for v in data[i:i + 12]) + ',')
    print('};')


if __name__ == '__main__':
    main(sys.argv[1])
