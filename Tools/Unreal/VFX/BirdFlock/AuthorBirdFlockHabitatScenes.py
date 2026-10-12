#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟鸟群生态样条子场景真实UE编辑器作者。

设计师未确认真实Village/OpenWorld锚点前，只创建DBAPresentationPack_Core（ClientOnly内容包）
下的独立生态审核场景，不将Niagara/纯表现鸟群写进DedicatedServer共享村庄主地图。
--inspect仅查看预设和真实世界地图身份证据；--author需受控独立UE5.8编辑器。
"""
import argparse
import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
PRESETS=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/BirdFlock/BirdFlockHabitatPresets.json"
BLUEPRINT="/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Blueprints/BP_DBA_BirdFlockSplineRoute"
TEMPLATE="/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockReview"
OUTPUT=ROOT/"Saved/Validation/BirdFlockSplineFlight/HabitatScenes.json"


def inspect():
    data=json.loads(PRESETS.read_text(encoding="utf-8"))
    if data.get("schema_version")!=1 or not data.get("requires_client_only_level"):
        raise RuntimeError("鸟群生态配置不符合单一客户端安全边界")
    for item in data["habitats"]:
        points=item["route_points_local_cm"]
        if len(points)<2 or not all(len(p)==3 and all(abs(float(v))<100000 for v in p) for p in points):
            raise RuntimeError("路线控制点数量/数值范围无效："+item["id"])
        if item["max_active_groups"]<1 or item["max_active_groups"]>16 or item["birds_per_group"]<0 or item["birds_per_group"]>512:
            raise RuntimeError("群组预算越界："+item["id"])
        if item["world_asset"] and item["world_asset_present_in_git"]:
            expected=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap"
            if not expected.is_file():raise RuntimeError("Village世界正式地图未找到，拒绝假设其存在")
        if "OpenWorld" in item["world_id"] and item.get("world_asset") is not None:
            raise RuntimeError("未交付OpenWorld地图不可被宣称已连接真实世界")
    return data


def author():
    import unreal
    project=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not project.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("不是授权的DivineBeastsArena主工程")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("必须使用UE5.8")
    if os.getenv("BIRD_SPLINE_ISOLATED_AUTHOR")!="1":
        raise RuntimeError("需要独立资产作者模式 BIRD_SPLINE_ISOLATED_AUTHOR=1")
    assets=unreal.EditorAssetLibrary
    editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    klass=assets.load_blueprint_class(BLUEPRINT)
    if klass is None or not assets.does_asset_exist(TEMPLATE):
        raise RuntimeError("正式BirdSplineRoute蓝图或鸟群审核模板地图不存在")
    result=[]
    for profile in inspect()["habitats"]:
        path=profile["client_only_review_level"]
        if assets.does_asset_exist(path):
            obj=assets.load_asset(path)
            if not isinstance(obj,unreal.World):raise RuntimeError("目标路径被其它资源占用："+path)
            result.append({"path":path,"status":"exists_not_overwritten"})
            continue
        if not editor.new_level_from_template(path,TEMPLATE):
            raise RuntimeError("UE独立客户端生态地图生成失败："+path)
        # 模板包含直接Niagara BirdReview原型；新客户端生态子场景只走项目正式组件。
        for actor in list(actors.get_all_level_actors()):
            if actor.get_actor_label().startswith("BirdReview_"):
                if not actors.destroy_actor(actor):
                    raise RuntimeError("不能从当前独立地图移除直接Niagara演示Actor")
        flock=actors.spawn_actor_from_class(klass,unreal.Vector(0,0,0),unreal.Rotator(0,0,0))
        if flock is None:raise RuntimeError("正式项目BirdSplineRoute蓝图无法生成："+path)
        flock.set_actor_label("BirdHabitat_"+profile["id"].replace(".","_"))
        spline=flock.get_component_by_class(unreal.SplineComponent)
        group=flock.get_component_by_class(unreal.DivineBeastsBirdFlockBatchComponent)
        if spline is None or group is None:
            raise RuntimeError("新场景缺正式Spline/Group组件："+path)
        spline.clear_spline_points(False)
        for point in profile["route_points_local_cm"]:
            spline.add_spline_point(unreal.Vector(float(point[0]),float(point[1]),float(point[2])),
                                    unreal.SplineCoordinateSpace.LOCAL,False)
        spline.update_spline()
        group.set_editor_properties({
            "species_id":unreal.Name(profile["species"]),
            "birds_per_group":int(profile["birds_per_group"]),
            "max_active_groups":int(profile["max_active_groups"]),
            "max_batches":int(profile["max_batches"]),
            "spawn_interval_seconds":float(profile["interval_seconds"]),
            "auto_start":False
        })
        if not editor.save_current_level():
            raise RuntimeError("独立客户端鸟群生态地图保存失败："+path)
        if not isinstance(assets.load_asset(path),unreal.World):
            raise RuntimeError("新地图保存后UE AssetRegistry回读失败："+path)
        result.append({"path":path,"status":"real_client_only_review_map_saved",
                       "species":profile["species"],"route_length_cm":spline.get_spline_length(),
                       "in_production_world":False,"human_anchor_approval":"pending"})
    return result


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--inspect",action="store_true")
    parser.add_argument("--author",action="store_true")
    args=parser.parse_args()
    specs=inspect()
    if args.inspect:
        print(json.dumps({"status":"source_specs_only","habitats":len(specs["habitats"]),
              "village_map_in_repo":True,"openworld_map_in_repo":False,
              "approval":"pending"},ensure_ascii=False))
        return
    if not args.author:
        raise RuntimeError("必须明确--author并连接真实UE编辑器")
    report={"status":"pending","review_maps":[],"production_placement":"not_done"}
    try:
        report["review_maps"]=author()
        report["status"]="client_review_maps_saved_pending_visual_and_production_review"
    except Exception as error:
        report["status"]="failed"
        report["error"]=str(error)
        raise
    finally:
        OUTPUT.parent.mkdir(exist_ok=True,parents=True)
        OUTPUT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")


if __name__=="__main__":
    main()
