"""Create the approved prototype map shells after compiling the native builder.
Run in an isolated Unreal Editor process. Existing maps are verified, never replaced.
Geometry, services, fixed encounters and rewards are authored in the native builder.
"""
import unreal

maps = ("Lvl_RedBasin", "Lvl_StationZero", "Lvl_PortMeridian", "Lvl_BrokenCoast", "Lvl_Shatterpoint")
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mode = unreal.load_class(None, "/Script/RiorsEdge.BreakerGameMode")
if mode is None:
    raise RuntimeError("Compile RiorsEdge before creating prototype maps")
for name in maps:
    path = "/Game/Breaker/Maps/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        world = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(world, unreal.World):
            raise RuntimeError("Destination package is not a World: " + path)
        unreal.log("Prototype map already exists; left unchanged: " + path)
        continue
    if not levels.new_level(path):
        raise RuntimeError("Failed creating prototype map: " + path)
    world = unreal.EditorAssetLibrary.load_asset(path)
    if world is None:
        raise RuntimeError("New map cannot be loaded: " + path)
    world.get_world_settings().set_editor_property("default_game_mode", mode)
    start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 112), unreal.Rotator())
    if start is None:
        raise RuntimeError("Failed creating destination PlayerStart: " + path)
    start.set_actor_label(name + "_Arrival")
    if not levels.save_current_level():
        raise RuntimeError("Failed saving prototype map: " + path)
    unreal.log("Created playable native destination shell: " + path)
for name in maps:
    if not unreal.EditorAssetLibrary.does_asset_exist("/Game/Breaker/Maps/" + name):
        raise RuntimeError("Incomplete destination creation; native catalogue must remain package-gated")
unreal.log("Prototype destination map packages verified: Red Basin, Station Zero, Port Meridian, Broken Coast and Shatterpoint")
