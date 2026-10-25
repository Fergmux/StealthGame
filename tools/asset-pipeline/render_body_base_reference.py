"""Render the mannequin-proportion reference image for a Meshy character generation (art bible 8.7 step 1).

CPA story 003 AC-1: the single Meshy test character is generated image-to-3D from a reference rendered from the
accepted Body_Base, so the generation starts at the UEFN mannequin's proportions (risk R8: Meshy text-to-3D
bodies came back with arms about 17 % short).

Run headless (never touches an open Blender session):
    blender -b --factory-startup --python tools/asset-pipeline/render_body_base_reference.py -- <out.png> [size]
Front orthographic view of Body_Base in its reference A-pose, matte light-grey surface on a white background,
the figure filling about 90 % of the frame height, square image (Meshy image models default to 1:1).
"""
import os
import sys

sys.dont_write_bytecode = True
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
BLEND = os.path.join(REPO, "assets", "characters", "body_base", "Body_Base.blend")


def main(out_png, size=1024):
    bpy.ops.wm.open_mainfile(filepath=BLEND)
    scene = bpy.context.scene
    body = bpy.data.objects["Body_Base"]
    for o in scene.objects:
        o.hide_render = o is not body
    dg = bpy.context.evaluated_depsgraph_get()
    pts = [body.matrix_world @ Vector(c) for c in body.evaluated_get(dg).bound_box]
    zmin, zmax = min(p.z for p in pts), max(p.z for p in pts)
    height = zmax - zmin

    cam_data = bpy.data.cameras.new("RefCam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = height / 0.9
    cam = bpy.data.objects.new("RefCam", cam_data)
    scene.collection.objects.link(cam)
    # Body_Base faces -Y (UE mannequin as Blender imports it): the camera stands in front, looking +Y.
    cam.location = (0.0, -6.0, zmin + height / 2.0)
    cam.rotation_euler = (1.5707963, 0.0, 0.0)
    scene.camera = cam

    scene.render.engine = "BLENDER_WORKBENCH"
    shading = scene.display.shading
    shading.light = "STUDIO"
    shading.color_type = "SINGLE"
    shading.single_color = (0.80, 0.78, 0.75)
    shading.show_cavity = False
    shading.show_shadows = False
    shading.show_specular_highlight = False
    scene.world = scene.world or bpy.data.worlds.new("World")
    scene.render.film_transparent = False
    shading.background_type = "VIEWPORT"
    shading.background_color = (1.0, 1.0, 1.0)
    scene.display_settings.display_device = scene.display_settings.display_device
    scene.view_settings.view_transform = "Standard"
    scene.render.resolution_x = size
    scene.render.resolution_y = size
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = out_png
    bpy.ops.render.render(write_still=True)
    print("REFERENCE_RENDERED", {"out": out_png, "height_cm": round(height * 100.0, 2), "size": size})


if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if not argv:
        raise SystemExit("usage: ... -- <out.png> [size]")
    main(os.path.abspath(argv[0]), int(argv[1]) if len(argv) > 1 else 1024)
