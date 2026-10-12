#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群样条真实蓝图与独立审核关卡的UE5.8作者。
必须在本任务独占UE Editor操作或受控Commandlet进程中运行。只创建新Blueprint和新Map；
如发现同名已存在，只验证类型和关键组件，不覆盖其它任务或人工修改。
不使用旧ISM原型，不伪造二进制资源、截图及GPU通过状态。
"""
from pathlib import Path
import json
import os
import unreal

PROJECT="/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockSplineReview"
TEMPLATE="/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockIntegrationReview"
BLUEPRINT="/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Blueprints/BP_DBA_BirdFlockSplineRoute"
SPECIES={
    "Seagull":{"birds":32,"groups":2,"z":1800,"x":-6000,
        "points":[(-1400,-3600,200),(-900,-2400,360),(0,-1000,480),
                  (1600,800,360),(2200,2900,250),(200,4300,380)]},
    "Swallow":{"birds":72,"groups":3,"z":1100,"x":0,
        "points":[(-1600,-2400,180),(-800,-1500,450),(650,-500,320),
                  (1200,900,560),(300,2100,400),(-1200,3000,260)]},
    "Eagle":{"birds":5,"groups":1,"z":3500,"x":6000,
        "points":[(-3100,-3200,450),(-1500,-1300,900),(0,300,1150),
                  (1900,1500,1400),(3100,3500,1000)]},
}

def assert_editor():
    name=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not name.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("不是授权的神兽联盟工作空间")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("需要UE5.8")
    if os.getenv("BIRD_SPLINE_ISOLATED_AUTHOR") != "1":
        raise RuntimeError("只允许显式BIRD_SPLINE_ISOLATED_AUTHOR=1的独占场景作者执行")

def ensure_blueprint(assets):
    cls=getattr(unreal,"DivineBeastsBirdFlockRouteActor",None)
    if cls is None:raise RuntimeError("未编译/加载正式BirdFlockRouteActor C++反射父类")
    if assets.does_asset_exist(BLUEPRINT):
        asset=assets.load_asset(BLUEPRINT)
        if not isinstance(asset,unreal.Blueprint):
            raise RuntimeError("目标BP路径已被非蓝图资产占用")
        result=assets.load_blueprint_class(BLUEPRINT)
        if result is None:raise RuntimeError("现有路径蓝图无法加载")
        print("BIRD_SPLINE_BP_REUSED",BLUEPRINT)
        return result
    folder,name=BLUEPRINT.rsplit("/",1)
    assets.make_directory(folder)
    factory=unreal.BlueprintFactory()
    factory.set_editor_property("parent_class",cls)
    asset=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.Blueprint,factory)
    if asset is None:raise RuntimeError("无法创建真实BirdSplineRoute蓝图")
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    if not assets.save_loaded_asset(asset,only_if_is_dirty=False):
        raise RuntimeError("鸟群路径蓝图保存失败")
    result=assets.load_blueprint_class(BLUEPRINT)
    if result is None:raise RuntimeError("鸟群路径蓝图无法重新回读")
    print("BIRD_SPLINE_BP_SAVED",BLUEPRINT)
    return result

def build_map(klass,assets):
    subsystem=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if assets.does_asset_exist(PROJECT):
        world=assets.load_asset(PROJECT)
        if not isinstance(world,unreal.World):raise RuntimeError("目标路径被非地图资产占用")
        if not subsystem.load_level(PROJECT):raise RuntimeError("无法加载已有路径审核地图")
        print("BIRD_SPLINE_MAP_EXISTS_NO_OVERWRITE",PROJECT)
        return [{"status":"existing_map_needs_readback"}]
    if not assets.does_asset_exist(TEMPLATE):raise RuntimeError("已存在的正式链路审核地图缺失")
    if not subsystem.new_level_from_template(PROJECT,TEMPLATE):
        raise RuntimeError("UE LevelEditor模板地图生成失败")
    # 在新副本中移除老审核地图的三个Actor，避免两套Niagara同时出现。
    for actor in list(actors.get_all_level_actors()):
        if actor.get_actor_label().startswith("BirdIntegration_"):
            if not actors.destroy_actor(actor):
                raise RuntimeError("无法清理新副本中的旧BirdIntegration演示Actor")

    records=[]
    for species,profile in SPECIES.items():
        pos=unreal.Vector(float(profile["x"]),0.0,float(profile["z"]))
        actor=actors.spawn_actor_from_class(klass,pos,unreal.Rotator(0,0,0))
        if actor is None:raise RuntimeError("UE生成项目路径Actor失败："+species)
        actor.set_actor_label("BirdSpline_"+species)
        spline=actor.get_component_by_class(unreal.SplineComponent)
        batch=actor.get_component_by_class(unreal.DivineBeastsBirdFlockBatchComponent)
        if spline is None or batch is None:raise RuntimeError("路径蓝图未包含原生Spline和Batch组件")
        spline.clear_spline_points(False)
        for point in profile["points"]:
            spline.add_spline_point(unreal.Vector(*[float(x) for x in point]),
                                    unreal.SplineCoordinateSpace.LOCAL,False)
        spline.update_spline()
        if spline.get_spline_length()<100.0:raise RuntimeError("鸟群路径长度非法")
        # 一次提交组件字段，避免逐属性PostEditChange重构蓝图实例导致引用失效。
        batch.set_editor_properties({
            "species_id":unreal.Name(species),
            "birds_per_group":profile["birds"],
            "max_active_groups":profile["groups"],
            "max_batches":4,
            "spawn_interval_seconds":6.0,
            "cruise_speed_cm_per_second":250.0 if species!="Eagle" else 380.0,
            "auto_start":False,
        })
        records.append({"species":species,"actor":actor.get_actor_label(),
                        "points":len(profile["points"]),
                        "path_length_cm":spline.get_spline_length(),
                        "birds_per_group":profile["birds"]})
    if not subsystem.save_current_level():
        raise RuntimeError("实际样条审核关卡保存失败")
    if not isinstance(assets.load_asset(PROJECT),unreal.World):
        raise RuntimeError("保存后地图类型回读错误")
    print("BIRD_SPLINE_REVIEW_MAP_SAVED",PROJECT,len(records))
    return records

def main():
    assert_editor()
    assets=unreal.EditorAssetLibrary
    klass=ensure_blueprint(assets)
    records=build_map(klass,assets)
    root=Path(unreal.Paths.get_project_file_path()).resolve().parent.parent
    directory=root/"Saved/Validation/BirdFlockSplineFlight"
    directory.mkdir(parents=True,exist_ok=True)
    (directory/"SplineSceneAuthoring.json").write_text(
        json.dumps({"map":PROJECT,"blueprint":BLUEPRINT,"authors":records,
                    "asset_creation":"UE5.8","actual_niagara_gpu":"not_yet_verified",
                    "human_visual":"pending"},ensure_ascii=False,indent=2)+"\n",
        encoding="utf-8")

if __name__=="__main__":
    main()
