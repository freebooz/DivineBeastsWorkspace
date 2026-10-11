#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟CliffRock01低频三向投射变体：真正UE Material/MI制作。

复制已验证的WorldAligned材质，给三个WorldAligned函数共用同一个低频
世界坐标扭曲输入，避免颜色/法线/ORM错位，不添加第二组完整PBR纹理采样。
另建审核用UV暗度补偿MI，不改变原本林地Rock01、正式Village或已验证Cliff材质。
CLIFFROCK_MACRO_MODE=inspect/apply/verify；新资源仅存项目层与Development QA目录。
此变体可减轻视觉重复，但不能以材质Shader通过代替真正GPU/人工审核。
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
SOURCE=(ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/"
        "DBAWorldPack_Village/SourceArt/Terrain/PBR/CliffRock01")
SIGNATURE=SOURCE/"CliffRock01_ColorGradeManifest.json"
FOLDER="/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials"
OLD=FOLDER+"/M_DBA_CliffRock01_WorldAligned"
NEW=FOLDER+"/M_DBA_CliffRock01_WorldAlignedMacro"
NEW_MI=FOLDER+"/MI_DBA_CliffRock01_WorldAlignedMacro"
UV=FOLDER+"/M_DBA_CliffRock01_PBRReview"
QA_ROOT="/Game/Development/Terrain/Materials"
QA=QA_ROOT+"/MI_DBA_CliffRock01_UVBalancedReview"
TEX="/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Textures"
MAPS={"BaseColor":TEX+"/T_DBA_CliffRock01_BaseColor_BlueGray",
      "NormalDX":TEX+"/T_DBA_CliffRock01_NormalDX",
      "ORM":TEX+"/T_DBA_CliffRock01_ORM"}
FREQ=(.00039,.00027,.00058)
AMP=(98.,-75.,64.)
MODE=os.environ.get("CLIFFROCK_MACRO_MODE","inspect").strip().lower()


def run()->None:
    spec=json.loads(SIGNATURE.read_text(encoding="utf-8"))
    src=(ROOT/spec["gradedSourcePath"]).resolve()
    if (spec.get("sourceId")!="marble_cliff_05" or
        spec.get("productionAssetApproved") is not False or
        not src.is_relative_to(SOURCE.resolve()) or
        hashlib.sha256(src.read_bytes()).hexdigest()!=spec["gradedBaseColorSHA256"]):
        raise RuntimeError("CliffRock来源/人工签收状态不正确")
    if MODE not in ("inspect","apply","verify"):
        raise ValueError("仅支持inspect/apply/verify")
    print("CLIFF_MACRO_AUTHOR_PLAN",MODE,NEW,NEW_MI,QA)
    if MODE=="inspect":return
    import unreal # type: ignore
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
        "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("当前不是神兽联盟工程")
    lib=unreal.EditorAssetLibrary
    edit=unreal.MaterialEditingLibrary
    tool=unreal.AssetToolsHelpers.get_asset_tools()
    old=lib.load_asset(OLD)
    uv=lib.load_asset(UV)
    if not isinstance(old,unreal.Material) or not isinstance(uv,unreal.Material):
        raise TypeError("缺少原始已验收PBR Material")
    if edit.get_num_material_expressions(old)!=14:
        raise RuntimeError("原始Cliff世界对齐材质拓扑已变，拒绝盲目复制")
    textures={name:lib.load_asset(asset) for name,asset in MAPS.items()}
    if any(not isinstance(a,unreal.Texture2D) for a in textures.values()):
        raise TypeError("Cliff源4K材质引用不完整")
    for target,cls in ((NEW,unreal.Material),(NEW_MI,unreal.MaterialInstanceConstant),
                       (QA,unreal.MaterialInstanceConstant)):
        if lib.does_asset_exist(target) and not isinstance(lib.load_asset(target),cls):
            raise TypeError("同名资产类型冲突："+target)
        if MODE=="verify" and not lib.does_asset_exist(target):
            raise FileNotFoundError("候选资源还不存在："+target)

    if MODE=="apply" and not lib.does_asset_exist(NEW):
        mat=lib.duplicate_asset(OLD,NEW)
        if not isinstance(mat,unreal.Material):
            raise RuntimeError("UE复制三向材质失败")
        if edit.get_num_material_expressions(mat)!=14:
            raise RuntimeError("原始14节点没有正确复制")
        def n(cls,x,y):
            result=edit.create_material_expression(mat,cls,x,y)
            if result is None:raise RuntimeError("UE宏观Material节点创建失败")
            return result
        def link(a,b,pin):
            name="" if pin=="Sine" else pin
            if not edit.connect_material_expressions(a,"",b,name):
                raise RuntimeError("UE宏观坐标节点连线失败："+pin)
        wp=n(unreal.MaterialExpressionWorldPosition,-2100,-120)
        freq=n(unreal.MaterialExpressionVectorParameter,-2100,100)
        freq.set_editor_property("parameter_name",unreal.Name("CliffMacroWarpFrequency"))
        freq.set_editor_property("default_value",unreal.LinearColor(*FREQ,1))
        dot=n(unreal.MaterialExpressionDotProduct,-1810,-120)
        link(wp,dot,"A");link(freq,dot,"B")
        sine=n(unreal.MaterialExpressionSine,-1570,-120)
        link(dot,sine,"Sine")
        amplitude=n(unreal.MaterialExpressionVectorParameter,-1570,180)
        amplitude.set_editor_property("parameter_name",unreal.Name("CliffMacroWarpAmplitudeCm"))
        amplitude.set_editor_property("default_value",unreal.LinearColor(*AMP,1))
        displaced=n(unreal.MaterialExpressionMultiply,-1350,-100)
        link(sine,displaced,"A");link(amplitude,displaced,"B")
        absolute=n(unreal.MaterialExpressionAdd,-1090,-100)
        link(wp,absolute,"A");link(displaced,absolute,"B")
        calls=[e for e in edit.get_material_expressions(mat)
               if isinstance(e,unreal.MaterialExpressionMaterialFunctionCall)]
        if len(calls)!=3:raise RuntimeError("三向PBR应有3个世界投射函数")
        for function in calls:
            link(absolute,function,"WorldPosition")
        count=edit.get_num_material_expressions(mat)
        if count<21:raise RuntimeError("Cliff宏观变体不足21节点")
        results=edit.recompile_material(mat) or []
        for item in results:
            print("CLIFF_MACRO_SHADER_DIAGNOSTIC",str(item))
            if "error" in str(item).lower():
                raise RuntimeError("世界坐标扰动Shader编译失败")
        if not lib.save_loaded_asset(mat,only_if_is_dirty=False):
            raise RuntimeError("宏观WorldAligned UE保存失败")
        print("CLIFF_MACRO_WORLDALIGNED_SAVED",NEW,"nodes",count)

    if MODE=="apply" and not lib.does_asset_exist(NEW_MI):
        mi=tool.create_asset(NEW_MI.rsplit("/",1)[-1],FOLDER,
                             unreal.MaterialInstanceConstant,
                             unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(mi,unreal.MaterialInstanceConstant):
            raise TypeError("宏观Cliff MI未创建")
        edit.set_material_instance_parent(mi,lib.load_asset(NEW))
        for name,texture in textures.items():
            edit.set_material_instance_texture_parameter_value(
                mi,"CliffWorld"+name,texture)
        edit.set_material_instance_vector_parameter_value(
            mi,"CliffWorldTextureSizeCm",unreal.LinearColor(2000,2000,2000,1))
        edit.set_material_instance_vector_parameter_value(
            mi,"CliffMacroWarpFrequency",unreal.LinearColor(*FREQ,1))
        edit.set_material_instance_vector_parameter_value(
            mi,"CliffMacroWarpAmplitudeCm",unreal.LinearColor(*AMP,1))
        if not lib.save_loaded_asset(mi,only_if_is_dirty=False):
            raise RuntimeError("宏观Cliff MI未保存")
        print("CLIFF_MACRO_MI_SAVED",NEW_MI)

    if MODE=="apply" and not lib.does_asset_exist(QA):
        if not lib.does_directory_exist(QA_ROOT):
            lib.make_directory(QA_ROOT)
        mi=tool.create_asset(QA.rsplit("/",1)[-1],QA_ROOT,
                             unreal.MaterialInstanceConstant,
                             unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(mi,unreal.MaterialInstanceConstant):
            raise RuntimeError("UV亮度校准MI创建失败")
        edit.set_material_instance_parent(mi,uv)
        edit.set_material_instance_vector_parameter_value(
            mi,"CliffRockColorTint",unreal.LinearColor(.79,.79,.79,1))
        if not lib.save_loaded_asset(mi,only_if_is_dirty=False):
            raise RuntimeError("UV亮度校准MI未保存")
        print("CLIFF_MACRO_UV_QA_SAVED",QA)

    mat=lib.load_asset(NEW)
    macro=lib.load_asset(NEW_MI)
    qa=lib.load_asset(QA)
    if not isinstance(mat,unreal.Material) or not isinstance(
        macro,unreal.MaterialInstanceConstant) or not isinstance(
        qa,unreal.MaterialInstanceConstant):
        raise RuntimeError("UE资产保存后的类别不正确")
    count=edit.get_num_material_expressions(mat)
    functions=[e for e in edit.get_material_expressions(mat)
               if isinstance(e,unreal.MaterialExpressionMaterialFunctionCall)]
    if count<21 or len(functions)!=3 or mat.get_editor_property("tangent_space_normal"):
        raise RuntimeError("CliffMacro世界法线/3样本函数不完整")
    if macro.get_editor_property("parent")!=mat or qa.get_editor_property("parent")!=uv:
        raise RuntimeError("Cliff Macro/UV QA实例父类错位")
    for name,texture in textures.items():
        if edit.get_material_instance_texture_parameter_value(
            macro,"CliffWorld"+name)!=texture:
            raise RuntimeError("同源PBR三通道未对齐："+name)
    tex_size=edit.get_material_instance_vector_parameter_value(
        macro,"CliffWorldTextureSizeCm")
    if any(abs(v-2000)>1 for v in (tex_size.r,tex_size.g,tex_size.b)):
        raise RuntimeError("Cliff已批准20米真实扫描尺度发生改变")
    qa_tint=edit.get_material_instance_vector_parameter_value(qa,"CliffRockColorTint")
    if abs(qa_tint.r-.79)>.005:
        raise RuntimeError("UV对比亮度校准未保存")
    print("CLIFF_MACRO_UE_REAL_ASSETS_PASS",
          "material=1 macroMI=1 balancedUVMI=1",
          "nodes",count,"worldAlignedFunctions",len(functions),
          "originalMaterialPreserved=True userApprovalPending=True")


if __name__=="__main__":
    run()
