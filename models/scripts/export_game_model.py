"""Export the low-poly car for SCR Redux. From the repository root:

LOWPOLY=1 blender -b --factory-startup -P models/scripts/build_car.py -- models
blender -b models/stuntcar.blend -P models/scripts/export_game_model.py -- models/stuntcar.scm

Writes flat-coloured triangles in groups plus the suspension rig and physics data the game needs.
Game frame: +x right, +y up, +z forward, origin = centre of mass (Blender: x, z - COM.z, y - COM.y).
All rig maths is done in the game frame so the runtime's rotations match.
"""
import bpy, math, struct, sys
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

OUT = sys.argv[sys.argv.index('--') + 1]
COM = Vector((0.0, 0.218, 0.833))          # Blender coords; mass estimate of the lifted car
LMIN = 0.12                                # game bump-stop length (suspension length at full bump)
FY, RY = 1.80, -1.15
STEER_RATIO = 8.0

dg = bpy.context.evaluated_depsgraph_get()
car = bpy.data.collections['StuntCar']
driver = bpy.data.collections['Driver fit check']


def G(v):
    v = Vector(v)
    return Vector((v.x, v.z - COM.z, v.y - COM.y))


def Gdir(v):
    v = Vector(v)
    return Vector((v.x, v.z, v.y))


def C(o):
    return sum((o.matrix_world @ Vector(c) for c in o.bound_box), Vector()) / 8


def bbox(objs):
    pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    return (Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts))),
            Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts))))


def bvh_game(objs, M=None):
    """BVH of the objects' evaluated meshes in the game frame, optionally moved by a game-frame matrix."""
    V, P = [], []
    for o in objs:
        oe = o.evaluated_get(dg)
        me = oe.to_mesh()
        off = len(V)
        for v in me.vertices:
            g = G(oe.matrix_world @ v.co)
            V.append(M @ g if M is not None else g)
        P += [[off + i for i in poly.vertices] for poly in me.polygons]
        oe.to_mesh_clear()
    return BVHTree.FromPolygons(V, P)


def rot(p, axis, ang):
    return Matrix.Translation(p) @ Matrix.Rotation(ang, 4, axis.normalized()) @ Matrix.Translation(-p)


meshes = [o for o in car.objects if o.type == 'MESH']
named = lambda prefix, s=None: [o for o in meshes if o.name.startswith(prefix) and (s is None or C(o).x * s > 0)]

# ---------------------------------------------------------------- colours
PAINT, LIT, UNLIT = 128, 255, 0
COL = {
    'Paint red': (0xF2F2F2, PAINT), 'Header wrap red': (0xB4B4B4, PAINT), 'Strap red': (0x9A9A9A, PAINT),
    'Chrome': (0xD2D2DC, LIT), 'Polished alloy': (0xB4B8C0, LIT), 'Cast aluminium': (0x8A8E96, LIT),
    'Tyre rubber': (0x1E1E20, LIT), 'Blue vinyl': (0x2440A0, LIT), 'Satin black': (0x1C1C20, LIT),
    'Dark steel': (0x4A4C54, LIT), 'Spring orange': (0xE85A10, LIT), 'Plug wire': (0x161616, LIT),
    'Gauge face': (0x101014, LIT), 'Gauge needle': (0xFF6A10, UNLIT), 'Tail lens': (0xE01408, UNLIT),
    'Pipe interior': (0x060606, LIT), 'Extinguisher red': (0xC81810, LIT), 'Harness': (0x18181C, LIT),
    'Leather': (0x141416, LIT), 'Caliper': (0xB82010, LIT), 'Strap black': (0x1A1A1A, LIT),
    'Mannequin': (0x2848A0, LIT),
}


def colour(mat):
    if mat and mat.name in COL:
        return COL[mat.name]
    c = mat.diffuse_color if mat else (0.5, 0.5, 0.5, 1)
    srgb = [int(255 * (x * 12.92 if x <= 0.0031308 else 1.055 * x ** (1 / 2.4) - 0.055)) for x in c[:3]]
    return (srgb[0] << 16 | srgb[1] << 8 | srgb[2], LIT)


# ---------------------------------------------------------------- group assignment
ROLE = dict(BODY=0, ENGTOP=1, ROOF=2, DRIVER=3, STWHL=4, WHEEL=5, UPRIGHT=6, LARM=7, UARM=8, CBOT=9, CTOP=10,
            SPRING=11, LINK=12)
CORNERS = ['FL', 'FR', 'RL', 'RR']


def ancestors(o):
    while o.parent:
        o = o.parent
        yield o


def corner_of(o):
    c = C(o)
    return ('F' if c.y > 0.3 else 'R') + ('L' if c.x < 0 else 'R')


def classify(o):
    """-> (role, corner or None, link index or -1)"""
    n = o.name
    anc = {a.name.split('.')[0] for a in ancestors(o)}
    for side, tag in ((1, 'R'), (-1, 'L')):
        if 'Front wheel ' + tag in anc:
            return 'WHEEL', 'F' + tag, -1
        if 'Rear wheel ' + tag in anc:
            return 'WHEEL', 'R' + tag, -1
        if 'Steering pivot ' + tag in anc or 'Rear upright ' + tag in anc:
            return 'UPRIGHT', corner_of(o), -1
    if 'Steering wheel rim' in anc or n.startswith('Steering wheel rim'):
        return 'STWHL', None, -1
    for pre in ('Lower wishbone', 'Rear lower wishbone', 'Upper wishbone', 'Rear upper wishbone'):
        if n.startswith(pre) and not n.startswith((pre + ' bush', pre + ' bracket')):
            return ('LARM' if 'ower' in pre else 'UARM'), corner_of(o), -1
    if n.startswith(('Ball joint housing', 'Rear ball joint housing')):
        hub_z = 0.50
        return ('LARM' if C(o).z < hub_z else 'UARM'), corner_of(o), -1
    for pre in ('Front coilover ', 'Rear coilover '):
        if n.startswith(pre):
            part = n[len(pre):].split('.')[0]
            role = ('SPRING' if part == 'spring' else 'CBOT' if part in ('shaft', 'seat low', 'eye low') else 'CTOP')
            return role, corner_of(o), -1
    if n.startswith('Tie rod') and not n.startswith('Tie rod end'):
        return 'LINK', corner_of(o), 0
    if n.startswith('Rear toe link') or n.startswith('Toe link'):
        if n.startswith('Toe link tab'):
            return 'BODY', None, -1
        return 'LINK', corner_of(o), 0
    if n.startswith(('Half-shaft', 'Inner CV boot', 'Outer CV boot')):
        return 'LINK', corner_of(o), 1
    if n.startswith(('Velocity stack', 'Stack bore', 'Injector nozzle', 'Fuel rail', 'Throttle shaft')):
        return 'ENGTOP', None, -1
    if n.startswith('Cage'):
        return 'ROOF', None, -1
    return 'BODY', None, -1


groups = {}   # key -> list of (verts, col, alpha)


def add_mesh(o, key):
    oe = o.evaluated_get(dg)
    me = oe.to_mesh()
    me.calc_loop_triangles()
    M = oe.matrix_world
    tris = groups.setdefault(key, [])
    for t in me.loop_triangles:
        mat = me.materials[t.material_index] if me.materials else None
        col, alpha = colour(mat)
        if o.name.startswith('Driver helmet'):
            nrm = (M.to_3x3() @ t.normal).normalized()
            col = 0x0A0A10 if nrm.y > 0.55 and abs(nrm.z) < 0.6 else 0xF0F0F0   # visor band on a white helmet
        tris.append(([G(M @ me.vertices[i].co) for i in t.vertices], col, alpha))
    oe.to_mesh_clear()


for o in meshes:
    role, corner, link = classify(o)
    add_mesh(o, (role, corner, link))
for o in driver.objects:
    if o.type == 'MESH':
        add_mesh(o, ('DRIVER', None, -1))

# ---------------------------------------------------------------- suspension rig (game frame)
rigs, wheels = [], []
for cn in CORNERS:
    front, s = cn[0] == 'F', (1 if cn[1] == 'R' else -1)
    pre = '' if front else 'Rear '
    lo_n, up_n = (pre + 'lower wishbone' if pre else 'Lower wishbone'), (pre + 'upper wishbone' if pre else 'Upper wishbone')
    lb = sorted([C(o) for o in named(lo_n + ' bush', s)], key=lambda v: v.y)
    ub = sorted([C(o) for o in named(up_n + ' bush', s)], key=lambda v: v.y)
    L0, U0 = sorted([C(o) for o in named(pre + ('ball joint housing' if pre else 'Ball joint housing'), s)],
                    key=lambda v: v.z)
    coil = ('Front' if front else 'Rear') + ' coilover '
    eye_lo, eye_hi = C(named(coil + 'eye low', s)[0]), C(named(coil + 'eye high', s)[0])
    max_ext = named(coil + 'body', s)[0]['max_extension']
    pad = named(('Bump stop pad' if front else 'Rear bump stop pad'), s)[0]
    pad_z = min((pad.matrix_world @ Vector(c)).z for c in pad.bound_box)
    strike = Vector(pad['contact'])
    wheel = bpy.data.objects[('Front wheel ' if front else 'Rear wheel ') + ('R' if s > 0 else 'L')]
    hub = wheel.matrix_world.translation.copy()
    tyre = [o for o in wheel.children_recursive if o.name.startswith((wheel.name + ' tyre', wheel.name + ' tread'))]
    tb = bbox(tyre)
    radius = (tb[1].z - tb[0].z) / 2
    width = tb[1].x - tb[0].x
    links = []
    if front:
        inner = bpy.data.objects['Tie rod inner ' + ('R' if s > 0 else 'L')].matrix_world.translation
        outer = bpy.data.objects['Tie rod target ' + ('R' if s > 0 else 'L')].matrix_world.translation
        links.append((inner, outer))
    else:
        ends = sorted([C(o) for o in named('Rear toe link rod end', s)], key=lambda v: abs(v.x))
        links.append((ends[0], ends[-1]))
        links.append((Vector((s * 0.21, RY, 0.50)), Vector((s * 0.75, RY, 0.50))))
    # convert everything to the game frame
    lb, ub = [G(v) for v in lb], [G(v) for v in ub]
    L0g, U0g, elo, ehi, hubg = G(L0), G(U0), G(eye_lo), G(eye_hi), G(hub)
    pad_tree = bvh_game([pad])
    uarm = [o for o in named(up_n, s) if not o.name.startswith((up_n + ' bush', up_n + ' bracket'))]
    la, ua, d0 = lb[1] - lb[0], ub[1] - ub[0], (U0g - L0g).length
    c0 = (elo - ehi).length

    def upper_for(a, b_prev):
        L = rot(lb[0], la, a) @ L0g
        best = None
        for k in range(-200, 201):
            b = b_prev + math.radians(k * 0.05)
            e = abs((rot(ub[0], ua, b) @ U0g - L).length - d0)
            if best is None or e < best[0]:
                best = (e, b)
        return best[1]

    def hub_dz(a, b):
        L, U = rot(lb[0], la, a) @ L0g, rot(ub[0], ua, b) @ U0g
        q = (U0g - L0g).rotation_difference(U - L)
        T = Matrix.Translation(L) @ q.to_matrix().to_4x4() @ Matrix.Translation(-L0g)
        return (T @ hubg).y - hubg.y

    # sweep the lower arm both ways; stop at the bump stop (up) and full shock extension (down)
    rows, limits = [(0.0, 0.0, 0.0)], {}
    for direction in (1, -1):
        b, k = 0.0, 0
        while True:
            k += 1
            a = math.radians(direction * k * 0.25)
            b = upper_for(a, b)
            dz = hub_dz(a, b)
            ext = (rot(lb[0], la, a) @ elo - ehi).length - c0
            hit = dz > 0 and bool(bvh_game(uarm, rot(ub[0], ua, b)).overlap(pad_tree))
            stop = hit or (dz < 0 and ext >= max_ext) or k > 400
            rows.append((dz, a, b))
            if stop:
                limits['bump' if dz > 0 else 'droop'] = abs(dz)
                break
    rows.sort()
    bump, droop = limits['bump'], limits['droop']
    rest_l = LMIN + bump
    mount = hubg + Vector((0, rest_l, 0))
    wheels.append(dict(mount=mount, radius=radius, lmin=LMIN, lf=LMIN + bump + droop, rest_l=rest_l, bump=bump,
                       droop=droop, kp_point=L0g, kp_axis=(U0g - L0g).normalized(), hub=hubg, width=width))
    rigs.append(dict(lb=lb, ub=ub, L0=L0g, U0=U0g, elo=elo, ehi=ehi,
                     links=[(G(i), G(o)) for i, o in links], table=rows))
    print(f'RIG {cn}: bump {bump * 100:.1f} cm, droop {droop * 100:.1f} cm, radius {radius:.3f}, table {len(rows)}')

# ---------------------------------------------------------------- physics points
# the game's vertical field of view is ~48 deg: from the driver's real eye the windscreen hoop would sit outside
# the frame, so the cockpit camera sits a little behind and above the helmet (the driver is hidden in that view)
eye = G((0.0, -0.676, 1.646))
fb_lo, fb_hi = bbox(named('Front bumper hoop'))
rb_lo, rb_hi = bbox(named('Rear bumper hoop'))
tb_lo, tb_hi = bbox(named('Body tub'))
cage_lo, cage_hi = bbox(named('Cage side'))
st_lo, st_hi = bbox(named('Velocity stack'))
cpts = [G((fb_lo.x * 0.9, fb_hi.y, fb_lo.z)), G((fb_hi.x * 0.9, fb_hi.y, fb_lo.z)),
        G((rb_lo.x * 0.9, rb_lo.y, rb_lo.z)), G((rb_hi.x * 0.9, rb_lo.y, rb_lo.z)),
        G((0, fb_hi.y, fb_hi.z)),
        G((tb_lo.x, tb_hi.y, tb_lo.z)), G((tb_hi.x, tb_hi.y, tb_lo.z)),
        G((tb_lo.x, tb_lo.y, tb_lo.z)), G((tb_hi.x, tb_lo.y, tb_lo.z)),
        G((0, 0.02, cage_hi.z))]
for w in wheels:
    h = w['hub']
    cpts.append(Vector((h.x + math.copysign(w['width'] / 2, h.x), h.y, h.z)))
cpts += [G((0, -1.22, cage_hi.z)), G((0, (st_lo.y + st_hi.y) / 2, st_hi.z))]
assert len(cpts) == 16
flames, flame_dirs = [], []
for s in (-1, 1):
    tips = sorted([o for o in named('Zoomie tip', s)], key=lambda o: o.matrix_world.translation.y)
    for o in tips:
        d = (o.matrix_world.to_3x3() @ Vector((0, 0, 1))).normalized()
        flames.append(G(o.matrix_world.translation + d * 0.08))
        flame_dirs.append(Gdir(d).normalized())
piv = bpy.data.objects['Steering wheel pivot']
stw_c = G(piv.matrix_world.translation)
stw_ax = Gdir(piv.matrix_world.to_3x3() @ Vector((0, 0, 1))).normalized()

# ---------------------------------------------------------------- write
vbuf, gtab = bytearray(), []
order = sorted(groups, key=lambda k: (ROLE[k[0]], CORNERS.index(k[1]) if k[1] else -1, k[2]))
nv = 0
for key in order:
    role, corner, link = key
    tris = groups[key]
    abbrev = dict(WHEEL='W', UPRIGHT='UP', LARM='LA', UARM='UA', CBOT='CB', CTOP='CT', SPRING='SP', LINK='LK')
    name = corner + '_' + abbrev[role] + (str(link) if link >= 0 else '') if corner else role
    first = nv
    for verts, col, alpha in tris:
        for v in verts:
            vbuf += struct.pack('<3f4B', v.x, v.y, v.z, (col >> 16) & 255, (col >> 8) & 255, col & 255, alpha)
            nv += 1
    gtab.append((name[:7].encode(), ROLE[role], CORNERS.index(corner) if corner else -1, link, first, nv - first))


def v3(v):
    return struct.pack('<3f', *v)


out = bytearray(b'SCRCAR1\0')
out += struct.pack('<I', 2)
out += struct.pack('<2f', COM.z, COM.z + 0.16)
out += v3(eye)
for p in cpts:
    out += v3(p)
for p in flames:
    out += v3(p)
for d in flame_dirs:          # version 2: pipe axis of each flame
    out += v3(d)
out += v3(stw_c) + v3(stw_ax) + struct.pack('<f', STEER_RATIO)
for w in wheels:
    out += v3(w['mount']) + struct.pack('<6f', w['radius'], w['lmin'], w['lf'], w['rest_l'], w['bump'], w['droop'])
    out += v3(w['kp_point']) + v3(w['kp_axis']) + v3(w['hub'])
for r in rigs:
    for v in (r['lb'][0], r['lb'][1], r['ub'][0], r['ub'][1], r['L0'], r['U0'], r['elo'], r['ehi']):
        out += v3(v)
    out += struct.pack('<I', len(r['links']))
    for i, o in r['links']:
        out += v3(i) + v3(o)
    out += struct.pack('<I', len(r['table']))
    for dz, a, b in r['table']:
        out += struct.pack('<3f', dz, a, b)
out += struct.pack('<I', len(gtab))
for name, role, corner, link, first, count in gtab:
    out += struct.pack('<8sIiiII', name, role, corner, link, first, count)
out += struct.pack('<I', nv) + vbuf
with open(OUT, 'wb') as f:
    f.write(out)
print(f'EXPORT {OUT}: {nv // 3} triangles, {len(gtab)} groups, {len(out) / 1024:.0f} KiB')
for name, role, corner, link, first, count in gtab:
    print('GROUP', name.decode(), role, corner, link, count // 3)
