"""Renders the Airverbs chassis background: assets/rendered/chassis.png

  Blender -b --python render_chassis.py -- [scale] [samples]

Geometry is driven by assets/layout.json (the same file the C++ GUI reads), so every
recessed bay lines up exactly with the controls drawn on top of it."""
import bpy, bmesh, json, math, os, sys
sys.path.append(os.path.dirname(__file__))
from common import *

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
SCALE = float(argv[0]) if len(argv) > 0 else 2.0
SAMPLES = int(argv[1]) if len(argv) > 1 else 128
HERE = os.path.dirname(__file__)
L = json.load(open(os.path.join(HERE, "..", "layout.json")))
W, H = L["size"]
U = 0.01  # blender units per logical pixel

scene = reset_scene()
scene.render.film_transparent = False
scene.world.node_tree.nodes['Background'].inputs[1].default_value = 0.35
scene.render.resolution_x = int(W * SCALE)
scene.render.resolution_y = int(H * SCALE)
if scene.render.engine == 'CYCLES':
    scene.cycles.samples = SAMPLES

def to_b(x, y):
    return (x * U - W * U / 2, H * U / 2 - y * U)

def box(name, rect, z0, z1, mat=None, bevel=0.0, segs=3):
    x, y, w, h = rect
    cx, cy = to_b(x + w / 2, y + h / 2)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(cx, cy, (z0 + z1) / 2))
    o = bpy.context.active_object; o.name = name
    o.scale = (w * U, h * U, z1 - z0)
    bpy.ops.object.transform_apply(scale=True)
    if bevel > 0:
        m = o.modifiers.new("bevel", 'BEVEL'); m.width = bevel; m.segments = segs; m.limit_method = 'ANGLE'
    if mat: assign(o, mat)
    return o

def inset(rect, d):
    x, y, w, h = rect
    return [x + d, y + d, w - 2 * d, h - 2 * d]

# ---------------- materials
chassis_m = brushed("Chassis", hex_rgb("17191e"), metallic=0.55, roughness=0.48, scale=(1, 260, 1))
topbar_m  = brushed("TopBar",  hex_rgb("101216"), metallic=0.6, roughness=0.4, scale=(1, 300, 1))
floor_m   = principled("BayFloor", hex_rgb("08090b"), metallic=0.0, roughness=0.85)
glass_m   = principled("Glass", hex_rgb("010102"), metallic=0.0, roughness=0.9)
chrome_m  = brushed("Chrome", hex_rgb("80858f"), metallic=1.0, roughness=0.22, scale=(1, 120, 1))
screw_m   = principled("Screw", hex_rgb("8a8f98"), metallic=1.0, roughness=0.3)
logo_m    = principled("Logo", hex_rgb("e9edf5"), metallic=1.0, roughness=0.12)
sub_m     = principled("SubLogo", hex_rgb("6c7380"), metallic=0.4, roughness=0.5)

# floor-plate noise for a powder-coat feel
nt = floor_m.node_tree; b = nt.nodes.get("Principled BSDF")
nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 900.0
bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.08
nt.links.new(nz.outputs["Fac"], bump.inputs["Height"]); nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])

# ---------------- base plate with pockets
base = box("Base", [-20, -20, W + 40, H + 40], -0.30, 0.0, chassis_m)
cutters = []
bays = [(k, v) for k, v in L["stars"].items()] + [(k, L[k]) for k in ("topology", "motion", "macros", "output")]
for name, rect in bays:
    cutters.append(box("cut_" + name, rect, -0.06, 0.2, bevel=0.05))
cutters.append(box("cut_view", inset(L["view"], -2), -0.16, 0.2, bevel=0.06))
for c in cutters:
    m = base.modifiers.new("bool_" + c.name, 'BOOLEAN'); m.operation = 'DIFFERENCE'; m.object = c; m.solver = 'EXACT'
    c.hide_render = True; c.hide_viewport = True
bev = base.modifiers.new("edgebevel", 'BEVEL'); bev.width = 0.035; bev.segments = 4; bev.limit_method = 'ANGLE'; bev.angle_limit = math.radians(40)

# floors of the bays
for name, rect in bays:
    box("floor_" + name, inset(rect, 1), -0.061, -0.058, floor_m)

# ---------------- star LED strips
for key, rect in L["stars"].items():
    x, y, w, h = rect
    col = hex_rgb(L["colors"][key])
    led = principled("LED_" + key, col, emission=col, strength=9.0)
    box("led_" + key, [x + 14, y + 6, w - 28, 2.2], -0.058, -0.050, led, bevel=0.004)
    glow = principled("LEDglow_" + key, col, emission=col, strength=0.6)
    box("ledglow_" + key, [x + 10, y + 4, w - 20, 7], -0.0595, -0.0585, glow)

# ---------------- top bar
box("TopBar", [0, -10, W, 66], 0.0, 0.045, topbar_m, bevel=0.03)
box("TopGroove", [0, 56, W, 1.5], 0.0, 0.002, principled("Groove", hex_rgb("050506"), roughness=0.9))

# ---------------- observatory window
vx, vy, vw, vh = L["view"]
box("ViewGlass", inset(L["view"], -1), -0.16, -0.155, glass_m)
# bezel ring = outer box minus inner box
outer = box("Bezel", inset(L["view"], -6), 0.0, 0.04, chrome_m)
inner = box("BezelCut", inset(L["view"], 1), -0.5, 0.5, bevel=0.03)
m = outer.modifiers.new("ring", 'BOOLEAN'); m.operation = 'DIFFERENCE'; m.object = inner; m.solver = 'EXACT'
inner.hide_render = True; inner.hide_viewport = True
bv = outer.modifiers.new("bevel", 'BEVEL'); bv.width = 0.02; bv.segments = 4; bv.limit_method = 'ANGLE'
rim = principled("ViewRim", hex_rgb("9fe7ff"), emission=hex_rgb("9fe7ff"), strength=3.0)
for (rx, ry, rw, rh) in ([vx - 1, vy - 1, vw + 2, 1.2], [vx - 1, vy + vh - 0.2, vw + 2, 1.2],
                         [vx - 1, vy - 1, 1.2, vh + 2], [vx + vw - 0.2, vy - 1, 1.2, vh + 2]):
    box("rim", [rx, ry, rw, rh], -0.02, 0.0, rim)

def screw(px, py, r=4.2):
    cx, cy = to_b(px, py)
    bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=r * U, depth=0.02, location=(cx, cy, 0.055))
    s = bpy.context.active_object; assign(s, screw_m)
    mm = s.modifiers.new("bevel", 'BEVEL'); mm.width = 0.006; mm.segments = 3
    bpy.ops.object.shade_smooth()
    # cross slot
    for ang in (45, 135):
        bpy.ops.mesh.primitive_cube_add(size=1, location=(cx, cy, 0.066))
        c = bpy.context.active_object
        c.scale = (r * 1.5 * U, 0.9 * U, 0.02); c.rotation_euler = (0, 0, math.radians(ang))
        bm = s.modifiers.new("slot", 'BOOLEAN'); bm.operation = 'DIFFERENCE'; bm.object = c
        c.hide_render = True; c.hide_viewport = True

for sx, sy in ((vx - 4, vy - 4), (vx + vw + 4, vy - 4), (vx - 4, vy + vh + 4), (vx + vw + 4, vy + vh + 4)):
    screw(sx, sy, 3.6)

# ---------------- logo
lx, ly, lw, lh = L["logo"]
bpy.ops.object.text_add(location=(*to_b(lx, ly + 27), 0.045))
t = bpy.context.active_object
t.data.body = "AIRVERBS"
t.data.size = 30 * U
t.data.space_character = 1.12
t.data.extrude = 0.012
t.data.bevel_depth = 0.004
t.data.bevel_resolution = 3
assign(t, logo_m)
bpy.ops.object.text_add(location=(*to_b(lx + 2, ly + 41), 0.046))
t2 = bpy.context.active_object
t2.data.body = "BY MTYAS  ·  CONSTELLATION REVERB  ·  AIRWINDOWS INSIDE"
t2.data.size = 7.5 * U
t2.data.space_character = 1.3
t2.data.extrude = 0.001
assign(t2, sub_m)

# ---------------- lights + camera
area_light("Key", (-10.0, 7.0, 3.0), (68, 0, -125), 6.0, 2200)
area_light("Fill", (9.0, -6.0, 3.0), (65, 0, 55), 8.0, 500, (0.7, 0.8, 1.0))
area_light("Top", (0.0, 0.0, 9.0), (0, 0, 0), 14.0, 120)

cam_d = bpy.data.cameras.new("Cam"); cam_d.type = 'ORTHO'; cam_d.ortho_scale = W * U
cam = bpy.data.objects.new("Cam", cam_d); scene.collection.objects.link(cam)
cam.location = (0, 0, 20); scene.camera = cam

scene.render.filepath = os.path.abspath(os.path.join(HERE, "..", "rendered", "chassis.png"))
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGB'
bpy.ops.render.render(write_still=True)
print("wrote", scene.render.filepath)
