# -*- coding: utf-8 -*-
"""SSLM新手村替换方案预检：真实UE资产的类、材质、Landscape层、玩家出生点。只读。"""
import unreal
import os, json, traceback

TARGET = "/DBAWorldPack_Village/Maps/L_Village_Start"
OLD_LAKE = "/DBAWorldPack_Village/Water/Blueprints/BP_DBA_VillageLake"
NEW_MAT = "/DBAWorldPack_Village/Water/Materials/MI_DBA_Village_Lake"
UNDERWATER = "/Water/Materials/PostProcessing/M_UnderWater_PostProcess_Volume"
DEST = os.path.join(unreal.Paths.project_saved_dir(), "Validation", "Water", "VillageMigrationPreflight.json")


def main():
    editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not editor.load_level(TARGET):
        raise RuntimeError("地图加载失败")
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    report={"target": TARGET, "actors":[], "assets":{}, "requirements":{}}
    for a in actors:
        t=a.get_actor_location()
        record={"label":str(a.get_actor_label()),"class":str(a.get_class().get_name()),"locationCm":[float(t.x),float(t.y),float(t.z)]}
        if isinstance(a,unreal.Landscape):
            try:
                record["canHaveLayersContent"]=a.get_editor_property("can_have_layers_content")
            except Exception as e:
                record["canHaveLayersContentError"]=str(e)
            try:
                record["landscapeMaterial"]=str(a.get_editor_property("landscape_material"))
            except Exception as e:
                record["landscapeMaterialError"]=str(e)
        report["actors"].append(record)
    for label,path in (("oldLakeBlueprint",OLD_LAKE),("villageLakeMaterial",NEW_MAT),("nativeUnderwaterPostProcess",UNDERWATER)):
        ob=unreal.load_asset(path)
        result={"exists":bool(ob),"path":path}
        if ob:
            result["class"]=str(ob.get_class().get_name())
            try:
                if label=="oldLakeBlueprint":
                    generated=ob.get_editor_property("generated_class")
                    result["generatedClass"]=str(generated.get_path_name()) if generated else None
                    if generated:
                        cdo=unreal.get_default_object(generated)
                        result["parentActorClass"]=str(cdo.get_class().get_name())
                        comps=cdo.get_components_by_class(unreal.ActorComponent)
                        result["components"]=[str(x.get_class().get_name()) for x in comps]
            except Exception as e:
                result["detailsError"]=str(e)
        report["assets"][label]=result
    report["requirements"]["PythonPluginEnabled"]=hasattr(unreal,"WaterBodyLake")
    report["requirements"]["WaterZoneClass"]=hasattr(unreal,"WaterZone")
    report["requirements"]["UnderwaterSettingsEditable"]=hasattr(unreal,"UnderwaterPostProcessSettings")
    os.makedirs(os.path.dirname(DEST),exist_ok=True)
    with open(DEST,"w",encoding="utf-8") as file:
        json.dump(report,file,ensure_ascii=False,indent=2,default=str)
    unreal.log("SSLM_VILLAGE_MIGRATION_PREFLIGHT_OK " + DEST)


try:
    main()
except Exception:
    unreal.log_error("SSLM_VILLAGE_MIGRATION_PREFLIGHT_FAILED " + traceback.format_exc())
    raise
