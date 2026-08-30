"""Builds M_BreakerBodyOverlay — the one material the reaction layer wears
over a NAMED body's livery.

Unlit translucent: EmissiveColor = the resolved body colour (a Color vector
parameter, the same name every other paint write uses), Opacity = Strength
(BreakerBodyPaint::ResolveOverlayStrength — zero at rest so the livery is
pure, occluding through reactions). The reaction component creates a dynamic
instance of this lazily; a clone without it keeps a paintless body.
Idempotent: the graph is rebuilt from empty each run.

Run (headless, editor closed):
  UnrealEditor-Cmd.exe <project> -run=pythonscript -script="breaker_build_paint_overlay.py"
"""

import unreal

DEST = "/Game/Breaker/Materials"
NAME = "M_BreakerBodyOverlay"

tools = unreal.AssetToolsHelpers.get_asset_tools()
matlib = unreal.MaterialEditingLibrary

material = unreal.load_asset("%s/%s" % (DEST, NAME))
if not material:
    material = tools.create_asset(NAME, DEST, unreal.Material, unreal.MaterialFactoryNew())
if not material:
    raise RuntimeError("[Overlay] could not create %s" % NAME)

material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("two_sided", True)
# The usage flag is the difference between working and silently absent: the
# editor auto-compiles usages on demand, a -game run does not, and an overlay
# on a skeletal mesh without used_with_skeletal_mesh simply never draws —
# photographed as a gold badge on the primitive Lattice beside an unpainted
# mech wearing the very state the badge proves.
material.set_editor_property("used_with_skeletal_mesh", True)

matlib.delete_all_material_expressions(material)
color = matlib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -450, -100)
color.set_editor_property("parameter_name", "Color")
color.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
strength = matlib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -450, 200)
strength.set_editor_property("parameter_name", "Strength")
strength.set_editor_property("default_value", 0.0)
matlib.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
matlib.connect_material_property(strength, "", unreal.MaterialProperty.MP_OPACITY)
matlib.recompile_material(material)

if not unreal.EditorAssetLibrary.save_directory(DEST, only_if_is_dirty=False, recursive=True):
    raise RuntimeError("[Overlay] could not save %s" % DEST)
unreal.log("[Overlay] DONE: %s/%s" % (DEST, NAME))
