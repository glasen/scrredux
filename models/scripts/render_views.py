"""Stills from the model's cameras: blender -b stuntcar.blend -P render_views.py -- <out_dir> <samples> <width> [view ...]

Uses the Metal GPU when there is one. SHOW_DRIVER=1 shows the fit-check mannequin, HIDE=prefix,... hides objects,
STEER=degrees turns the front wheels."""
import bpy, os, sys

args = sys.argv[sys.argv.index('--') + 1:]
out, samples, width = args[0], int(args[1]), int(args[2])
views = args[3:]

scn = bpy.context.scene
prefs = bpy.context.preferences.addons['cycles'].preferences
try:
    prefs.compute_device_type = 'METAL'
    prefs.get_devices()
    for d in prefs.devices:
        d.use = True
    scn.cycles.device = 'GPU'
except Exception as e:
    print('GPU unavailable, using CPU:', e)
scn.cycles.samples = samples
scn.render.resolution_x = width
scn.render.resolution_y = int(width * 10 / 16)
scn.render.image_settings.file_format = 'PNG'

if os.environ.get('SHOW_DRIVER'):
    bpy.data.collections['Driver fit check'].hide_render = False
for prefix in filter(None, os.environ.get('HIDE', '').split(',')):
    for o in scn.objects:
        if o.name.startswith(prefix):
            o.hide_render = True
if os.environ.get('STEER'):
    root = bpy.data.objects['Stunt Car']
    root['steer'] = float(os.environ['STEER'])
    root.update_tag()
    scn.frame_set(scn.frame_current)
    import math
    piv = bpy.data.objects['Steering pivot R'].evaluated_get(bpy.context.evaluated_depsgraph_get())
    print('STEER applied: right wheel at %.1f deg' % math.degrees(piv.matrix_world.to_euler().z))
cams = [o for o in scn.objects if o.type == 'CAMERA']
os.makedirs(out, exist_ok=True)
for cam in cams:
    name = cam.name[4:]
    if views and name not in views:
        continue
    scn.camera = cam
    scn.render.filepath = os.path.join(out, name + '.png')
    bpy.ops.render.render(write_still=True)
    print('RENDERED', scn.render.filepath)
