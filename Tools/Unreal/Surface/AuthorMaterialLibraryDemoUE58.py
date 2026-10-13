#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟材质函数库独立演示关卡内容作者（非UI蓝图作者）。

职责：在Monolith已创建并打开的独立审核UWorld中，放置真实Blueprint展示架
BP_DBA_MaterialLibraryReviewStand、真实UE母材质实例、中文标牌与中性照明。
禁止修改登录、新手村或正式地图；禁止脚本自行切换当前未保存地图； UI控件只能
由Monolith创建，在此仅记录UI地图须使用独立Widget Blueprint进行人工视觉验收。
不在此阶段编译项目、运行PIE、Cook、GPU性能测试或伪造审核结果。
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
CATALOG=ROOT/"Docs/Implementation/Materials/MaterialCapabilityRegistry_V1.json"
BP="/Game/Development/MaterialLibrary/Blueprints/BP_DBA_MaterialLibraryReviewStand"
BP_CLASS=BP+".BP_DBA_MaterialLibraryReviewStand_C"
BASE="/Game/Development/MaterialLibrary"
GROUPS={
    "Surface": {
        "map":BASE+"/Surface/Maps/L_DBA_MaterialLibrary_Surface_Review",
        "ids":["MAT-01-09","MAT-01-02","MAT-02-02","MAT-02-05","MAT-02-08",
               "MAT-03-07","MAT-04-02","MAT-04-04","MAT-04-14","MAT-05-03",
               "MAT-05-11","MAT-06-09","MAT-07-03","MAT-07-15","MAT-12-10"]
    },
    "VFX": {
        "map":BASE+"/VFX/Maps/L_DBA_MaterialLibrary_VFX_Review",
        "ids":["MAT-09-01","MAT-09-09","MAT-09-14","MAT-09-08","MAT-09-13",
               "MAT-09-16","MAT-09-06","MAT-09-10","MAT-09-15","MAT-09-18"]
    },
    "Project": {
        "map":BASE+"/Project/Maps/L_DBA_MaterialLibrary_Project_Review",
        "ids":["MAT-08-02","MAT-08-04","MAT-08-05","MAT-08-06","MAT-08-07",
               "MAT-08-08","MAT-08-09","MAT-08-10","MAT-08-11","MAT-08-12"]
    },
    "UI": {
        "map":BASE+"/UI/Maps/L_DBA_MaterialLibrary_UI_Review",
        "ids":["MAT-11-01","MAT-11-02","MAT-11-03","MAT-11-04","MAT-11-06",
               "MAT-11-08","MAT-11-09","MAT-11-12","MAT-11-13","MAT-11-14",
               "MAT-11-15","MAT-11-18"]
    }
}


def require_project(unreal):
    """正确主工程是唯一允许创建项目审核场景的容器。"""
    current=str(unreal.Paths.get_project_file_path())
    expected=str(ROOT/"Game/DivineBeastsArena.uproject")
    if os.path.normcase(os.path.realpath(current)) != os.path.normcase(os.path.realpath(expected)):
        raise RuntimeError("拒绝在非锁定DivineBeastsWorkspace工程制作材质库演示地图："+current)
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("审核地图只允许在UE5.8原生编辑器创建与保存")


def manifest(group):
    """场景清单严格来自201项能力索引，未知身份不生成演示模型。"""
    data=json.loads(CATALOG.read_text(encoding="utf8"))
    keys={r["CapabilityId"]:r for r in data["items"]}
    result=[]
    for id in GROUPS[group]["ids"]:
        entry=keys.get(id)
        if not entry:
            raise KeyError("材质演示清单引用未知能力编号："+id)
        result.append(entry)
    return result


def material_path_for(unreal,item):
    """优先真实开发审核Material；不会把尚未创建的贴图字符串冒充资源。"""
    lib=unreal.EditorAssetLibrary
    name=item["NameEN"]
    aliases={
        "RadialEnergyGradient":"VFXRadialEnergy",
        "SnowCoverage":"SnowCoverage"
    }
    n=aliases.get(name,name)
    candidates=[
       BASE+"/Materials/M_DBA_MaterialLibrary_"+n+"_DomainReview",
       BASE+"/Materials/M_DBA_MaterialLibrary_"+n+"_Review"
    ]
    for path in candidates:
        if lib.does_asset_exist(path):
            obj=lib.load_asset(path)
            if isinstance(obj,unreal.MaterialInterface):return obj
    raise FileNotFoundError("当前能力尚无真实独立审核母材质："+item["CapabilityId"]+" "+name)


def map_matches(unreal,expected):
    """脚本不打开关卡，只允许修改用户明确从Monolith打开的指定新地图。"""
    world=unreal.EditorLevelLibrary.get_editor_world()
    if world is None:raise RuntimeError("编辑器当前没有可修改的World")
    package=world.get_path_name().split(".")[0]
    if package!=expected:
        raise RuntimeError("当前打开地图不是材质库独立审核地图：current="+package+" expected="+expected)
    return world


def stage_scene(unreal,group):
    """每张地图只建立一组不影响正式项目内容的亮度和几何背景。"""
    editor=unreal.EditorLevelLibrary
    sphere=unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Sphere")
    plane=unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane")
    if not isinstance(sphere,unreal.StaticMesh) or not isinstance(plane,unreal.StaticMesh):
        raise RuntimeError("UE默认BasicShapes引擎网格未找到，不能以空几何冒充审核")
    light=editor.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,450),
                                        unreal.Rotator(-48,35,0))
    if not isinstance(light,unreal.DirectionalLight):
        raise RuntimeError("审核场景无法创建真实方向光")
    light.set_actor_label("MLReview_KeyLight")
    light.set_folder_path("MaterialLibrary/Lighting")
    sky=editor.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,200))
    if not isinstance(sky,unreal.SkyLight):
        raise RuntimeError("审核场景无法生成SkyLight")
    sky.set_actor_label("MLReview_FillLight")
    sky.set_folder_path("MaterialLibrary/Lighting")
    floor=editor.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-85))
    if not isinstance(floor,unreal.StaticMeshActor):
        raise RuntimeError("审核地面网格创建失败")
    floor.get_editor_property("static_mesh_component").set_static_mesh(plane)
    floor.set_actor_scale3d(unreal.Vector(25,25,1))
    floor.set_actor_label("MLReview_NeutralFloor")
    floor.set_folder_path("MaterialLibrary/Stage")
    return sphere


def spawn_preview(unreal,actor_class,item,index,sphere,material,group):
    """实例化真BP并将现有审核Material赋予预览网格，禁止生成虚假材质实例。"""
    editor=unreal.EditorLevelLibrary
    count=len(GROUPS[group]["ids"])
    columns=5
    row=index//columns
    col=index%columns
    x=float(col*295-(columns-1)*147)
    y=float(row*330)
    at=unreal.Vector(x,y,125)
    actor=editor.spawn_actor_from_class(actor_class,at)
    if actor is None:
        raise RuntimeError("未生成真Blueprint审核展示架："+item["CapabilityId"])
    components=actor.get_components_by_class(unreal.StaticMeshComponent)
    selected=next((c for c in components if c.get_name().startswith("PreviewMesh")),None)
    if not selected:
        raise RuntimeError("BP未定义名为PreviewMesh的真实StaticMeshComponent")
    selected.set_static_mesh(sphere)
    selected.set_material(0,material)
    actor.set_actor_label("MLReview_"+item["CapabilityId"].replace("-","_"))
    actor.set_folder_path("MaterialLibrary/"+group+"/Stands")
    # 世界标牌只用于独立审核，真实UI资源不得由这里的TextRender代替。
    text=editor.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector(x,y,300))
    if not isinstance(text,unreal.TextRenderActor):
        raise RuntimeError("审核材质名称标牌未生成")
    comp=text.get_editor_property("text_render")
    comp.set_text(str(item["NameZH"]+" / "+item["NameEN"]))
    comp.set_world_size(28.0)
    text.set_actor_label("MLReviewLabel_"+item["CapabilityId"].replace("-","_"))
    text.set_folder_path("MaterialLibrary/"+group+"/Labels")


def main():
    """场景和材质资源全量构建完成后才统一编译验证。"""
    parser=argparse.ArgumentParser(description="SSLM材质函数插件独立审核地图布置")
    parser.add_argument("--group",choices=tuple(GROUPS),required=True)
    parser.add_argument("--mode",choices=("inspect","apply","verify"),default=os.getenv("MATERIAL_DEMO_MODE","inspect"))
    opts,_=parser.parse_known_args()
    group=opts.group
    entries=manifest(group)
    print("REVIEW_MAP_TARGET",group,GROUPS[group]["map"],"count",len(entries))
    for item in entries:print("REVIEW_CASE",item["CapabilityId"],item["NameZH"])
    if opts.mode=="inspect":
        print("INSPECT_ONLY：不会修改任何地图或UE资产")
        return
    try:import unreal # type: ignore[import-not-found]
    except ImportError as e:raise RuntimeError("审核地图制作必须在UE5.8编辑器内执行") from e
    require_project(unreal)
    if group=="UI":
        raise RuntimeError("UI必须通过Monolith创建真实WBP并在UI域测试；此脚本不能生成UI替身")
    map_matches(unreal,GROUPS[group]["map"])
    review_materials=[material_path_for(unreal,entry) for entry in entries]
    # 先完整检查审核专属标签：旧版本仅凭一个MLReview_标签就将不完整地图视为已制作。
    # apply遇到任何已有审核对象均拒绝重叠；verify必须核对灯光、地面、每个展示架和中文标牌。
    actors=unreal.EditorLevelLibrary.get_all_level_actors()
    labels=[str(a.get_actor_label()) for a in actors]
    review_labels=[name for name in labels
                   if name.startswith("MLReview_") or name.startswith("MLReviewLabel_")]
    if review_labels:
        if opts.mode=="verify":
            expected_labels={"MLReview_KeyLight","MLReview_FillLight","MLReview_NeutralFloor"}
            for entry in entries:
                suffix=entry["CapabilityId"].replace("-","_")
                expected_labels.add("MLReview_"+suffix)
                expected_labels.add("MLReviewLabel_"+suffix)
            missing=expected_labels-set(review_labels)
            extra=set(review_labels)-expected_labels
            repeated=[name for name in expected_labels if labels.count(name)!=1]
            if missing or extra or repeated:
                raise RuntimeError("独立审核地图内容不完整或重复；不能伪称已完成："
                                   +repr({"missing":sorted(missing),"extra":sorted(extra),"duplicate":sorted(repeated)}))
            for entry,material in zip(entries,review_materials):
                name="MLReview_"+entry["CapabilityId"].replace("-","_")
                actor=next(a for a in actors if str(a.get_actor_label())==name)
                preview=next((c for c in actor.get_components_by_class(unreal.StaticMeshComponent)
                              if c.get_name().startswith("PreviewMesh")),None)
                if preview is None or preview.get_material(0) is None or (
                    preview.get_material(0).get_path_name()!=material.get_path_name()
                ):
                    raise RuntimeError("审核展示架的真实材质绑定不符："+name)
            print("REVIEW_MAP_CONTENT_VERIFIED",GROUPS[group]["map"],"stand_count",len(entries))
            return
        raise RuntimeError("已存在审核内容，禁止在同一地图叠加重复演示物；需先核对半成品")
    if opts.mode=="verify":
        raise RuntimeError("当前审核地图尚无真正展示架，不可称已完成")
    blueprint_class=unreal.load_class(None,BP_CLASS)
    if not blueprint_class or not blueprint_class.is_child_of(unreal.Actor.static_class()):
        raise RuntimeError("缺少真实Monolith Blueprint审核展示架："+BP_CLASS)
    sphere=stage_scene(unreal,group)
    for index,(entry,mat) in enumerate(zip(entries,review_materials)):
        spawn_preview(unreal,blueprint_class,entry,index,sphere,mat,group)
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("UE无法保存当前独立审核地图："+GROUPS[group]["map"])
    print("REVIEW_MAP_ASSETS_SAVED",GROUPS[group]["map"],"stand_count",len(entries))


if __name__=="__main__":
    main()
