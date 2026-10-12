#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群翅膀动画、材质与LOD的真实UE资产作者（可选高级VAT保留验证门禁）。

--inspect离线验证三种正式Flight网格与材质资产已存在，检验源Manifest并留存SHA。
--author必须在本任务独占的UE5.8中运行，只增量制作项目材质实例而不重写RGB/A顶点色。
新VAT动画必须有真实骨骼输入和AnimToTexture插件，否则明确跳过并采用原WPO正式回退，
严禁生成没有有效Mesh/Material绑定的空“高质量”资产。
"""
import argparse
import hashlib
import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
CONTENT=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/BirdFlock"
SOURCE=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/BirdFlock/RefinedMeshes"
BASE="/DBAPresentationPack_Core/VFX/Environment/BirdFlock"
SPECIES=("Seagull","Swallow","Eagle")
REPORT=ROOT/"Saved/Validation/BirdFlockSplineFlight/WingLodAuthoring.json"


def read_source():
    required=[]
    for species in SPECIES:
        paths=[
            CONTENT/f"Meshes/Flight/SM_DBA_Bird_{species}.uasset",
            CONTENT/f"Materials/MI_DBA_Bird_{species}.uasset",
            SOURCE/f"SM_DBA_Bird_{species}_Flight.fbx"
        ]
        for path in paths:
            if not path.is_file() or path.stat().st_size<1024:
                raise RuntimeError("正式飞行网格、物种材质或源FBX缺失："+str(path))
            required.append({"path":str(path.relative_to(ROOT)).replace("\\","/"),
                             "sha256":hashlib.sha256(path.read_bytes()).hexdigest(),
                             "bytes":path.stat().st_size})
    return required


def author_material_instances():
    import unreal
    project=unreal.Paths.get_project_file_path().replace("\\","/")
    if not project.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("UE编辑器当前项目身份不符")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("需要UE5.8")
    editor=unreal.EditorAssetLibrary
    master=editor.load_asset("/GamePlatformVFX/Environment/BirdFlock/Materials/M_GP_BirdFlock_Master")
    if not isinstance(master,unreal.Material):
        raise RuntimeError("平台通用WPO母材质缺失，停止所有LOD制作")
    names={str(x) for x in unreal.MaterialEditingLibrary.get_scalar_parameter_names(master)}
    # 不根据猜测编辑未知节点：先在真实母材质发现可控振翅参数，才能制作可追溯实例。
    frequency=next((x for x in ("FlapHz","FlapFrequency","WingBeatHz") if x in names),None)
    amplitude=next((x for x in ("WingAmplitude","FlapAmplitude","FlapAmount") if x in names),None)
    if not frequency or not amplitude:
        raise RuntimeError("母材质不存在已发布的WingFrequency/WingAmplitude标量控制。"
                           "需经Monolith或UE材质编辑器补充并验证WPO连线，不能伪造LOD变体。"
                           "发现的标量参数："+repr(sorted(names)))
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    factory=unreal.MaterialInstanceConstantFactoryNew()
    result=[]
    for species in SPECIES:
        base=editor.load_asset(BASE+"/Materials/MI_DBA_Bird_"+species)
        if not isinstance(base,unreal.MaterialInstanceConstant):
            raise RuntimeError("缺少正式项目鸟类材质实例："+species)
        for level,scale in (("Near",1.0),("Far",0.75)):
            name="MI_DBA_Bird_"+species+"_"+level
            path=BASE+"/Materials/"+name
            if editor.does_asset_exist(path):
                existing=editor.load_asset(path)
                if not isinstance(existing,unreal.MaterialInstanceConstant):
                    raise RuntimeError("新材质路径已由非材质实例占用："+path)
                print("BIRD_LOD_MATERIAL_REUSED",path)
                continue
            material=tools.create_asset(name,BASE+"/Materials",unreal.MaterialInstanceConstant,factory)
            if not material:
                raise RuntimeError("不能生成真实材质实例："+path)
            material.set_editor_property("parent",base)
            ok1=unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
                material,amplitude,float(scale))
            ok2=unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
                material,frequency,2.2 if level=="Near" else 1.4)
            if ok1 is False or ok2 is False:
                raise RuntimeError("正式母材质标量绑定被拒绝："+path)
            if not editor.save_loaded_asset(material,only_if_is_dirty=False):
                raise RuntimeError("真实LOD材质保存失败："+path)
            result.append({"species":species,"lod":level,"path":path,
                           "parent":base.get_path_name(),
                           "wpo_existing_rgba_vertex_colors":"preserved"})
    return result


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--inspect",action="store_true")
    ap.add_argument("--author",action="store_true")
    args=ap.parse_args()
    data={"source_assets":read_source(),"status":"source_audited",
          "vat":"not_requested_without_verified_rig_and_animtotexture",
          "production_wpo":"existing_master_preserved",
          "human_review":"pending"}
    if args.inspect:
        print(json.dumps({"status":data["status"],"assets":len(data["source_assets"]),
                          "vat":data["vat"]},ensure_ascii=False))
        return
    if not args.author or os.getenv("BIRD_SPLINE_MATERIAL_AUTHOR")!="1":
        raise RuntimeError("写入真实UE材质必须显式--author和BIRD_SPLINE_MATERIAL_AUTHOR=1")
    data["status"]="authoring"
    try:
        data["material_variants"]=author_material_instances()
        data["status"]="real_materials_saved_pending_visual_acceptance"
    except Exception as exc:
        data["status"]="failed"
        data["error"]=str(exc)
        raise
    finally:
        REPORT.parent.mkdir(parents=True,exist_ok=True)
        REPORT.write_text(json.dumps(data,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")


if __name__=="__main__":
    main()
