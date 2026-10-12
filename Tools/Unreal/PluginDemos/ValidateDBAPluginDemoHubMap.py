# -*- coding: utf-8 -*-
"""UE5.8独立重启回读插件门户地图；不修改现有Asset或标记界面人工已通过。"""
from pathlib import Path
import json
import unreal
ROOT=Path(__file__).resolve().parents[3]
MAP="/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub"
def main():
    assert str(unreal.SystemLibrary.get_engine_version()).startswith("5.8.")
    assets=unreal.EditorAssetLibrary
    if not assets.does_asset_exist(MAP):
        raise RuntimeError("未制作真正的门户UMAP")
    subsystem=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not subsystem.load_level(MAP):
        raise RuntimeError("独立编辑器无法加载开发门户地图")
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world:
        raise RuntimeError("未获得编辑器真实World")
    labels={a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
    must=("DBA_PluginPortal_Floor","DBA_PluginPortal_PlayerStart",
          "DBA_PluginPortal_Sun","DBA_PluginPortal_Camera")
    missing=[x for x in must if x not in labels]
    if missing:
        raise RuntimeError("地图对象缺失："+str(missing))
    if not isinstance(labels["DBA_PluginPortal_PlayerStart"],unreal.PlayerStart):
        raise RuntimeError("门户没有正式PlayerStart")
    record={"status":"loaded_and_verified","map":MAP,"checkedActors":must,
        "actorCount":len(labels),"manualAcceptance":"pending",
        "widget":"requires_Monolith"}
    path=ROOT/"Saved/Validation/PluginDemos/Portal/PortalMapReload.json"
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding="utf-8")
    print("DBA_PORTAL_MAP_INDEPENDENT_RELOAD_OK",len(labels))
if __name__=="__main__":main()
