"""Renders the Airverbs knob as a vertical filmstrip: assets/rendered/knob.png

  Blender -b --python render_knob.py -- [frames] [px]

A knurled, machined aluminium knob with a concentric-brushed cap and an engraved
indicator, lit like the chassis so it sits in the panel. Frame 0 = minimum (-135 deg)."""
import bpy, bmesh, math, os, sys
sys.path.append(os.path.dirname(__file__))
from common import *

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
FRAMES = int(argv[0]) if len(argv) > 0 else 64
PX = int(argv[1]) if len(argv) > 1 else 144
OUT = os.path.join(os.path.dirname(__file__), "..", "rendered")
TMP = os.path.join(bpy.app.tempdir, "airverbs_knob")
os.makedirs(TMP, exist_ok=True)

scene = reset_scene()
scene.render.resolution_x = scene.render.resolution_y = PX
scene.render.resolution_percentage = 100
if scene.render.engine == 'CYCLES':
    scene.cycles.samples = 96

def gear_prism(name, r_outer, r_inner, teeth, z0, z1, bevel=0.0):
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    pts = []
    n = teeth * 2
    for i in range(n):
        a = 2 * math.pi * i / n
        r = r_outer if i % 2 == 0 else r_inner
        pts.append((r * math.cos(a), r * math.sin(a)))
    bot = [bm.verts.new((x, y, z0)) for x, y in pts]
    top = [bm.verts.new((x, y, z1)) for x, y in pts]
    bm.faces.new(bot[::-1]); bm.faces.new(top)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((bot[i], bot[j], top[j], top[i]))
    bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me)
    scene.collection.objects.link(o)
    if bevel > 0:
        mod = o.modifiers.new("bevel", 'BEVEL'); mod.width = bevel; mod.segments = 3; mod.limit_method = 'ANGLE'
    for p in o.data.polygons: p.use_smooth = False
    return o

def cylinder(name, r, z0, z1, verts=128, bevel=0.0, segs=4):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=z1 - z0, location=(0, 0, (z0 + z1) / 2))
    o = bpy.context.active_object; o.name = name
    if bevel > 0:
        mod = o.modifiers.new("bevel", 'BEVEL'); mod.width = bevel; mod.segments = segs; mod.limit_method = 'ANGLE'
    bpy.ops.object.shade_smooth()
    return o

alu_dark = brushed("AluDark", hex_rgb("2a2d33"), metallic=1.0, roughness=0.38, scale=(1, 1, 60))
alu_cap  = brushed("AluCap",  hex_rgb("5d626c"), metallic=1.0, roughness=0.26, radial=True)
skirt_m  = principled("Skirt", hex_rgb("15161a"), metallic=0.6, roughness=0.45)
mark_m   = principled("Mark",  hex_rgb("ffffff"), metallic=0.0, roughness=0.3, emission=hex_rgb("ffffff"), strength=2.5)

pivot = bpy.data.objects.new("Pivot", None); scene.collection.objects.link(pivot)

skirt = cylinder("Skirt", 1.0, 0.0, 0.10, bevel=0.03); assign(skirt, skirt_m)
body = gear_prism("Body", 0.80, 0.765, 60, 0.08, 0.58, bevel=0.012); assign(body, alu_dark); body.parent = pivot
chamfer = cylinder("Chamfer", 0.76, 0.56, 0.64, bevel=0.06, segs=6); assign(chamfer, alu_dark); chamfer.parent = pivot
cap = cylinder("Cap", 0.66, 0.60, 0.665, bevel=0.015); assign(cap, alu_cap); cap.parent = pivot

bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0.42, 0.665))
mark = bpy.context.active_object; mark.name = "Indicator"
mark.scale = (0.045, 0.40, 0.02)
mod = mark.modifiers.new("bevel", 'BEVEL'); mod.width = 0.01; mod.segments = 2
assign(mark, mark_m); mark.parent = pivot

# contact shadow onto the panel
bpy.ops.mesh.primitive_plane_add(size=6, location=(0, 0, 0))
catcher = bpy.context.active_object
catcher.is_shadow_catcher = True

area_light("Key", (-3.0, 3.0, 5.0), (35, 0, -135), 3.5, 900)
area_light("Fill", (4.0, -1.0, 3.0), (60, 0, 75), 4.0, 180, (0.75, 0.85, 1.0))
area_light("Rim", (0.0, -4.0, 1.2), (80, 0, 0), 2.0, 260, (1.0, 0.85, 0.7))

cam_d = bpy.data.cameras.new("Cam"); cam_d.type = 'ORTHO'; cam_d.ortho_scale = 2.25
cam = bpy.data.objects.new("Cam", cam_d); scene.collection.objects.link(cam)
cam.location = (0, 0, 10); scene.camera = cam

frames = []
for f in range(FRAMES):
    v = f / (FRAMES - 1)
    pivot.rotation_euler = (0, 0, -math.radians(-135 + 270 * v))
    path = os.path.join(TMP, f"knob_{f:03d}.png")
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    frames.append(path)
    print("frame", f)

# stitch into a vertical strip (frame 0 at the top)
import numpy as np
strip = np.zeros((PX * FRAMES, PX, 4), dtype=np.float32)
for i, p in enumerate(frames):
    img = bpy.data.images.load(p)
    px = np.array(img.pixels[:], dtype=np.float32).reshape(PX, PX, 4)
    # Blender pixel rows are bottom-up; strip rows are bottom-up too, so frame i sits at row block FRAMES-1-i
    r0 = (FRAMES - 1 - i) * PX
    strip[r0:r0 + PX] = px
    bpy.data.images.remove(img)
out = bpy.data.images.new("strip", PX, PX * FRAMES, alpha=True)
out.alpha_mode = 'STRAIGHT'
out.pixels = strip.ravel()
out.filepath_raw = os.path.abspath(os.path.join(OUT, "knob.png"))
out.file_format = 'PNG'
out.save()
print("wrote", out.filepath_raw)
