"""Procedural Stunt Car Racer buggy for Blender 5.x.

blender -b --factory-startup -P build_car.py -- <out_dir>
Writes stuntcar.blend and stuntcar.glb into <out_dir>. With LOWPOLY=1 it builds the faceted version the game uses
(models/stuntcar.blend in SCR Redux); without, the detailed one for renders.
Axes: X right, Y forward, Z up, metres.
"""
import bpy, bmesh, math, os, sys
from mathutils import Vector, Matrix

OUT = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else os.getcwd()
LOW = os.environ.get('LOWPOLY') == '1'  # faceted game version: coarse segments, no bevels or tiny details
pi = math.pi

bpy.ops.wm.read_factory_settings(use_empty=True)
scn = bpy.context.scene
CAR = bpy.data.collections.new('StuntCar')
scn.collection.children.link(CAR)
ENV = bpy.data.collections.new('Environment')
scn.collection.children.link(ENV)
ROOT = bpy.data.objects.new('Stunt Car', None)
CAR.objects.link(ROOT)


# ---------------------------------------------------------------- materials
def srgb(h):
    h = h.lstrip('#')
    c = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return tuple(x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c)


def material(name, color, metal=0.0, rough=0.5, coat=0.0, coat_rough=0.03, spec=0.5,
             emit=None, emit_str=0.0, trans=0.0, ior=1.45):
    m = bpy.data.materials.new(name)
    try:
        m.use_nodes = True
    except Exception:
        pass
    p = m.node_tree.nodes.get('Principled BSDF')
    col = srgb(color)
    p.inputs['Base Color'].default_value = (*col, 1)
    p.inputs['Metallic'].default_value = metal
    p.inputs['Roughness'].default_value = rough
    p.inputs['Coat Weight'].default_value = coat
    p.inputs['Coat Roughness'].default_value = coat_rough
    p.inputs['Specular IOR Level'].default_value = spec
    p.inputs['IOR'].default_value = ior
    p.inputs['Transmission Weight'].default_value = trans
    if emit:
        p.inputs['Emission Color'].default_value = (*srgb(emit), 1)
        p.inputs['Emission Strength'].default_value = emit_str
    m.diffuse_color = (*col, 1)
    m.metallic = metal
    m.roughness = rough
    return m


def noise_detail(m, scale, bump=0.0, dist=0.001, rough=None, color2=None, cscale=None):
    nt = m.node_tree
    p = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord')
    n = nt.nodes.new('ShaderNodeTexNoise')
    n.inputs['Scale'].default_value = scale
    n.inputs['Detail'].default_value = 10
    nt.links.new(tc.outputs['Object'], n.inputs['Vector'])
    if bump:
        b = nt.nodes.new('ShaderNodeBump')
        b.inputs['Strength'].default_value = bump
        b.inputs['Distance'].default_value = dist
        nt.links.new(n.outputs['Fac'], b.inputs['Height'])
        nt.links.new(b.outputs['Normal'], p.inputs['Normal'])
    if rough:
        mr = nt.nodes.new('ShaderNodeMapRange')
        mr.inputs['To Min'].default_value, mr.inputs['To Max'].default_value = rough
        nt.links.new(n.outputs['Fac'], mr.inputs['Value'])
        nt.links.new(mr.outputs['Result'], p.inputs['Roughness'])
    if color2:
        n2 = nt.nodes.new('ShaderNodeTexNoise')
        n2.inputs['Scale'].default_value = cscale or scale * 0.05
        n2.inputs['Detail'].default_value = 6
        nt.links.new(tc.outputs['Object'], n2.inputs['Vector'])
        mix = nt.nodes.new('ShaderNodeMix')
        mix.data_type = 'RGBA'
        mix.inputs['A'].default_value = p.inputs['Base Color'].default_value
        mix.inputs['B'].default_value = (*srgb(color2), 1)
        nt.links.new(n2.outputs['Fac'], mix.inputs['Factor'])
        nt.links.new(mix.outputs['Result'], p.inputs['Base Color'])


PAINT = material('Paint red', '#b3160c', rough=0.32, coat=1.0, coat_rough=0.02)
CHROME = material('Chrome', '#f2f2f2', metal=1.0, rough=0.04)
ALLOY = material('Polished alloy', '#d9dbde', metal=1.0, rough=0.14)
CAST = material('Cast aluminium', '#9ea1a5', metal=1.0, rough=0.48)
noise_detail(CAST, 900, bump=0.25, dist=0.0004)
RUBBER = material('Tyre rubber', '#0e0e0e', rough=0.78, spec=0.35)
noise_detail(RUBBER, 400, bump=0.15, dist=0.0003, rough=(0.68, 0.88))
VINYL = material('Blue vinyl', '#1c3584', rough=0.5, spec=0.45, coat=0.2, coat_rough=0.3)
noise_detail(VINYL, 300, bump=0.08, dist=0.0003)
SATIN = material('Satin black', '#0d0d0f', metal=0.2, rough=0.38)
STEEL = material('Dark steel', '#3a3c40', metal=1.0, rough=0.35)
SPRING = material('Spring orange', '#e2520f', rough=0.25, coat=1.0, coat_rough=0.05)
WIRE = material('Plug wire', '#141414', rough=0.35, spec=0.5)
GAUGE = material('Gauge face', '#0b0b0c', rough=0.25, emit='#202020', emit_str=0.0)
NEEDLE = material('Gauge needle', '#ff5a10', rough=0.3, emit='#ff5a10', emit_str=1.5)
LENS = material('Tail lens', '#c00a05', rough=0.08, trans=0.6, ior=1.5, emit='#ff1505', emit_str=0.3)
SOOT = material('Pipe interior', '#050505', rough=0.9)
EXTING = material('Extinguisher red', '#c9150b', rough=0.2, coat=1.0)


# ---------------------------------------------------------------- helpers
def finish(o, parent=None):
    CAR.objects.link(o)
    if parent:
        o.parent = parent
    return o


def shade(me, ang):
    try:
        me.shade_smooth()
    except AttributeError:
        me.polygons.foreach_set('use_smooth', [True] * len(me.polygons))
    try:
        me.set_sharp_from_angle(angle=math.radians(ang))
    except Exception:
        pass


def bm_obj(name, bm, mats, parent=None, ang=30):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for m in mats:
        me.materials.append(m)
    shade(me, ang)
    return finish(bpy.data.objects.new(name, me), parent)


def bevel(o, w, seg=3, ang=35):
    if LOW:
        return None
    m = o.modifiers.new('Bevel', 'BEVEL')
    m.width = w
    m.segments = seg
    m.limit_method = 'ANGLE'
    m.angle_limit = math.radians(ang)
    m.harden_normals = True
    return m


def box(name, size, loc, mat, rot=(0, 0, 0), bev=0.008, seg=3, parent=None):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts)
    o = bm_obj(name, bm, [mat], parent)
    o.location = loc
    o.rotation_euler = rot
    if bev:
        bevel(o, bev, seg)
    return o


def orient(o, a, d):
    o.location = a
    o.rotation_mode = 'QUATERNION'
    o.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d.normalized())


def cyl(name, a, b, r, mat, seg=32, parent=None, bev=0.0, r2=None):
    a, b = Vector(a), Vector(b)
    d = b - a
    if LOW and seg > 8:
        seg = max(8, seg // 2)
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=seg,
                          radius1=r, radius2=r if r2 is None else r2, depth=d.length)
    o = bm_obj(name, bm, [mat], parent, ang=40)
    orient(o, (a + b) / 2, d)
    if bev:
        bevel(o, bev, 2)
    return o


def sphere(name, loc, r, mat, scale=(1, 1, 1), parent=None):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=12 if LOW else 32, v_segments=8 if LOW else 16, radius=r)
    o = bm_obj(name, bm, [mat], parent, ang=80)
    o.location = loc
    o.scale = scale
    return o


def lathe(name, prof, mat, seg=72, parent=None, closed=False, ang=40):
    """Revolve a (r, z) profile around local Z."""
    if LOW:
        seg = max(8, seg // 3)
    n = len(prof)
    verts, faces = [], []
    for i in range(seg):
        a = 2 * pi * i / seg
        ca, sa = math.cos(a), math.sin(a)
        verts += [(r * ca, r * sa, z) for r, z in prof]
    for i in range(seg):
        i2 = (i + 1) % seg
        for j in range(n if closed else n - 1):
            j2 = (j + 1) % n
            faces.append((i * n + j, i2 * n + j, i2 * n + j2, i * n + j2))
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat)
    shade(me, ang)
    return finish(bpy.data.objects.new(name, me), parent)


def tube(name, pts, r, mat, parent=None, closed=False, radii=None, res=6, caps=True):
    if LOW:
        res = 1 if r < 0.012 else min(res, 2)
    cu = bpy.data.curves.new(name, 'CURVE')
    cu.dimensions = '3D'
    cu.bevel_mode = 'ROUND'
    cu.bevel_depth = r
    cu.bevel_resolution = res
    cu.use_fill_caps = caps
    cu.twist_mode = 'MINIMUM'
    sp = cu.splines.new('POLY')
    sp.points.add(len(pts) - 1)
    for i, p in enumerate(pts):
        sp.points[i].co = (p[0], p[1], p[2], 1.0)
        if radii:
            sp.points[i].radius = radii[i]
    sp.use_cyclic_u = closed
    sp.use_smooth = True
    cu.materials.append(mat)
    return finish(bpy.data.objects.new(name, cu), parent)


def corner(p_prev, p, p_next, rad):
    d1, d2 = p_prev - p, p_next - p
    t = min(rad, d1.length * 0.45, d2.length * 0.45)
    return p + d1.normalized() * t, p + d2.normalized() * t


def rounded(pts, rad, closed=False, n=12):
    if LOW:
        n = min(n, 5)
    P = [Vector(p) for p in pts]
    N = len(P)
    out = []
    for i in range(N):
        if not closed and i in (0, N - 1):
            out.append(P[i].copy())
            continue
        e, f = corner(P[i - 1], P[i], P[(i + 1) % N], rad)
        out += [(1 - u) ** 2 * e + 2 * (1 - u) * u * P[i] + u * u * f
                for u in (k / n for k in range(n + 1))]
    return out


def corner_mid(pts, i, rad):
    P = [Vector(p) for p in pts]
    e, f = corner(P[i - 1], P[i], P[(i + 1) % len(P)], rad)
    return 0.25 * e + 0.5 * P[i] + 0.25 * f


def catmull(pts, n=16):
    if LOW:
        n = max(3, n // 2)
    P = [Vector(p) for p in pts]
    P = [2 * P[0] - P[1]] + P + [2 * P[-1] - P[-2]]
    out = []
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        for k in range(n):
            t = k / n
            out.append(0.5 * (2 * p1 + (p2 - p0) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t
                              + (3 * p1 - p0 - 3 * p2 + p3) * t ** 3))
    out.append(P[-2])
    return out


def resample(pts, step):
    L = [0.0]
    for a, b in zip(pts, pts[1:]):
        L.append(L[-1] + (b - a).length)
    total = L[-1]
    n = max(2, int(total / step))
    out, j = [], 0
    for k in range(n + 1):
        s = total * k / n
        while j < len(L) - 2 and L[j + 1] < s:
            j += 1
        seg = L[j + 1] - L[j] or 1e-9
        out.append(pts[j].lerp(pts[j + 1], (s - L[j]) / seg))
    return out, total


def helix(a, b, R, turns, per=28):
    if LOW:
        per = 6
    a, b = Vector(a), Vector(b)
    z = (b - a).normalized()
    x = z.orthogonal().normalized()
    y = z.cross(x)
    L = (b - a).length
    tot = int(turns * per)
    pts = []
    for i in range(tot + 1):
        t = i / tot
        h = t + 0.035 * math.sin(2 * pi * t)  # tighter end coils
        ang = 2 * pi * turns * t
        pts.append(a + z * (L * min(max(h, 0), 1)) + (x * math.cos(ang) + y * math.sin(ang)) * R)
    return pts


def coilover(name, a, b, R=0.046, wire=0.0085, turns=7.5, reservoir=None):
    a, b = Vector(a), Vector(b)
    u = (b - a).normalized()
    L = (b - a).length
    if reservoir is not None:
        side = Vector(reservoir).normalized()
        side = (side - u * side.dot(u)).normalized()
        r0 = b - u * 0.05 + side * (R * 1.75)
        cyl(name + ' reservoir', r0, r0 - u * 0.16, R * 0.5, ALLOY, bev=0.006)
        cyl(name + ' reservoir cap', r0, r0 + u * 0.012, R * 0.42, SATIN)
        tube(name + ' reservoir hose', catmull([b - u * 0.07, b - u * 0.07 + side * R * 0.9,
                                               r0 - u * 0.02 - side * R * 0.45], 6), 0.007, WIRE, res=2)
    cyl(name + ' shaft', a + u * 0.02, a + u * L * 0.55, R * 0.2, CHROME, seg=16)
    body = cyl(name + ' body', a + u * L * 0.42, b - u * 0.03, R * 0.52, ALLOY, bev=0.004)
    body['max_extension'] = 0.08 * L  # shaft top stays 5 % of L inside the body: internal droop stop
    cyl(name + ' seat low', a + u * L * 0.10, a + u * (L * 0.10 + 0.01), R * 1.3, ALLOY, bev=0.002)
    cyl(name + ' seat high', b - u * (L * 0.09 + 0.01), b - u * L * 0.09, R * 1.3, ALLOY, bev=0.002)
    for end, nm in ((a, 'eye low'), (b, 'eye high')):
        side = u.orthogonal().normalized() * 0.022
        cyl(name + ' ' + nm, end - side, end + side, R * 0.42, STEEL, seg=20)
    tube(name + ' spring', helix(a + u * (L * 0.10 + 0.01 + wire), b - u * (L * 0.09 + 0.01 + wire), R,
                                 min(turns, 6) if LOW else turns),
         wire, SPRING, res=3)


# ---------------------------------------------------------------- body tub
TUB_REAR = -0.74


def build_tub():
    prof = [(0.82, 0.30), (0.82, 0.80), (0.30, 1.0), (TUB_REAR, 1.0), (TUB_REAR, 0.30)]
    X = 0.63
    bm = bmesh.new()
    Lv = [bm.verts.new((-X, y, z)) for y, z in prof]
    Rv = [bm.verts.new((X, y, z)) for y, z in prof]
    bm.faces.new(Lv)
    bm.faces.new(Rv)
    n = len(prof)
    for i in range(n):
        if i == 2:  # cockpit opening
            continue
        j = (i + 1) % n
        bm.faces.new((Lv[i], Lv[j], Rv[j], Rv[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    right = [f for f in bm.faces if all(v.co.x > 0 for v in f.verts)][0]
    if right.normal.x < 0:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    o = bm_obj('Body tub', bm, [PAINT, VINYL], ang=20)
    s = o.modifiers.new('Shell', 'SOLIDIFY')
    s.thickness = 0.022
    s.offset = -1
    s.use_even_offset = True
    s.material_offset = 1
    s.material_offset_rim = 0
    bevel(o, 0.007, 3, 30)
    lines = []
    for sd in (-1, 1):
        x, nx = sd * 0.633, (sd, 0, 0)
        lines += [((x, 0.80, 0.335), (x, -0.60, 0.335), nx), ((x, 0.795, 0.35), (x, 0.795, 0.775), nx),
                  ((x, 0.78, 0.792), (x, 0.31, 0.968), nx), ((x, 0.29, 0.968), (x, TUB_REAR + 0.03, 0.968), nx),
                  ((x, TUB_REAR + 0.005, 0.35), (x, TUB_REAR + 0.005, 0.95), nx)]
    yb = TUB_REAR - 0.003
    lines += [((-0.60, yb, 0.335), (0.60, yb, 0.335), (0, -1, 0)), ((-0.60, yb, 0.965), (0.60, yb, 0.965), (0, -1, 0)),
              ((-0.597, yb, 0.36), (-0.597, yb, 0.94), (0, -1, 0)), ((0.597, yb, 0.36), (0.597, yb, 0.94), (0, -1, 0))]
    rivets('Tub rivets', lines, ALLOY)
    for sd in (-1, 1):
        nb = rounded([(sd * 0.645, 0.72, 0.50), (sd * 0.77, 0.67, 0.50), (sd * 0.77, -0.35, 0.50),
                      (sd * 0.645, -0.40, 0.50)], 0.08)
        tube('Nerf bar', nb, 0.024, CHROME)


# ---------------------------------------------------------------- roll cage
LIFT = 0.22  # body, cage, cockpit and engine sit this much higher than the suspension frame
CAGE_TOP = 1.72 + LIFT


def cage_side(s):
    x, z = s * 0.6, 1.0 + LIFT
    return [(x, 0.28, z), (x, 0.02, CAGE_TOP), (x, -1.22, CAGE_TOP), (x, -1.32, z)]


def side_rail(s, y):
    """Point on the cage's bottom side rail at y."""
    loop = rounded(cage_side(s), 0.2, closed=True)
    hits = [p.lerp(q, (y - p.y) / (q.y - p.y)) for p, q in zip(loop, loop[1:] + loop[:1])
            if (p.y - y) * (q.y - y) <= 0 and p.y != q.y and max(p.z, q.z) < 1.0 + LIFT + 0.05]
    return min(hits, key=lambda v: v.z)


REAR_SHOCK_Z = 1.27  # on the cage's lower rear corner, coilover stands upright


def rear_hoop():
    return [(-0.50, -1.84, 0.42), (0.50, -1.84, 0.42), (0.50, -1.82, 0.95 + LIFT), (-0.50, -1.82, 0.95 + LIFT)]


def rear_rail(s):
    return rounded([(s * 0.30, TUB_REAR - 0.02, 0.30 + LIFT), (s * 0.30, -0.86, 0.30), (s * 0.30, -1.58, 0.30),
                    (s * 0.37, -1.83, 0.42)], 0.10)


REAR_POST_Y = (-0.95, -1.45)
REAR_UPPER_PIVOT = (0.40, 0.80)  # x, z on the rear crossmembers


def rear_upper(s):
    return [corner_mid(cage_side(s), 3, 0.2), corner_mid(rear_hoop(), 2 if s > 0 else 3, 0.12)]


def rear_post_top(s, y):
    c3, hoop = rear_upper(s)
    if y >= c3.y:
        return side_rail(s, y)
    return c3.lerp(hoop, (y - c3.y) / (hoop.y - c3.y))


def rear_post_at(s, y, z):
    lo, hi = at_y(rear_rail(s), y), rear_post_top(s, y)
    return lo.lerp(hi, (z - lo.z) / (hi.z - lo.z))


def rear_post(s, z):
    loop = rounded(cage_side(s), 0.2, closed=True)
    for p, q in zip(loop, loop[1:] + loop[:1]):
        if (p.z - z) * (q.z - z) <= 0 and p.y < -1.0 and q.y < -1.0 and p.z != q.z:
            return p.lerp(q, (z - p.z) / (q.z - p.z))
    raise ValueError(z)


def build_cage():
    R, rr = 0.2, 0.04
    loops = {}
    for s in (-1, 1):
        pts = cage_side(s)
        loops[s] = pts
        tube('Cage side ' + ('R' if s > 0 else 'L'), rounded(pts, R, closed=True), rr, PAINT, closed=True)
    names = ['cowl bar', 'front top bar', 'rear top bar']
    for i, nm in enumerate(names):
        a = corner_mid(loops[-1], i, R)
        b = corner_mid(loops[1], i, R)
        tube('Cage ' + nm, [a, b], rr, PAINT)
    tube('Cage rear shock bar', [rear_post(-1, REAR_SHOCK_Z), rear_post(1, REAR_SHOCK_Z)], rr, PAINT)
    tube('Cage rear diagonal', [rear_post(-1, REAR_SHOCK_Z), corner_mid(loops[1], 2, R)], rr * 0.85, PAINT)
    for s in (-1, 1):
        top = corner_mid(loops[s], 2, R)
        tube('Cage rear strut', [top, corner_mid(rear_hoop(), 2 if s > 0 else 3, 0.12)], rr * 0.85, PAINT)
    return loops


# ---------------------------------------------------------------- cockpit
STEER = {}
SEAT_DZ = 0.13


def build_seat():
    """Moulded bucket: a U-section swept along the seating line, then subdivided."""
    path = catmull([(-0.03, 0.46), (-0.28, 0.42), (-0.47, 0.43), (-0.565, 0.55), (-0.60, 0.78),
                    (-0.625, 1.00), (-0.64, 1.20)], 4)
    path = [Vector((0, q.x, q.y + SEAT_DZ)) for q in path]
    n_st = len(path)
    bm = bmesh.new()
    rings = []
    for i, p in enumerate(path):
        t = (path[min(i + 1, n_st - 1)] - path[max(i - 1, 0)]).normalized()
        nrm = Vector((0, t.z, -t.y))
        f = i / (n_st - 1)
        hb = 0.05 + 0.10 * math.sin(min(f * 1.6, 1.0) * pi / 2) + (0.05 if f > 0.85 else 0.0)
        hw = 0.24 - 0.04 * max(0.0, f - 0.7) / 0.3
        prof = [(-hw, hb), (-hw - 0.025, 0.0), (-hw + 0.04, -0.045), (hw - 0.04, -0.045), (hw + 0.025, 0.0),
                (hw, hb), (hw - 0.04, hb), (hw - 0.075, 0.035), (-hw + 0.075, 0.035), (-hw + 0.04, hb)]
        rings.append([bm.verts.new(p + Vector((u, 0, 0)) + nrm * v) for u, v in prof])
    m = len(rings[0])
    for a, b in zip(rings, rings[1:]):
        for j in range(m):
            k = (j + 1) % m
            bm.faces.new((a[j], a[k], b[k], b[j]))
    bm.faces.new(rings[0])
    bm.faces.new(rings[-1][::-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    seat = bm_obj('Bucket seat', bm, [VINYL], ang=180)
    if not LOW:
        sub = seat.modifiers.new('Smooth', 'SUBSURF')
        sub.levels = 2
        sub.render_levels = 2
    harness = material('Harness', '#16161a', rough=0.7)
    up = Vector((0, 0, SEAT_DZ))
    for s in (-1, 1):
        tube('Shoulder belt', catmull([Vector(p) + up for p in ((s * 0.10, -0.70, 1.02), (s * 0.10, -0.60, 1.03),
                                       (s * 0.09, -0.46, 0.92), (s * 0.06, -0.36, 0.62))], 8),
             0.012, harness, res=2)
    tube('Lap belt', catmull([Vector(p) + up for p in ((-0.20, -0.40, 0.48), (-0.12, -0.30, 0.56),
                              (0.12, -0.30, 0.56), (0.20, -0.40, 0.48))], 8), 0.012, harness, res=2)
    box('Belt buckle', (0.07, 0.02, 0.06), (0, -0.31, 0.575 + SEAT_DZ), ALLOY, rot=(0.3, 0, 0), bev=0.006)


def rivets(name, lines, mat, spacing=0.055, r=0.0065):
    if LOW:
        return None
    bm = bmesh.new()
    for a, b, nrm in lines:
        a, b, nrm = Vector(a), Vector(b), Vector(nrm)
        k = max(1, int((b - a).length / spacing))
        q = Vector((0, 0, 1)).rotation_difference(nrm).to_matrix().to_4x4()
        for i in range(k + 1):
            M = Matrix.Translation(a.lerp(b, i / k)) @ q @ Matrix.Diagonal((1, 1, 0.45, 1))
            bmesh.ops.create_uvsphere(bm, u_segments=10, v_segments=6, radius=r, matrix=M)
    return bm_obj(name, bm, [mat], ang=180)


def build_cockpit():
    build_seat()
    box('Dash panel', (1.12, 0.05, 0.16), (0, 0.272, 0.895), SATIN, bev=0.012)
    for x, z, r in ((-0.27, 0.895, 0.058), (0.21, 0.90, 0.04), (0.31, 0.90, 0.04), (0.40, 0.90, 0.032)):
        cyl('Gauge bezel', (x, 0.248, z), (x, 0.236, z), r, CHROME, bev=0.003)
        cyl('Gauge face', (x, 0.247, z), (x, 0.233, z), r * 0.86, GAUGE)
        box('Gauge needle', (r * 0.7, 0.003, 0.004), (x - r * 0.18, 0.231, z + r * 0.18), NEEDLE,
            rot=(0, 0.8, 0), bev=0)
    for x, nm in ((-0.16, 'clutch'), (0.12, 'brake'), (0.21, 'throttle')):
        cyl('Pedal arm ' + nm, (x, 0.75, 0.80), (x, 0.67, 0.60), 0.011, ALLOY, seg=12)
        box('Pedal pad ' + nm, (0.075 if nm != 'throttle' else 0.05, 0.022, 0.10), (x, 0.66, 0.60), ALLOY,
            rot=(-0.35, 0, 0), bev=0.006)
    # steering
    n = Vector((0, -0.9, 0.44)).normalized()
    c = Vector((0, -0.04, 1.05))
    nr = 8 if LOW else 16
    prof = [(0.168 + 0.018 * math.cos(a), 0.018 * math.sin(a)) for a in (2 * pi * k / nr for k in range(nr))]
    piv = finish(bpy.data.objects.new('Steering wheel pivot', None))
    orient(piv, c, n)
    wheel = lathe('Steering wheel rim', prof, material('Leather', '#121212', rough=0.55), seg=64, closed=True,
                  parent=piv)
    STEER['wheel'] = wheel
    for k in range(3):
        a = pi / 2 + k * 2 * pi / 3 + pi
        box('Steering spoke', (0.16, 0.025, 0.008), (0.08 * math.cos(a), 0.08 * math.sin(a), 0),
            ALLOY, rot=(0, 0, a), bev=0.003, parent=wheel)
    cyl('Steering hub', (0, 0, -0.01), (0, 0, 0.025), 0.04, ALLOY, parent=wheel, bev=0.004)
    cyl('Steering column', c - n * 0.01, c - n * 0.34, 0.02, SATIN)
    cyl('Shifter', (0.25, -0.02, 0.35), (0.25, -0.10, 0.76), 0.011, CHROME)
    sphere('Shifter knob', (0.25, -0.10, 0.78), 0.03, SATIN)
    cyl('Extinguisher', (-0.47, 0.17, 0.35), (-0.47, 0.17, 0.70), 0.048, EXTING, bev=0.012)
    cyl('Extinguisher valve', (-0.47, 0.17, 0.70), (-0.47, 0.17, 0.76), 0.016, CHROME)


# ---------------------------------------------------------------- engine
BORES = [0.52, 0.74, 0.96, 1.18]
HEADER_R = 0.05


def build_engine():
    box('Oil pan', (0.36, 0.74, 0.14), (0, 0.86, 0.40), SATIN, bev=0.03, seg=4)
    box('Engine block', (0.44, 0.88, 0.30), (0, 0.85, 0.62), CAST, bev=0.014)
    for sd in (-1, 1):
        for yy in (0.55, 0.85, 1.15):
            cyl('Core plug', (sd * 0.219, yy, 0.64), (sd * 0.226, yy, 0.64), 0.024, ALLOY, seg=24)
        for yy in (0.43, 0.70, 1.00, 1.27):
            box('Block rib', (0.02, 0.025, 0.26), (sd * 0.225, yy, 0.62), CAST, bev=0.006)
    cyl('Oil filter', (0.22, 0.85, 0.47), (0.345, 0.85, 0.47), 0.048, CHROME, bev=0.008)
    cyl('Filter base', (0.215, 0.85, 0.47), (0.235, 0.85, 0.47), 0.055, CAST, bev=0.004)
    cyl('Starter motor', (-0.27, 0.44, 0.44), (-0.27, 0.68, 0.44), 0.05, SATIN, bev=0.01)
    cyl('Starter solenoid', (-0.27, 0.62, 0.52), (-0.27, 0.74, 0.52), 0.025, CHROME, bev=0.004)
    box('Bellhousing', (0.38, 0.09, 0.34), (0, 0.365, 0.58), CAST, bev=0.03, seg=4)
    box('Timing cover', (0.30, 0.05, 0.30), (0, 1.315, 0.66), ALLOY, bev=0.018, seg=4)
    phi = 0.62
    for s in (-1, 1):
        head = bpy.data.objects.new('Bank ' + ('R' if s > 0 else 'L'), None)
        finish(head)
        head.location = (s * 0.23, 0.85, 0.86)
        head.rotation_euler = (0, s * phi, 0)
        box('Cylinder head', (0.24, 0.84, 0.13), (0, 0, 0), CAST, bev=0.01, parent=head)
        box('Valve cover', (0.21, 0.80, 0.10), (0, 0, 0.115), ALLOY, bev=0.035, seg=5, parent=head)
        for fx in (-0.055, 0, 0.055):
            box('Valve cover fin', (0.012, 0.66, 0.022), (fx, 0, 0.165), ALLOY, bev=0.005, parent=head)
        cyl('Breather', (0, -0.33, 0.15), (0, -0.33, 0.20), 0.028, CHROME, parent=head, bev=0.006)
        box('Engine mount', (0.12, 0.12, 0.14), (s * 0.28, 0.62, 0.42), SATIN, bev=0.01)
        ENGINE_MOUNTS.append((s, 0.62))
    # induction
    box('Intake manifold', (0.24, 0.82, 0.16), (0, 0.85, 0.98), ALLOY, bev=0.022, seg=4)
    box('Injector plate', (0.30, 0.86, 0.02), (0, 0.85, 1.068), ALLOY, bev=0.006)
    stack = [(0.042, 0.0), (0.042, 0.012), (0.034, 0.014), (0.033, 0.10), (0.036, 0.125), (0.045, 0.145),
             (0.056, 0.158), (0.061, 0.166), (0.059, 0.172), (0.052, 0.169), (0.043, 0.156), (0.034, 0.136),
             (0.029, 0.10), (0.029, 0.02)]
    if LOW:
        stack = [(0.042, 0.0), (0.033, 0.014), (0.036, 0.125), (0.061, 0.168), (0.029, 0.10)]
    for s in (-1, 1):
        for y in BORES:
            o = lathe('Velocity stack', stack, CHROME, seg=48)
            o.location = (s * 0.068, y, 1.078)
            d = lathe('Stack bore', [(0.0, 0.0), (0.0295, 0.0), (0.0295, 0.001)], SOOT, seg=24)
            d.location = (s * 0.068, y, 1.10)
            cyl('Injector nozzle', (s * 0.118, y, 1.096), (s * 0.095, y, 1.112), 0.006, CHROME, seg=12)
        cyl('Fuel rail', (s * 0.118, 0.44, 1.096), (s * 0.118, 1.26, 1.096), 0.009, CHROME, seg=16)
    cyl('Throttle shaft', (0, 0.42, 1.09), (0, 1.26, 1.09), 0.007, CHROME, seg=12)
    # distributor
    cyl('Distributor', (0, 0.38, 0.92), (0, 0.38, 1.04), 0.035, ALLOY, bev=0.004)
    cyl('Distributor cap', (0, 0.38, 1.03), (0, 0.38, 1.085), 0.052, SATIN, bev=0.01)
    # front accessories
    pulleys = [((0.0, 0.52), 0.09), ((0.0, 0.80), 0.075), ((0.24, 0.86), 0.042)]
    for (x, z), r in pulleys:
        cyl('Pulley', (x, 1.34, z), (x, 1.385, z), r, ALLOY, bev=0.006)
        cyl('Pulley hub', (x, 1.385, z), (x, 1.395, z), r * 0.35, STEEL)
    cyl('Alternator', (0.24, 1.20, 0.86), (0.24, 1.335, 0.86), 0.07, CAST, bev=0.012)
    # belt = convex hull of pulleys
    import itertools
    pts = []
    for (x, z), r in pulleys:
        pts += [(x + (r + 0.004) * math.cos(a), z + (r + 0.004) * math.sin(a)) for a in
                (2 * pi * k / 90 for k in range(0, 90, 8 if LOW else 1))]
    pts = sorted(set(pts))

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    hull = lower[:-1] + upper[:-1]
    bm = bmesh.new()
    a = [bm.verts.new((x, 1.350, z)) for x, z in hull]
    b = [bm.verts.new((x, 1.375, z)) for x, z in hull]
    for i in range(len(hull)):
        j = (i + 1) % len(hull)
        bm.faces.new((a[i], a[j], b[j], b[i]))
    belt = bm_obj('Drive belt', bm, [RUBBER], ang=60)
    so = belt.modifiers.new('Thickness', 'SOLIDIFY')
    so.thickness = 0.006
    so.offset = 0

    # zoomie headers + plug wires: fat chrome pipes as in the original's cockpit view, splayed outwards
    for s in (-1, 1):
        for k, y in enumerate(BORES):
            ctrl = [(s * 0.31, y, 0.80), (s * 0.42, y, 0.786), (s * 0.47, y - 0.04, 0.87),
                    (s * 0.54, y - 0.09, 0.97), (s * 0.575, y - 0.12, 1.05)]
            dense = catmull(ctrl, 24)
            path, L = resample(dense, 0.08 if LOW else 0.004)
            tube('Header primary', path, HEADER_R, CHROME, res=5, caps=False)   # open pipe; the flange hides the head end
            cyl('Header flange', path[0] + Vector((s * 0.025, 0, 0)), path[0] + Vector((s * 0.035, 0, 0)), 0.066,
                ALLOY, bev=0.002)
            # flared tip with a dark mouth
            tip_dir = (dense[-1] - dense[-3]).normalized()
            base = path[-1] - tip_dir * 0.07
            tip_prof = ([(0.052, 0.0), (0.063, 0.08), (0.048, 0.07), (0.045, 0.0)] if LOW else
                        [(0.052, 0.0), (0.053, 0.05), (0.060, 0.072), (0.063, 0.08), (0.057, 0.08), (0.048, 0.06),
                         (0.045, 0.0)])
            tip = lathe('Zoomie tip', tip_prof, CHROME, seg=60)
            orient(tip, base, tip_dir)
            plug = lathe('Pipe soot', [(0.0, 0.0), (0.046, 0.0), (0.046, 0.001)], SOOT, seg=60)
            orient(plug, base + tip_dir * 0.055, tip_dir)
            # spark plug + wire
            yp = y + 0.095 if k < 3 else y - 0.095  # front plug sits behind its port, clear of the coilover
            o_dir = Vector((s * math.cos(0.62), 0, -math.sin(0.62)))
            plug_pt = Vector((s * 0.348, yp, 0.818))
            boot_end = plug_pt + o_dir * 0.065
            if not LOW:
                cyl('Plug boot', plug_pt, boot_end, 0.011, WIRE, seg=16)
            ca = (pi / 2 - s * pi / 2) + s * (k - 1.5) * 0.42
            cap = Vector((0.054 * math.cos(ca), 0.38 + 0.054 * math.sin(ca), 1.058))
            off = (k - 1.5) * 0.011
            wire = [boot_end, boot_end + o_dir * 0.025 + Vector((0, 0, 0.035)),
                    Vector((s * 0.445, yp - 0.09, 0.955 + off)),
                    Vector((s * 0.44, 0.50, 0.965 + off)),
                    Vector((s * 0.33, 0.405, 1.0 + off * 0.6)),
                    cap + Vector((s * 0.05, 0, 0)), cap]
            if not LOW:
                tube('Plug wire', catmull(wire, 14), 0.0055, WIRE, res=3)


# ---------------------------------------------------------------- chassis
ENGINE_MOUNTS = []
ENGINE_SHIFT = 0.50
FY, FR, FW, FX = 1.80, 0.50, 0.45, 1.005   # same tyre all round; track set so the inner faces stay put
RY, RR, RW, RX = -1.15, 0.50, 0.45, 1.015


def bumper_hoop():
    return [(-0.50, 2.32, 0.30), (0.50, 2.32, 0.30), (0.50, 2.30, 0.78 + LIFT), (-0.50, 2.30, 0.78 + LIFT)]


def upper_rail(s):
    top = corner_mid(bumper_hoop(), 2 if s > 0 else 3, 0.12)
    start = corner_mid(cage_side(s), 0, 0.2)
    z = LIFT
    return rounded([start, (s * 0.68, 0.70, 0.99 + z), (s * 0.70, 1.32, 1.00 + z), (s * 0.70, 1.56, 1.00 + z),
                    (s * 0.62, 1.70, 1.00 + z),
                    (s * 0.62, 1.97, 0.99 + z), (s * 0.57, 2.13, 0.88 + z), top], 0.12)


FRONT_HOOP_Y = 1.80




def lower_rail(s):
    return rounded([(s * 0.58, 0.80, 0.30 + LIFT), (s * 0.40, 0.94, 0.33), (s * 0.26, 1.10, 0.33),
                    (s * 0.26, 2.32, 0.30)], 0.10)


def at_y(path, y):
    for p, q in zip(path, path[1:]):
        if (p.y - y) * (q.y - y) <= 0 and p.y != q.y:
            return p.lerp(q, (y - p.y) / (q.y - p.y))
    raise ValueError(y)


POST_Y = (1.533, 1.98)
FRONT_BUMP_GAP = 0.16
UPPER_PIVOT = (0.33, 0.64)  # x, z of the upper wishbone pivots, on a mount rail below the headers
FRONT_SHOCK_BAR = (0.40, 0.56)  # crossbar positions on the lower / upper wishbone


def upper_pivot(s, y):
    return Vector((s * UPPER_PIVOT[0], y, UPPER_PIVOT[1]))


def build_chassis():
    rr = 0.038
    tube('Front bumper hoop', rounded(bumper_hoop(), 0.12, closed=True), rr, PAINT, closed=True)
    tube('Bumper centre bar', [Vector((-0.50, 2.315, 0.62)), Vector((0.50, 2.315, 0.62))], 0.028, PAINT)
    for s in (-1, 1):
        up, lo = upper_rail(s), lower_rail(s)
        tube('Front upper rail', up, rr, PAINT)
        tube('Front lower rail', lo, 0.036, PAINT)
        # rear: short tower under the headers; front: post up to the raised rail
        tube('Suspension post', [at_y(lo, POST_Y[0]), upper_pivot(s, POST_Y[0])], 0.034, PAINT)
        tube('Suspension post', [at_y(lo, POST_Y[1]), upper_pivot(s, POST_Y[1]), at_y(up, POST_Y[1])], 0.034, PAINT)
        tube('Upper arm mount rail', [upper_pivot(s, POST_Y[0]), upper_pivot(s, POST_Y[1])], 0.03, PAINT)
    for s, y in ENGINE_MOUNTS:
        y += ENGINE_SHIFT
        top = Vector((s * 0.28, y, 0.42 + LIFT - 0.07))
        cyl('Engine mount leg', at_y(lower_rail(s), y) + Vector((0, 0, 0.02)), top, 0.026, PAINT, seg=16)
    for y, z in ((1.02, 0.30), (2.05, 0.31)):
        x = abs(at_y(lower_rail(1), y).x)
        tube('Front crossmember', [Vector((-x, y, z)), Vector((x, y, z))], 0.026, PAINT)


# ---------------------------------------------------------------- wheels
def wheel(name, R, W, loc, side):
    e = bpy.data.objects.new(name, None)
    finish(e)
    e.location = loc
    e.rotation_euler = (0, side * pi / 2, 0)
    h = W / 2
    lug_h = 0.014 if LOW else 0.024   # game version: shallow knobs so the carcass between them stays near the road
    Rc = R - lug_h + 0.004
    Rr = R * 0.56
    c = 0.07
    prof = [(Rr * 1.02, -h * 0.80), (Rr * 1.12, -h * 0.93), (Rr + (Rc - Rr) * 0.55, -h * 1.0)]
    na = 3 if LOW else 8
    prof += [(Rc - c + c * math.cos(a), -h + c + c * math.sin(a)) for a in
             (-pi / 2 + k / na * pi / 2 for k in range(1, na + 1))]
    if not LOW:
        prof += [(Rc, 0.0)]
    prof += [(Rc - c + c * math.cos(a), h - c + c * math.sin(a)) for a in (k / na * pi / 2 for k in range(0, na))]
    prof += [(Rr + (Rc - Rr) * 0.55, h * 1.0), (Rr * 1.12, h * 0.93), (Rr * 1.02, h * 0.80)]
    lathe(name + ' tyre', prof, RUBBER, seg=72 if LOW else 96, parent=e, ang=50)
    # chevron tread blocks
    bm = bmesh.new()
    N = 22 if LOW else int(round(2 * pi * Rc / 0.15))
    for row, (zc, tw) in enumerate(((W * 0.22, 0.28), (-W * 0.22, -0.28))):
        for k in range(N):
            a = 2 * pi * (k + 0.5 * row) / N
            M = (Matrix.Rotation(a, 4, 'Z') @ Matrix.Translation((Rc + lug_h / 2 - 0.004, 0, zc))
                 @ Matrix.Rotation(tw, 4, 'X') @ Matrix.Diagonal((lug_h, 0.085, W * 0.36, 1)))
            bmesh.ops.create_cube(bm, size=1.0, matrix=M)
    lugs = bm_obj(name + ' tread', bm, [RUBBER], parent=e)
    bevel(lugs, 0.005, 2)
    # deep-dish rim with beadlock ring
    rim = [(Rr * 0.97, -h * 0.80), (Rr, -h * 0.80), (Rr, h * 0.74), (Rr * 1.07, h * 0.80), (Rr * 1.08, h * 0.86),
           (Rr * 1.03, h * 0.88), (Rr * 0.95, h * 0.84), (Rr * 0.93, h * 0.66), (Rr * 0.89, h * 0.36),
           (Rr * 0.80, h * 0.26), (Rr * 0.46, h * 0.26), (Rr * 0.34, h * 0.33), (Rr * 0.24, h * 0.38),
           (0.0, h * 0.38)]
    if LOW:
        rim = [(Rr, -h * 0.80), (Rr, h * 0.74), (Rr * 1.08, h * 0.86), (Rr * 0.93, h * 0.70), (Rr * 0.80, h * 0.26),
               (Rr * 0.30, h * 0.30), (0.0, h * 0.38)]
    lathe(name + ' rim', rim, ALLOY, seg=72 if LOW else 96, parent=e, ang=25)
    ring = [(Rr * 0.99, h * 0.86), (Rr * 1.20, h * 0.86), (Rr * 1.21, h * 0.88), (Rr * 1.21, h * 0.93),
            (Rr * 1.20, h * 0.95), (Rr * 0.995, h * 0.95), (Rr * 0.985, h * 0.93), (Rr * 0.985, h * 0.88)]
    if LOW:
        ring = [(Rr * 0.99, h * 0.86), (Rr * 1.21, h * 0.86), (Rr * 1.21, h * 0.95), (Rr * 0.99, h * 0.95)]
    lathe(name + ' beadlock', ring, ALLOY, seg=72 if LOW else 96, parent=e, closed=True, ang=30)
    for k in range(0 if LOW else 24):
        a = 2 * pi * k / 24
        p = Vector((Rr * 1.105 * math.cos(a), Rr * 1.105 * math.sin(a), h * 0.95))
        cyl(name + ' bolt', p, p + Vector((0, 0, 0.012)), 0.0085, CHROME, seg=6, parent=e)
    cyl(name + ' hub', (0, 0, h * 0.30), (0, 0, h * 0.30 + 0.05), Rr * 0.20, ALLOY, parent=e, bev=0.006)
    if LOW:
        cyl(name + ' hub cap', (0, 0, h * 0.30 + 0.05), (0, 0, h * 0.30 + 0.08), Rr * 0.13, CHROME, seg=8, parent=e,
            r2=Rr * 0.07)
    else:
        sphere(name + ' hub cap', (0, 0, h * 0.30 + 0.05), Rr * 0.13, CHROME, scale=(1, 1, 0.55), parent=e)
    for k in range(0 if LOW else 8):
        a = 2 * pi * k / 8 + pi / 8
        p = Vector((Rr * 0.34 * math.cos(a), Rr * 0.34 * math.sin(a), h * 0.32))
        cyl(name + ' lug nut', p, p + Vector((0, 0, 0.028)), 0.014, CHROME, seg=6, parent=e)
    cyl(name + ' brake disc', (0, 0, -h * 0.55), (0, 0, -h * 0.47), Rr * 0.75, STEEL, parent=e)
    return e


def bush(name, p, axis, r=0.026, L=0.055):
    p, axis = Vector(p), Vector(axis).normalized()
    cyl(name, p - axis * L / 2, p + axis * L / 2, r, SATIN, seg=20)


def clevis(name, p, bolt_axis, out, size=0.07, gap=0.05):
    """Double-plate bracket around a rod end at p, plates normal to bolt_axis, extending along out."""
    if LOW:
        return
    p, ax, out = Vector(p), Vector(bolt_axis).normalized(), Vector(out).normalized()
    up = ax.cross(out).normalized()
    M = Matrix((out, up, ax)).transposed().to_4x4()
    for k in (-1, 1):
        o = box(name, (size * 1.3, size, 0.008), (0, 0, 0), SATIN, bev=0.003)
        o.matrix_world = Matrix.Translation(p + ax * k * gap / 2 + out * size * 0.35) @ M
    cyl(name + ' bolt', p - ax * (gap / 2 + 0.012), p + ax * (gap / 2 + 0.012), 0.008, STEEL, seg=12)


def link(name, a, b, r, bolt_a, bolt_b, mat=None):
    """Heavy link with rod ends at both ends."""
    a, b = Vector(a), Vector(b)
    u = (b - a).normalized()
    cyl(name, a + u * 0.05, b - u * 0.05, r, mat or SATIN, seg=20, bev=0.004)
    for p, q, ax in ((a, u, bolt_a), (b, -u, bolt_b)):
        if not LOW:
            cyl(name + ' jam nut', p + q * 0.045, p + q * 0.06, r * 0.9, STEEL, seg=6)
            cyl(name + ' rod end shank', p + q * 0.02, p + q * 0.05, r * 0.55, ALLOY, seg=16)
        ax = Vector(ax).normalized()
        cyl(name + ' rod end', p - ax * 0.016, p + ax * 0.016, r * 1.05, ALLOY, seg=20, bev=0.003)


def wishbone(name, p1, p2, apex, r=0.024, bar=0.45):
    p1, p2, apex = Vector(p1), Vector(p2), Vector(apex)
    for p in (p1, p2):
        cyl(name, p, apex, r, SATIN, seg=20)
    g1, g2 = apex.lerp(p1, 0.38), apex.lerp(p2, 0.38)
    bm = bmesh.new()
    bm.faces.new([bm.verts.new(v) for v in (apex, g1, g2)])
    o = bm_obj(name + ' gusset', bm, [SATIN], ang=30)
    so = o.modifiers.new('Plate', 'SOLIDIFY')
    so.thickness = 0.008
    so.offset = 0
    bevel(o, 0.002, 1)
    cyl(name + ' crossbar', p1.lerp(apex, bar), p2.lerp(apex, bar), r * 0.75, SATIN, seg=16)


KINGPIN_X = 0.76
ARM_TIP = Vector((0.72, FY + 0.12, FR - 0.06))
RACK = (0.26, FY + 0.12, FR - 0.08)
WHEELBASE = FY - RY
STEER_RATIO = 12.0
STEER_MAX = 33.0  # the wheels touch the frame at 37 deg: 4 deg margin


def wheel_angle_expr(sg):
    k = sg * KINGPIN_X
    d = 'radians(steer)'
    return f'atan2({WHEELBASE:.4f}*sin({d}), {WHEELBASE:.4f}*cos({d}) + {k:.4f}*sin({d}))'


def add_driver(obj, path, index, expr):
    fc = obj.driver_add(path, index)
    while fc.keyframe_points:  # plain pass-through, no mapping curve
        fc.keyframe_points.remove(fc.keyframe_points[0])
    for m in list(fc.modifiers):
        fc.modifiers.remove(m)
    drv = fc.driver
    drv.type = 'SCRIPTED'
    v = drv.variables.new()
    v.name = 'steer'
    v.type = 'SINGLE_PROP'
    v.targets[0].id_type = 'OBJECT'
    v.targets[0].id = ROOT
    v.targets[0].data_path = '["steer"]'
    drv.expression = expr
    return drv


def build_front_corner(s):
    up, lo = upper_rail(s), lower_rail(s)
    Yax = Vector((0, 1, 0))
    lo_apex = Vector((s * 0.735, FY, FR - 0.16))
    up_apex = Vector((s * 0.735, FY, FR + 0.17))
    piv_l = [at_y(lo, y) for y in POST_Y]
    piv_u = [upper_pivot(s, y) for y in POST_Y]
    for (pa, pb), apex, nm, bar in ((piv_l, lo_apex, 'Lower wishbone', FRONT_SHOCK_BAR[0]),
                                    (piv_u, up_apex, 'Upper wishbone', FRONT_SHOCK_BAR[1])):
        wishbone(nm, pa, pb, apex, bar=bar)
        for p in (pa, pb):
            bush(nm + ' bush', p, Yax)
            clevis(nm + ' bracket', p, Yax, Vector((-s, 0, 0)), size=0.06, gap=0.06)
    for apex in (lo_apex, up_apex):
        cyl('Ball joint housing', apex - Vector((0, 0, 0.025)), apex + Vector((0, 0, 0.025)), 0.036, STEEL,
            bev=0.004)
    # steered parts hang from a pivot on the kingpin axis
    pivot = finish(bpy.data.objects.new('Steering pivot ' + ('R' if s > 0 else 'L'), None))
    pivot.location = (s * KINGPIN_X, FY, 0)
    inv = Matrix.Translation(pivot.location).inverted()
    steered = []
    steered.append(wheel('Front wheel ' + ('R' if s > 0 else 'L'), FR, FW, (s * FX, FY, FR), s))
    steered.append(cyl('Hub carrier', (s * 0.745, FY, FR), (s * 0.805, FY, FR), 0.092, CAST, bev=0.012))
    for z0, z1, nm in ((FR + 0.05, up_apex.z, 'upper'), (FR - 0.05, lo_apex.z, 'lower')):
        zc, hz = (z0 + z1) / 2, abs(z1 - z0)
        steered.append(box('Knuckle arm ' + nm, (0.05, 0.075, hz + 0.03), (s * 0.748, FY, zc), CAST, bev=0.014))
        steered.append(cyl('Knuckle eye ' + nm, (s * 0.735, FY, z1 - 0.03), (s * 0.735, FY, z1 + 0.03), 0.034,
                           CAST, bev=0.005))
    steered.append(cyl('Spindle', (s * 0.805, FY, FR), (s * (FX - 0.11), FY, FR), 0.045, STEEL, bev=0.004))
    steered.append(box('Brake caliper', (0.06, 0.11, 0.07), (s * 0.80, FY - 0.17, FR + 0.06), material(
        'Caliper', '#b51d10', rough=0.3, coat=1.0), rot=(0.5, 0, 0), bev=0.012))
    tip = Vector((s * ARM_TIP.x, ARM_TIP.y, ARM_TIP.z))
    steered.append(cyl('Steering arm', (s * 0.77, FY + 0.05, ARM_TIP.z), tip, 0.02, CAST, seg=16, bev=0.004))
    steered.append(sphere('Tie rod end', tip, 0.022, STEEL))
    tip_e = finish(bpy.data.objects.new('Tie rod target ' + ('R' if s > 0 else 'L'), None))
    tip_e.location = tip
    steered.append(tip_e)
    for o in steered:
        o.parent = pivot
        o.matrix_parent_inverse = inv
    add_driver(pivot, 'rotation_euler', 2, wheel_angle_expr(s))
    # tie rod: inner end rides with the rack, outer end follows the arm (Stretch To)
    inner = finish(bpy.data.objects.new('Tie rod inner ' + ('R' if s > 0 else 'L'), None))
    inner.location = (s * RACK[0], RACK[1], RACK[2])
    ax, ay = s * (ARM_TIP.x - KINGPIN_X), ARM_TIP.y - FY
    th = wheel_angle_expr(s)
    add_driver(inner, 'location', 0, f'{s * RACK[0]:.4f} + {ax:.4f}*(cos({th}) - 1) - {ay:.4f}*sin({th})')
    L0 = (tip - Vector(inner.location)).length
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=16, radius1=0.014, radius2=0.014, depth=L0 - 0.04,
                          matrix=Matrix.Translation((0, L0 / 2, 0)) @ Matrix.Rotation(-pi / 2, 4, 'X'))
    rod = bm_obj('Tie rod', bm, [CHROME], parent=inner, ang=40)
    c = rod.constraints.new('STRETCH_TO')
    c.target = tip_e
    c.rest_length = L0
    c.volume = 'NO_VOLUME'
    sphere('Inner tie rod joint', (0, 0, 0), 0.02, STEEL, parent=inner)
    # coilover: lower-arm crossbar -> front shock hoop, vertical
    f = FRONT_SHOCK_BAR[0]
    c0, c1 = piv_l[0].lerp(lo_apex, f), piv_l[1].lerp(lo_apex, f)
    low = c0.lerp(c1, (FRONT_HOOP_Y - c0.y) / (c1.y - c0.y)) + Vector((0, 0, 0.04))
    rp = at_y(up, FRONT_HOOP_Y)
    node = Vector((low.x, FRONT_HOOP_Y, rp.z - 0.06))
    tube('Shock mount arm', [rp, node], 0.03, PAINT)
    tube('Shock mount brace', [node, at_y(up, FRONT_HOOP_Y + 0.15)], 0.022, PAINT)
    top = node - Vector((0, 0, 0.065))
    box('Shock tab', (0.022, 0.07, 0.06), node - Vector((0, 0, 0.04)), PAINT, bev=0.004)
    coilover('Front coilover', low, top, R=0.058, wire=0.0115, turns=9, reservoir=(s, -1, 0))
    # bump stop hanging from the upper rail, striking the upper wishbone crossbar (clear of the ball joint)
    c0, c1 = piv_u[0].lerp(up_apex, FRONT_SHOCK_BAR[1]), piv_u[1].lerp(up_apex, FRONT_SHOCK_BAR[1])
    yb = 1.70
    g = c0.lerp(c1, (yb - c0.y) / (c1.y - c0.y)) + Vector((0, 0, 0.018))
    rp = at_y(up, yb)
    zb = g.z + FRONT_BUMP_GAP
    cyl('Bump stop body', Vector((g.x, yb, rp.z - 0.03)), Vector((g.x, yb, zb + 0.025)), 0.03, ALLOY, bev=0.004)
    pad = cyl('Bump stop pad', Vector((g.x, yb, zb + 0.025)), Vector((g.x, yb, zb)), 0.034, RUBBER, bev=0.006)
    pad['contact'] = list(g)
    box('Bump stop tab', (abs(rp.x - g.x) + 0.05, 0.05, 0.04), ((rp.x + g.x) / 2, yb, rp.z - 0.03), PAINT,
        bev=0.004)
    # front droop is limited inside the shocks (full extension), see coilover()


def build_rear_frame():
    rr = 0.038
    tube('Rear bumper hoop', rounded(rear_hoop(), 0.12, closed=True), rr, PAINT, closed=True)
    tube('Rear bumper centre bar', [Vector((-0.50, -1.835, 0.70)), Vector((0.50, -1.835, 0.70))], 0.028, PAINT)
    for s in (-1, 1):
        rail = rear_rail(s)
        tube('Rear lower rail', rail, 0.036, PAINT)
        tube('Rear upper rail', rear_upper(s), rr, PAINT)
        for y in REAR_POST_Y:
            tube('Rear suspension post', [at_y(rail, y), rear_post_top(s, y)], 0.034, PAINT)
        cyl('Tail light housing', (s * 0.30, -1.84, 0.88 + LIFT), (s * 0.30, -1.87, 0.88 + LIFT), 0.055, SATIN, bev=0.006)
        cyl('Tail light lens', (s * 0.30, -1.868, 0.88 + LIFT), (s * 0.30, -1.882, 0.88 + LIFT), 0.045, LENS, bev=0.005)
        box('Tail light bracket', (0.03, 0.03, 0.06), (s * 0.30, -1.83, 0.925 + LIFT), SATIN, bev=0.004)
        tube('Fuel cell tray rail', [Vector((s * 0.25, REAR_POST_Y[1], 0.80 + LIFT)),
                                     Vector((s * 0.25, -1.825, 0.80 + LIFT))],
             0.02, PAINT)
    for y in REAR_POST_Y:
        tube('Rear crossmember', [rear_post_at(-1, y, 0.80), rear_post_at(1, y, 0.80)], 0.03, PAINT)
        tube('Rear floor crossmember', [at_y(rear_rail(-1), y), at_y(rear_rail(1), y)], 0.03, PAINT)
    tube('Fuel cell tray bar', [Vector((-0.50, -1.825, 0.80 + LIFT)), Vector((0.50, -1.825, 0.80 + LIFT))], 0.024, PAINT)
    tube('Fuel cell tray bar', [rear_post_at(-1, REAR_POST_Y[1], 0.80 + LIFT), rear_post_at(1, REAR_POST_Y[1], 0.80 + LIFT)],
         0.024, PAINT)
    box('Fuel cell', (0.72, 0.26, 0.25), (0, -1.64, 0.95 + LIFT), SATIN, bev=0.02, seg=3)
    for x in (-0.22, 0.22):
        box('Fuel cell strap', (0.05, 0.28, 0.262), (x, -1.64, 0.95 + LIFT), material('Strap red', '#9e1209', rough=0.6),
            bev=0.006)
    cyl('Fuel filler neck', (0.10, -1.60, 1.075 + LIFT), (0.10, -1.60, 1.10 + LIFT), 0.055, ALLOY, bev=0.005)
    cyl('Fuel filler cap', (0.10, -1.60, 1.10 + LIFT), (0.10, -1.60, 1.12 + LIFT), 0.06, CHROME, bev=0.006)
    tube('Fuel line', catmull([(-0.20, -1.51, 0.86 + LIFT), (-0.20, -1.30, 0.87 + LIFT), (-0.20, -0.85, 0.87 + LIFT),
                               (-0.20, TUB_REAR - 0.025, 0.70 + LIFT), (-0.20, TUB_REAR - 0.025, 0.33 + LIFT)], 10),
         0.009, WIRE,
         res=2)


def build_tunnel():
    """Driveshaft tunnel along the cockpit floor; the seat sits on top of it."""
    w, floor = 0.085, 0.31
    stations = [(0.80, 0.58), (0.52, 0.44), (TUB_REAR + 0.022, 0.52)]
    bm = bmesh.new()
    rings = []
    for y, top in stations:
        prof = [(-w, floor), (-w, top - w)]
        prof += [(-w * math.cos(a), top - w + w * math.sin(a)) for a in (pi * k / 10 for k in range(1, 10))]
        prof += [(w, top - w), (w, floor)]
        rings.append([bm.verts.new((x, y, z)) for x, z in prof])
    for a, b in zip(rings, rings[1:]):
        for j in range(len(a) - 1):
            bm.faces.new((a[j], a[j + 1], b[j + 1], b[j]))
    o = bm_obj('Driveshaft tunnel', bm, [VINYL], ang=50)
    so = o.modifiers.new('Shell', 'SOLIDIFY')
    so.thickness = 0.012


def bellows(name, a, b, r0, r1, mat=RUBBER, ribs=5):
    if LOW:
        ribs = 2
    a, b = Vector(a), Vector(b)
    L = (b - a).length
    prof = []
    n = ribs * 4
    for k in range(n + 1):
        t = k / n
        prof.append((r0 + (r1 - r0) * math.sin(t * pi) ** 0.6 * 0.85 + 0.006 * math.cos(t * ribs * 2 * pi), t * L))
    o = lathe(name, [(0.0, 0.0)] + prof + [(0.0, L)], mat, seg=32, ang=60)
    orient(o, a, b - a)
    return o


def build_rear_corner(s):
    Yax = Vector((0, 1, 0))
    rail = rear_rail(s)
    lo_apex = Vector((s * 0.77, RY, RR - 0.21))
    up_apex = Vector((s * 0.77, RY, RR + 0.22))
    piv_l = [at_y(rail, y) for y in REAR_POST_Y]
    piv_u = [Vector((s * REAR_UPPER_PIVOT[0], y, REAR_UPPER_PIVOT[1])) for y in REAR_POST_Y]
    for (pa, pb), apex, nm, bar in ((piv_l, lo_apex, 'Rear lower wishbone', REAR_SHOCK_BAR),
                                    (piv_u, up_apex, 'Rear upper wishbone', 0.50)):
        wishbone(nm, pa, pb, apex, bar=bar)
        for p in (pa, pb):
            bush(nm + ' bush', p, Yax)
            clevis(nm + ' bracket', p, Yax, Vector((-s, 0, 0)), size=0.06, gap=0.06)
    for apex in (lo_apex, up_apex):
        cyl('Rear ball joint housing', apex - Vector((0, 0, 0.025)), apex + Vector((0, 0, 0.025)), 0.036, STEEL,
            bev=0.004)
    # upright group: hub carrier, hub, brake and wheel move together
    grp = finish(bpy.data.objects.new('Rear upright ' + ('R' if s > 0 else 'L'), None))
    parts = [wheel('Rear wheel ' + ('R' if s > 0 else 'L'), RR, RW, (s * RX, RY, RR), s)]
    parts.append(cyl('Rear hub carrier', (s * 0.745, RY, RR), (s * 0.825, RY, RR), 0.10, CAST, bev=0.012))
    for z1, nm in ((up_apex.z, 'upper'), (lo_apex.z, 'lower')):
        z0 = RR + (0.05 if z1 > RR else -0.05)
        parts.append(box('Rear knuckle arm ' + nm, (0.05, 0.08, abs(z1 - z0) + 0.03), (s * 0.758, RY, (z0 + z1) / 2),
                         CAST, bev=0.014))
        parts.append(cyl('Rear knuckle eye ' + nm, (s * 0.77, RY, z1 - 0.03), (s * 0.77, RY, z1 + 0.03), 0.034, CAST,
                         bev=0.005))
    toe_end = Vector((s * 0.76, RY + 0.15, RR - 0.04))
    parts.append(cyl('Rear toe arm', (s * 0.765, RY + 0.06, RR - 0.04), toe_end, 0.022, CAST, seg=16, bev=0.004))
    parts.append(cyl('Rear hub', (s * 0.815, RY, RR), (s * (RX - 0.12), RY, RR), 0.075, STEEL, bev=0.006))
    parts.append(box('Rear brake caliper', (0.06, 0.11, 0.07), (s * 0.80, RY + 0.17, RR + 0.06),
                     bpy.data.materials['Caliper'], rot=(-0.5, 0, 0), bev=0.012))
    for o in parts:
        o.parent = grp
    # toe link to the front post
    toe_in = rear_post_at(s, REAR_POST_Y[0], toe_end.z) + Vector((0, -0.04, 0))
    link('Rear toe link', toe_in, toe_end, 0.016, Yax, Yax, mat=CHROME)
    box('Toe link tab', (0.03, 0.06, 0.05), toe_in + Vector((0, 0.02, 0)), PAINT, bev=0.004)
    # half-shaft with CV boots
    cyl('Diff output flange', (s * 0.15, RY, RR), (s * 0.215, RY, RR), 0.07, CAST, bev=0.008)
    bellows('Inner CV boot', (s * 0.21, RY, RR), (s * 0.31, RY, RR), 0.05, 0.068)
    cyl('Half-shaft', (s * 0.30, RY, RR), (s * 0.66, RY, RR), 0.03, STEEL, seg=24)
    bellows('Outer CV boot', (s * 0.65, RY, RR), (s * 0.75, RY, RR), 0.045, 0.062)
    # coilover: rear leg of the lower wishbone -> cage shock bar
    xb = piv_l[1].lerp(lo_apex, REAR_SHOCK_BAR)
    low = Vector((xb.x, -1.255, xb.z + 0.04))
    top_y = rear_post(s, REAR_SHOCK_Z).y
    top = Vector((xb.x, top_y, REAR_SHOCK_Z - 0.075))
    box('Rear shock tab', (0.022, 0.07, 0.06), (xb.x, top_y, REAR_SHOCK_Z - 0.05), PAINT, bev=0.004)
    coilover('Rear coilover', low, top, R=0.06, wire=0.012, turns=10, reservoir=(-s, 0.6, 0))
    # bump stop under the cage side rail, landing on the upper wishbone gusset
    c0, c1 = piv_u[0].lerp(up_apex, 0.50), piv_u[1].lerp(up_apex, 0.50)
    yb = -1.10
    g = c0.lerp(c1, (yb - c0.y) / (c1.y - c0.y)) + Vector((0, 0, 0.018))
    zt = side_rail(s, yb).z - 0.03
    zb = g.z + REAR_BUMP_GAP
    cyl('Rear bump stop body', Vector((g.x, yb, zt)), Vector((g.x, yb, zb + 0.025)), 0.03, ALLOY, bev=0.004)
    pad = cyl('Rear bump stop pad', Vector((g.x, yb, zb + 0.025)), Vector((g.x, yb, zb)), 0.034, RUBBER, bev=0.006)
    pad['contact'] = list(g)
    box('Rear bump stop tab', (abs(abs(g.x) - 0.6) + 0.06, 0.06, 0.04), (s * (0.6 + abs(g.x)) / 2, yb, zt + 0.015),
        PAINT, bev=0.004)
    # droop is limited inside the shocks (full extension), see coilover()


REAR_BUMP_GAP = 0.185
REAR_SHOCK_BAR = (0.47 - 0.30) / (0.77 - 0.30)  # coilover sits at x 0.47


def build_rear():
    # differential bolted to the frame
    sphere('Differential', (0, RY, RR), 0.19, CAST, scale=(1.0, 0.85, 1.0))
    cyl('Diff cover', (0, RY - 0.14, RR), (0, RY - 0.18, RR), 0.14, ALLOY, bev=0.014)
    for k in range(0 if LOW else 10):
        a = 2 * pi * k / 10
        p = Vector((0.115 * math.cos(a), RY - 0.18, RR + 0.115 * math.sin(a)))
        cyl('Diff cover bolt', p, p - Vector((0, 0.012, 0)), 0.009, CHROME, seg=6)
    cyl('Pinion housing', (0, RY + 0.12, RR - 0.03), (0, RY + 0.24, RR - 0.06), 0.075, CAST, bev=0.01)
    for sd in (-1, 1):
        for y in (RY + 0.08, RY - 0.08):
            cyl('Diff mount strut', (sd * 0.13, y, RR - 0.12), at_y(rear_rail(sd), y) + Vector((0, 0, 0.02)), 0.022,
                SATIN, seg=16)
    cyl('Diff top strut', (0, RY + 0.06, RR + 0.17), (0, REAR_POST_Y[0], 0.80), 0.022, SATIN, seg=16)
    trans_out = Vector((0, 0.52, 0.375 + LIFT))
    pinion = Vector((0, RY + 0.24, RR - 0.06))
    cyl('Transmission', (0, 0.86, 0.50 + LIFT), trans_out, 0.07, CAST, bev=0.012)
    cyl('Driveshaft', trans_out, pinion, 0.038, STEEL)
    for p in (trans_out, pinion):
        sphere('U-joint', p, 0.048, STEEL)
    at_wall = trans_out.lerp(pinion, (trans_out.y - TUB_REAR) / (trans_out.y - pinion.y))
    cyl('Driveshaft boot', at_wall + Vector((0, 0.005, 0)), at_wall - Vector((0, 0.025, 0)), 0.065, RUBBER, bev=0.006)
    for s in (-1, 1):
        build_rear_corner(s)


def build_running_gear():
    for s in (-1, 1):
        build_front_corner(s)
    cyl('Steering rack', (-RACK[0] + 0.02, RACK[1], RACK[2]), (RACK[0] - 0.02, RACK[1], RACK[2]), 0.034, ALLOY,
        bev=0.006)
    for s in (-1, 1):
        cyl('Rack boot', (s * (RACK[0] - 0.02), RACK[1], RACK[2]), (s * (RACK[0] - 0.08), RACK[1], RACK[2]), 0.026,
            RUBBER, seg=20)
    build_rear_frame()
    build_rear()
    ROOT['steer'] = 0.0
    ROOT.id_properties_ui('steer').update(min=-STEER_MAX, max=STEER_MAX, soft_min=-STEER_MAX, soft_max=STEER_MAX,
                                          step=100, precision=1, subtype='NONE',
                                          description='Steering input in degrees; each front wheel gets its own '
                                                      'Ackermann angle')
    add_driver(STEER['wheel'], 'rotation_euler', 2, f'{STEER_RATIO} * radians(steer)')


def build_driver():
    """Seated 1.78 m mannequin used to check cockpit fit; not exported."""
    col = bpy.data.collections.new('Driver fit check')
    scn.collection.children.link(col)
    skin = material('Mannequin', '#8a8f96', rough=0.6)
    parts = []

    def limb(a, b, r):
        parts.append(cyl('Driver limb', a, b, r, skin, seg=20))
        if not LOW:
            parts.append(sphere('Driver joint', a, r, skin))
            parts.append(sphere('Driver joint', b, r, skin))
    def v(x, y, z):
        return Vector((x, y, z + SEAT_DZ))
    parts.append(sphere('Driver pelvis', v(0, -0.33, 0.52), 0.17, skin, scale=(1.0, 0.8, 0.6)))
    limb(v(0, -0.36, 0.60), v(0, -0.50, 0.92), 0.15)
    limb(v(0, -0.51, 1.02), v(0, -0.52, 1.10), 0.05)
    parts.append(sphere('Driver helmet', v(0, -0.50, 1.20), 0.13, skin))
    for x in (-1, 1):
        limb(v(x * 0.10, -0.30, 0.52), v(x * 0.14, 0.08, 0.68), 0.075)
        limb(v(x * 0.14, 0.08, 0.68), v(x * 0.15, 0.50, 0.43), 0.055)
        limb(v(x * 0.15, 0.50, 0.43), v(x * 0.15, 0.64, 0.50), 0.045)
        limb(v(x * 0.20, -0.48, 0.96), v(x * 0.24, -0.25, 0.84), 0.05)
        limb(v(x * 0.24, -0.25, 0.84), Vector((x * 0.15, -0.07, 1.10)), 0.04)
    for o in parts:
        CAR.objects.unlink(o)
        col.objects.link(o)
    col.hide_render = True


def lifted(build, dy=0.0):
    before = set(CAR.objects)
    build()
    for o in set(CAR.objects) - before:
        if o.parent is None:
            o.location.y += dy
            o.location.z += LIFT


lifted(build_tub)
build_cage()
lifted(build_cockpit)
lifted(build_tunnel)
lifted(build_engine, ENGINE_SHIFT)
build_chassis()
build_running_gear()
build_driver()
for o in bpy.data.collections['Driver fit check'].objects:
    o.location.z += LIFT

# parent everything to one root so the car moves as a unit
for o in list(CAR.objects):
    if o is not ROOT and o.parent is None:
        o.parent = ROOT

# curves -> meshes so the .blend and .glb are plain geometry
dg = bpy.context.evaluated_depsgraph_get()
for o in [o for o in CAR.objects if o.type == 'CURVE']:
    me = bpy.data.meshes.new_from_object(o.evaluated_get(dg))
    shade(me, 60)
    n = bpy.data.objects.new(o.name, me)
    for k in o.keys():
        n[k] = o[k]
    CAR.objects.link(n)
    n.parent = o.parent
    n.matrix_basis = o.matrix_basis.copy()
    for ch in list(o.children):
        ch.parent = n
    bpy.data.objects.remove(o)


# ---------------------------------------------------------------- environment
def env_link(o):
    ENV.objects.link(o)
    return o


ASPHALT = material('Asphalt', '#38393b', rough=0.9, spec=0.4)
noise_detail(ASPHALT, 220, bump=0.35, dist=0.002, rough=(0.75, 0.95), color2='#4a4b4c', cscale=0.6)
bm = bmesh.new()
bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=200)
me = bpy.data.meshes.new('Ground')
bm.to_mesh(me)
me.materials.append(ASPHALT)
env_link(bpy.data.objects.new('Ground', me))

world = bpy.data.worlds.new('Sky')
scn.world = world
try:
    world.use_nodes = True
except Exception:
    pass
wn = world.node_tree
bg = wn.nodes.get('Background') or wn.nodes.new('ShaderNodeBackground')
sky = wn.nodes.new('ShaderNodeTexSky')
sky.sky_type = 'MULTIPLE_SCATTERING'
sky.sun_disc = False
sky.aerosol_density = 0.4
sky.air_density = 1.4
sun_dir = Vector((0.55, 0.65, 0.62)).normalized()
sky.sun_elevation = math.asin(sun_dir.z)
sky.sun_rotation = math.atan2(sun_dir.x, sun_dir.y)
wn.links.new(sky.outputs['Color'], bg.inputs['Color'])
bg.inputs['Strength'].default_value = 0.42
out = next((n for n in wn.nodes if n.type == 'OUTPUT_WORLD'), None) or wn.nodes.new('ShaderNodeOutputWorld')
wn.links.new(bg.outputs['Background'], out.inputs['Surface'])

sun = bpy.data.lights.new('Sun', 'SUN')
sun.energy = 4.0
sun.angle = math.radians(1.5)
sun_o = env_link(bpy.data.objects.new('Sun', sun))
sun_o.rotation_mode = 'QUATERNION'
sun_o.rotation_quaternion = (-sun_dir).to_track_quat('-Z', 'Y')

VIEWS = {
    'front_three_quarter': ((5.8, 7.4, 1.95), (0, 0.4, 0.85), 60),
    'rear_three_quarter': ((-5.8, -6.6, 2.45), (0, -0.05, 0.9), 60),
    'side': ((9.6, 0.3, 1.15), (0, 0.3, 0.88), 70),
    'high_three_quarter': ((5.0, 5.0, 6.0), (0, 0.3, 0.8), 55),
    'cockpit': ((0, -0.40, 1.36 + LIFT), (0, 3.5, 0.98 + LIFT), 18),
    'engine_detail': ((1.9, 3.4, 2.1), (0, 1.35, 1.2), 50),
    'front_suspension': ((2.6, 3.6, 0.75), (0.55, 1.85, 0.55), 40),
}
for name, (pos, tgt, lens) in VIEWS.items():
    cam = bpy.data.cameras.new('Cam ' + name)
    cam.lens = lens
    cam.clip_start = 0.02
    o = env_link(bpy.data.objects.new('Cam ' + name, cam))
    o.location = pos
    o.rotation_mode = 'QUATERNION'
    o.rotation_quaternion = (Vector(tgt) - Vector(pos)).to_track_quat('-Z', 'Y')
scn.camera = bpy.data.objects['Cam front_three_quarter']

scn.render.engine = 'CYCLES'
scn.cycles.samples = 128
scn.cycles.use_denoising = True
scn.render.resolution_x, scn.render.resolution_y = 1920, 1200
try:
    scn.view_settings.view_transform = 'AgX'
    for look in ('AgX - Medium High Contrast', 'Medium High Contrast'):
        try:
            scn.view_settings.look = look
            break
        except TypeError:
            pass
except TypeError:
    pass

os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'stuntcar.blend'))

# glTF: car only
for o in bpy.context.view_layer.objects:
    o.select_set(o.users_collection[0] is CAR)
bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, 'stuntcar.glb'), export_format='GLB',
                          use_selection=True, export_apply=True)
print('BUILD OK', len(CAR.objects), 'objects')
