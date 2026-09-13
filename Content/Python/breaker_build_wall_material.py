"""Build the Fernhall wall grain with world-space horizontal/vertical UVs."""
import unreal
lib=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
path="/Game/Breaker/Materials/M_BreakerWall"
mat=unreal.load_asset(path) or tools.create_asset("M_BreakerWall","/Game/Breaker/Materials",unreal.Material,unreal.MaterialFactoryNew())
texture=unreal.load_asset("/Game/Breaker/Materials/T_BreakerGround")
if not texture: raise RuntimeError("The shipped concrete grain is required")
lib.delete_all_material_expressions(mat)
def node(kind): return lib.create_material_expression(mat,kind,0,0)
def link(a,b,pin=""): lib.connect_material_expressions(a,"",b,pin)
world=node(unreal.MaterialExpressionWorldPosition)
def axis(r,g,b):
 n=node(unreal.MaterialExpressionComponentMask)
 for name,value in (("r",r),("g",g),("b",b),("a",False)): n.set_editor_property(name,value)
 link(world,n); return n
horizontal=node(unreal.MaterialExpressionAdd);link(axis(True,False,False),horizontal,"A");link(axis(False,True,False),horizontal,"B")
uv=node(unreal.MaterialExpressionAppendVector);link(horizontal,uv,"A");link(axis(False,False,True),uv,"B")
scale=node(unreal.MaterialExpressionDivide);scale.set_editor_property("const_b",300.0);link(uv,scale,"A")
sample=node(unreal.MaterialExpressionTextureSample);sample.set_editor_property("texture",texture);link(scale,sample,"UVs")
grain=node(unreal.MaterialExpressionMultiply);grain.set_editor_property("const_b",0.7);link(sample,grain,"A")
base=node(unreal.MaterialExpressionAdd);base.set_editor_property("const_b",0.65);link(grain,base,"A")
color=node(unreal.MaterialExpressionVectorParameter);color.set_editor_property("parameter_name","Color");color.set_editor_property("default_value",unreal.LinearColor(.28,.30,.28,1))
out=node(unreal.MaterialExpressionMultiply);link(base,out,"A");link(color,out,"B")
lib.connect_material_property(out,"",unreal.MaterialProperty.MP_BASE_COLOR)
for prop,value in ((unreal.MaterialProperty.MP_ROUGHNESS,.9),(unreal.MaterialProperty.MP_SPECULAR,.1)):
 n=node(unreal.MaterialExpressionConstant);n.set_editor_property("r",value);lib.connect_material_property(n,"",prop)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(path,only_if_is_dirty=False)
unreal.log("[FernhallPolish] built "+path)

# O2 PLACEHOLDER understory palette. Keep the leaf texture's mask and detail,
# with a restrained green shared by the low route-side foliage.
path="/Game/Breaker/Materials/M_BreakerUnderstory"
mat=unreal.load_asset(path) or tools.create_asset("M_BreakerUnderstory","/Game/Breaker/Materials",unreal.Material,unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(mat)
mat.set_editor_property("two_sided",True)
mat.set_editor_property("used_with_instanced_static_meshes",True)
mat.set_editor_property("used_with_nanite",True)
mat.set_editor_property("blend_mode",unreal.BlendMode.BLEND_MASKED)
leaf=node(unreal.MaterialExpressionTextureSample)
leaf.set_editor_property("texture",unreal.load_asset("/Game/Breaker/EnvironmentKit/Fern_1/Fern_1/Textures/Leaves"))
gray=node(unreal.MaterialExpressionComponentMask)
gray.set_editor_property("r",True);gray.set_editor_property("g",False);gray.set_editor_property("b",False);gray.set_editor_property("a",False);link(leaf,gray)
color=node(unreal.MaterialExpressionVectorParameter);color.set_editor_property("parameter_name","Color")
color.set_editor_property("default_value",unreal.LinearColor(.28,.46,.12,1))
base=node(unreal.MaterialExpressionMultiply);link(gray,base,"A");link(color,base,"B")
lib.connect_material_property(base,"",unreal.MaterialProperty.MP_BASE_COLOR)
lib.connect_material_property(leaf,"A",unreal.MaterialProperty.MP_OPACITY_MASK)
rough=node(unreal.MaterialExpressionConstant);rough.set_editor_property("r",.9)
lib.connect_material_property(rough,"",unreal.MaterialProperty.MP_ROUGHNESS)
lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(path,only_if_is_dirty=False)
unreal.log("[FernhallPolish] built "+path)

# Composed imported floors use Nanite; persist the usage instead of relying on
# a transient game-session recompile that initially displays the fallback.
ground=unreal.load_asset("/Game/Breaker/Materials/M_BreakerGround")
ground.set_editor_property("used_with_nanite",True)
lib.recompile_material(ground)
unreal.EditorAssetLibrary.save_loaded_asset(ground,only_if_is_dirty=False)
