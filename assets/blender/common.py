"""Shared helpers for the Airverbs Blender asset scripts (Blender 4.x / 5.x, run headless)."""
import bpy, math

def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    try:
        scene.render.engine = 'CYCLES'
        prefs = bpy.context.preferences.addons['cycles'].preferences
        for dev_type in ('METAL', 'OPTIX', 'CUDA', 'HIP', 'ONEAPI'):
            try:
                prefs.compute_device_type = dev_type
                prefs.get_devices()
                if any(d.type == dev_type for d in prefs.devices):
                    for d in prefs.devices: d.use = True
                    scene.cycles.device = 'GPU'
                    break
            except TypeError:
                continue
        scene.cycles.use_denoising = True
    except Exception as e:
        print("Cycles unavailable, using EEVEE:", e)
        scene.render.engine = 'BLENDER_EEVEE'
    scene.render.film_transparent = True
    scene.view_settings.view_transform = 'AgX' if 'AgX' in [v.identifier for v in scene.view_settings.bl_rna.properties['view_transform'].enum_items] else 'Filmic'
    scene.view_settings.look = 'None'
    world = bpy.data.worlds.new("World")
    scene.world = world
    world.use_nodes = True
    nt = world.node_tree
    bg = nt.nodes.get("Background")
    bg.inputs[1].default_value = 1.0
    # studio gradient: bright softbox above, dark floor -> metals get a readable reflection
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.45
    ramp.color_ramp.elements[0].color = (0.012, 0.013, 0.016, 1)
    ramp.color_ramp.elements[1].position = 1.0
    ramp.color_ramp.elements[1].color = (0.55, 0.58, 0.64, 1)
    mr = nt.nodes.new("ShaderNodeMapRange")
    mr.inputs["From Min"].default_value = -1.0
    nt.links.new(tc.outputs["Generated"], sep.inputs[0])
    nt.links.new(sep.outputs["Z"], mr.inputs["Value"])
    nt.links.new(mr.outputs["Result"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], bg.inputs["Color"])
    return scene

def hex_rgb(h, a=1.0):
    h = h.lstrip('#')
    srgb = [int(h[i:i+2], 16) / 255.0 for i in (0, 2, 4)]
    lin = [c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in srgb]
    return (*lin, a)

def principled(name, color, metallic=0.0, roughness=0.5, aniso=0.0, emission=None, strength=0.0, coat=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = color
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = roughness
    if "Anisotropic" in b.inputs: b.inputs["Anisotropic"].default_value = aniso
    if coat and "Coat Weight" in b.inputs: b.inputs["Coat Weight"].default_value = coat
    if emission is not None:
        key = "Emission Color" if "Emission Color" in b.inputs else "Emission"
        b.inputs[key].default_value = emission
        b.inputs["Emission Strength"].default_value = strength
    return m

def brushed(name, color, metallic=1.0, roughness=0.32, scale=(1.0, 220.0, 1.0), radial=False):
    """Brushed metal: roughness modulated by stretched noise (linear or radial)."""
    m = principled(name, color, metallic, roughness)
    nt = m.node_tree
    b = nt.nodes.get("Principled BSDF")
    tc = nt.nodes.new("ShaderNodeTexCoord")
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 3.0
    noise.inputs["Detail"].default_value = 12.0
    if radial:
        # concentric brushing: noise of the distance from the centre
        sep = nt.nodes.new("ShaderNodeVectorMath"); sep.operation = 'LENGTH'
        mul = nt.nodes.new("ShaderNodeMath"); mul.operation = 'MULTIPLY'; mul.inputs[1].default_value = 260.0
        nt.links.new(tc.outputs["Object"], sep.inputs[0])
        nt.links.new(sep.outputs["Value"], mul.inputs[0])
        nt.links.new(mul.outputs[0], noise.inputs["Vector"] if "Vector" in noise.inputs else noise.inputs[0])
    else:
        mapping = nt.nodes.new("ShaderNodeMapping")
        mapping.inputs["Scale"].default_value = scale
        nt.links.new(tc.outputs["Object"], mapping.inputs["Vector"])
        nt.links.new(mapping.outputs["Vector"], noise.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeMapRange")
    ramp.inputs["To Min"].default_value = roughness * 0.65
    ramp.inputs["To Max"].default_value = roughness * 1.35
    nt.links.new(noise.outputs["Fac"], ramp.inputs["Value"])
    nt.links.new(ramp.outputs["Result"], b.inputs["Roughness"])
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.04
    nt.links.new(noise.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m

def area_light(name, loc, rot, size, energy, color=(1, 1, 1)):
    d = bpy.data.lights.new(name, 'AREA')
    d.size = size; d.energy = energy; d.color = color
    o = bpy.data.objects.new(name, d)
    bpy.context.scene.collection.objects.link(o)
    o.location = loc; o.rotation_euler = [math.radians(a) for a in rot]
    return o

def assign(obj, mat):
    obj.data.materials.clear()
    obj.data.materials.append(mat)
