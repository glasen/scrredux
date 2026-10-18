"""Suspension demo: blender -b stuntcar.blend -P anim_suspension.py -- <front|rear> <out.mp4> [--steer] [--stills] [--check]

Bakes per-frame keys for both corners of one axle (wishbones, upright + wheel, coilover,
and on the rear the half-shaft and toe link) from the linkage geometry in the model,
then renders with Workbench. The model files are not modified.
"""
import bpy, math, os, sys
from mathutils import Vector, Matrix

ARGS = sys.argv[sys.argv.index('--') + 1:]
AXLE, OUT = ARGS[0], ARGS[1]
STEER = AXLE == 'front' and '--steer' in ARGS
WHEELBASE, KINGPIN_X = 1.80 - (-1.15), 0.76
STEER_AMP = 33.0 if '--full-lock' in ARGS else 30.0
ROOT = bpy.data.objects['Stunt Car']
CFG = {
    'rear': dict(lower='Rear lower wishbone', upper='Rear upper wishbone', ball='Rear ball joint housing',
                 coil='Rear coilover ', group='Rear upright ', wheel='Rear wheel ', up=0.22, dn=0.15, peak=0.23,
                 lim=(-0.155, 0.235), hide=('Rear bumper', 'Tail light', 'Fuel cell', 'Fuel filler', 'Rear limit strap'),
                 cam=((1.45, -5.3, 1.05), (0.05, -1.2, 0.68))),
    'front': dict(lower='Lower wishbone', upper='Upper wishbone', ball='Ball joint housing', coil='Front coilover ',
                  group='Steering pivot ', wheel='Front wheel ', up=0.21, dn=0.145, peak=0.22, lim=(-0.15, 0.225),
                  hide=('Front bumper hoop', 'Bumper centre bar', 'Limit strap'),
                  cam=((1.25, 5.4, 0.85), (0.08, 1.85, 0.60))),
}[AXLE]
FPS, SECONDS = 24, 20
pi = math.pi
RY, RR = -1.15, 0.54
scn = bpy.context.scene
meshes = [o for o in bpy.data.collections['StuntCar'].objects if o.type == 'MESH']


def C(o):
    return sum((o.matrix_world @ Vector(c) for c in o.bound_box), Vector()) / 8


def side_objs(s, prefix, exclude=()):
    return [o for o in meshes if o.name.startswith(prefix) and C(o).x * s > 0
            and not o.name.startswith(exclude)]


def rot(p, axis, ang):
    return Matrix.Translation(p) @ Matrix.Rotation(ang, 4, axis.normalized()) @ Matrix.Translation(-p)


def smooth(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3 - 2 * x)


def target(t, s):
    """Wheel travel in metres (+ = bump) for side s at time t."""
    up, dn = CFG['up'], CFG['dn']
    def asym(v):
        return v * (up if v > 0 else dn)
    if t < 1:
        d = 0.0
    elif t < 5:
        d = asym(math.sin(2 * pi * (t - 1) / 2))
    elif t < 11:
        d = asym(s * math.sin(2 * pi * 1.5 * (t - 5) / 6))
    elif t < 16:
        tt = t - 11
        if tt < 0.25:
            d = CFG['peak'] * smooth(tt / 0.25)
        else:
            d = CFG['peak'] * math.exp(-1.4 * (tt - 0.25)) * math.cos(2 * pi * 0.8 * (tt - 0.25))
    else:
        tt = t - 16
        d = 0.11 * math.sin(pi * tt / 4) * math.sin(2 * pi * 1.4 * tt + s * 1.3)
    return min(max(d, CFG['lim'][0]), CFG['lim'][1])



def steer_input(t):
    """Steering input in degrees: a slow slalom that fades in and out."""
    env = smooth(t / 1.5) * smooth((SECONDS - t) / 1.5)
    return STEER_AMP * env * math.sin(2 * pi * (t - 1.0) / 6.5)


def wheel_angle(steer_deg, s):
    d = math.radians(steer_deg)
    return math.atan2(WHEELBASE * math.sin(d), WHEELBASE * math.cos(d) + s * KINGPIN_X * math.sin(d))


class Corner:
    def __init__(self, s):
        self.s = s
        tag = 'R' if s > 0 else 'L'
        bush = lambda n: sorted([C(o) for o in side_objs(s, n)], key=lambda v: v.y)
        lo, up = CFG['lower'], CFG['upper']
        self.lb, self.ub = bush(lo + ' bush'), bush(up + ' bush')
        self.L0, self.U0 = sorted([C(o) for o in side_objs(s, CFG['ball'])], key=lambda v: v.z)
        self.la, self.ua = self.lb[1] - self.lb[0], self.ub[1] - self.ub[0]
        self.d0 = (self.U0 - self.L0).length
        self.wc0 = bpy.data.objects[CFG['wheel'] + tag].matrix_world.translation.copy()
        ex = (lo + ' bush', lo + ' bracket', up + ' bush', up + ' bracket')
        self.lower = side_objs(s, lo, ex)
        self.upper = side_objs(s, up, ex)
        self.balls = side_objs(s, CFG['ball'])
        self.group = bpy.data.objects[CFG['group'] + tag]
        if self.group.animation_data:  # steering driver; the wheels stay straight here
            self.group.driver_remove('rotation_euler', 2)
        co = CFG['coil']
        self.co_low = [o for n in ('shaft', 'seat low', 'eye low') for o in side_objs(s, co + n)]
        self.co_top = [o for n in ('body', 'seat high', 'eye high', 'reservoir') for o in side_objs(s, co + n)]
        self.spring = side_objs(s, co + 'spring')[0]
        self.eye_lo = C(side_objs(s, co + 'eye low')[0])
        self.eye_hi = C(side_objs(s, co + 'eye high')[0])
        self.rear = AXLE == 'rear'
        self.toe, self.shaft = [], []
        if self.rear:  # front tie rods follow the hub by their Stretch To constraint
            self.toe = side_objs(s, 'Rear toe link')
            ends = sorted([C(o) for o in side_objs(s, 'Rear toe link rod end', ('Rear toe link rod end shank',))],
                          key=lambda v: abs(v.x))
            self.toe_in, self.toe_out = ends[0], ends[-1]
            self.shaft = [o for n in ('Half-shaft', 'Inner CV boot', 'Outer CV boot') for o in side_objs(s, n)]
            self.p_in, self.p_out = Vector((s * 0.21, RY, RR)), Vector((s * 0.75, RY, RR))
        # spring stretches along its own axis via a helper empty
        ax0 = self.eye_hi - self.eye_lo
        self.R0 = Vector((0, 0, 1)).rotation_difference(ax0.normalized()).to_matrix().to_4x4()
        e = bpy.data.objects.new('Spring stretch ' + tag, None)
        scn.collection.objects.link(e)
        e.matrix_world = Matrix.Translation(self.eye_lo) @ self.R0
        bpy.context.view_layer.update()
        mw = self.spring.matrix_world.copy()
        self.spring.parent = e
        self.spring.matrix_parent_inverse = e.matrix_world.inverted()
        self.spring.matrix_basis = mw
        self.stretch = e
        self.base = {o.name: o.matrix_basis.copy() for o in
                     self.lower + self.upper + self.balls + self.co_low + self.co_top + self.toe + self.shaft
                     + [self.group, e]}
        self.build_table()

    def solve(self, a, b_prev):
        L = rot(self.lb[0], self.la, a) @ self.L0
        best = None
        for k in range(-200, 201):
            b = b_prev + math.radians(k * 0.05)
            e = abs((rot(self.ub[0], self.ua, b) @ self.U0 - L).length - self.d0)
            if best is None or e < best[0]:
                best = (e, b)
        return best[1]

    def upright(self, a, b):
        L = rot(self.lb[0], self.la, a) @ self.L0
        U = rot(self.ub[0], self.ua, b) @ self.U0
        q = (self.U0 - self.L0).rotation_difference(U - L)
        return Matrix.Translation(L) @ q.to_matrix().to_4x4() @ Matrix.Translation(-self.L0)

    def build_table(self):
        rows = [(0.0, 0.0, 0.0)]
        for sgn in (1, -1):
            b = 0.0
            for k in range(1, 181):
                a = math.radians(sgn * k * 0.25)
                b = self.solve(a, b)
                dz = (self.upright(a, b) @ self.wc0).z - self.wc0.z
                rows.append((dz, a, b))
        self.table = sorted(rows)

    def angles(self, dz):
        t = self.table
        for (d0, a0, b0), (d1, a1, b1) in zip(t, t[1:]):
            if d0 <= dz <= d1:
                f = (dz - d0) / (d1 - d0) if d1 > d0 else 0
                return a0 + (a1 - a0) * f, b0 + (b1 - b0) * f
        return (t[0][1], t[0][2]) if dz < t[0][0] else (t[-1][1], t[-1][2])

    def pose(self, dz, frame, steer=0.0):
        a, b = self.angles(dz)
        Ml, Mu = rot(self.lb[0], self.la, a), rot(self.ub[0], self.ua, b)
        T = self.upright(a, b)
        low = Ml @ self.eye_lo
        ax0, ax1 = self.eye_hi - self.eye_lo, self.eye_hi - low
        Rc = ax0.rotation_difference(ax1).to_matrix().to_4x4()
        top = self.eye_hi
        G = T @ self.base[self.group.name] @ Matrix.Rotation(wheel_angle(steer, self.s), 4, 'Z')
        key(self.group, G, frame)
        poses = [(self.lower, Ml), (self.upper, Mu), (self.balls, T),
                 (self.co_low, Matrix.Translation(low) @ Rc @ Matrix.Translation(-self.eye_lo)),
                 (self.co_top, Matrix.Translation(top) @ Rc @ Matrix.Translation(-top))]
        if self.rear:
            d0, d1 = self.toe_out - self.toe_in, T @ self.toe_out - self.toe_in
            poses.append((self.toe, rot_to(self.toe_in, d0, d1)))
            poses.append((self.shaft, rot_to(self.p_in, self.p_out - self.p_in, T @ self.p_out - self.p_in)))
        for objs, M in poses:
            for o in objs:
                key(o, M @ self.base[o.name], frame)
        k = ax1.length / ax0.length
        E = Matrix.Translation(low) @ Rc @ self.R0 @ Matrix.Diagonal((1, 1, k, 1))
        key(self.stretch, E, frame)


def rot_to(p, d0, d1):
    R = d0.rotation_difference(d1).to_matrix().to_4x4()
    return Matrix.Translation(p) @ R @ Matrix.Translation(-p)


def key(o, M, frame):
    o.rotation_mode = 'QUATERNION'
    o.matrix_basis = M
    o.keyframe_insert('location', frame=frame)
    o.keyframe_insert('rotation_quaternion', frame=frame)
    o.keyframe_insert('scale', frame=frame)


# clear the view into the corners
hide = (('Limit strap',) if STEER else CFG['hide']) + ('Ground',)
for o in scn.objects:
    if o.name.startswith(hide):
        o.hide_render = True

corners = [Corner(1), Corner(-1)]
n = FPS * SECONDS
for f in range(1, n + 1):
    t = (f - 1) / FPS
    st = steer_input(t) if STEER else 0.0
    if STEER:
        ROOT['steer'] = st
        ROOT.keyframe_insert('["steer"]', frame=f)
    for c in corners:
        c.pose(target(t, c.s), f, st)

cam = bpy.data.objects.new('Cam suspension anim', bpy.data.cameras.new('Cam suspension anim'))
scn.collection.objects.link(cam)
cam.data.lens = 32
pos, tgt = ((2.9, 5.8, 2.5), (0.1, 1.55, 0.72)) if STEER else CFG['cam']
cam.location = pos
cam.rotation_mode = 'QUATERNION'
cam.rotation_quaternion = (Vector(tgt) - cam.location).to_track_quat('-Z', 'Y')
scn.camera = cam

scn.frame_start, scn.frame_end = 1, n
scn.render.fps = FPS
scn.render.engine = 'BLENDER_WORKBENCH'
sh = scn.display.shading
sh.light = 'STUDIO'
sh.color_type = 'MATERIAL'
sh.show_shadows = True
sh.show_cavity = True
scn.render.resolution_x, scn.render.resolution_y = 960, 540
scn.render.resolution_percentage = 100
im = scn.render.image_settings
if hasattr(im, 'media_type'):
    im.media_type = 'VIDEO'
im.file_format = 'FFMPEG'
scn.render.ffmpeg.format = 'MPEG4'
scn.render.ffmpeg.codec = 'H264'
scn.render.ffmpeg.constant_rate_factor = 'MEDIUM'
if '--check' in sys.argv:  # contact test of the steered wheels against everything that does not move with them
    from mathutils.bvhtree import BVHTree
    moving = set()
    for c in corners:
        moving |= set(c.group.children_recursive) | set(c.lower + c.upper + c.balls + c.co_low + c.co_top)
        moving.add(c.spring)
    skip = ('Tie rod', 'Inner tie rod', 'Limit strap', 'Ground')
    static = [o for o in meshes if o not in moving and not o.name.startswith(skip)]
    def bvh(objs, dg):
        V, P = [], []
        for o in objs:
            oe = o.evaluated_get(dg); me = oe.to_mesh(); M = oe.matrix_world; off = len(V)
            V += [M @ v.co for v in me.vertices]; P += [[off + i for i in p.vertices] for p in me.polygons]
            oe.to_mesh_clear()
        return BVHTree.FromPolygons(V, P)
    dg = bpy.context.evaluated_depsgraph_get()
    S = bvh(static, dg)
    hits = []
    for f in range(1, n + 1, 2):
        scn.frame_set(f)
        dg = bpy.context.evaluated_depsgraph_get()
        for c in corners:
            parts = [o for o in c.group.children_recursive if o.type == 'MESH']
            W = bvh(parts, dg)
            if W.overlap(S):
                names = sorted({o.name.split('.')[0] for o in static if W.overlap(bvh([o], dg))})
                t = (f - 1) / FPS
                hits.append((f, c.s, round(target(t, c.s) * 100, 1), round(steer_input(t), 1), names))
                if '--where' in ARGS:
                    for o in static:
                        if o.name.startswith('Header'):
                            pairs = W.overlap(bvh([o], dg))
                            if pairs:
                                oe = o.evaluated_get(dg); me = oe.to_mesh(); M = oe.matrix_world
                                pts = [M @ me.polygons[j].center for _, j in pairs]
                                ctr = sum(pts, Vector()) / len(pts)
                                print('WHERE frame %d %s at (%.3f, %.3f, %.3f)' % (f, o.name, ctr.x, ctr.y, ctr.z))
                                oe.to_mesh_clear()
    for h in hits:
        print('CONTACT frame %d side %+d travel %+.1f cm steer %+.1f -> %s' % h)
    print('CHECK DONE', len(hits), 'contact samples')
elif '--stills' in sys.argv:  # quick check: a few frames as PNG next to OUT
    im.media_type = 'IMAGE' if hasattr(im, 'media_type') else None
    im.file_format = 'PNG'
    for f in (60, 90, 168, 216, 270, 300):
        scn.frame_set(f)
        scn.render.filepath = f'{OUT}_{f:03d}.png'
        bpy.ops.render.render(write_still=True)
    print('STILLS DONE')
else:
    scn.render.filepath = OUT
    bpy.ops.render.render(animation=True)
    print('ANIM DONE', OUT)
