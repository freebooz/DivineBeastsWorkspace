#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01宏观扰动过度模糊后的轻扰动对照MI，保留旧材质原样。

对源WorldAlignedMacro独立创建专供开发对照的低振幅材质实例（20/-15/12cm），
保留20m真实扫描尺度、相同三向投射Normal/ORM，不能替换正式Village地图。
完成仅表示UE真实资产生成和参数回读；画面清晰度必须独立D3D12截图再审。
"""
from __future__ import annotations
import unreal
ROOT="/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials"
PARENT=ROOT+"/M_DBA_CliffRock01_WorldAlignedMacro"
TARGET="/Game/Development/Terrain/Materials/MI_DBA_CliffRock01_WorldAlignedMacro_LowWarpReview"
AMPLITUDE=unreal.LinearColor(20.,-15.,12.,1.)

def main():
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("Cliff项目审查要求正式DivineBeastsArena工程")
    lib=unreal.EditorAssetLibrary
    edit=unreal.MaterialEditingLibrary
    parent=lib.load_asset(PARENT)
    if not isinstance(parent,unreal.Material) or edit.get_num_material_expressions(parent)<21:
        raise RuntimeError("真实宏观扫描母材质尚未保存或结构已更改")
    if not lib.does_asset_exist(TARGET):
        folder,name=TARGET.rsplit("/",1)
        if not lib.does_directory_exist(folder):lib.make_directory(folder)
        instance=unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name,folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(instance,unreal.MaterialInstanceConstant):
            raise RuntimeError("UE未创建真实低振幅宏观材质实例")
        edit.set_material_instance_parent(instance,parent)
        edit.set_material_instance_vector_parameter_value(instance,"CliffMacroWarpAmplitudeCm",AMPLITUDE)
        edit.set_material_instance_vector_parameter_value(
            instance,"CliffWorldTextureSizeCm",unreal.LinearColor(2000,2000,2000,1))
        if not lib.save_loaded_asset(instance,only_if_is_dirty=False):
            raise RuntimeError("真实UE低振幅MI保存失败")
        print("CLIFFROCK_LOW_WARP_UE_MI_CREATED",TARGET)
    instance=lib.load_asset(TARGET)
    if not isinstance(instance,unreal.MaterialInstanceConstant) or instance.get_editor_property("parent")!=parent:
        raise RuntimeError("低振幅WorldAlignedMacro项目MI资产/父类回读失败")
    value=edit.get_material_instance_vector_parameter_value(instance,"CliffMacroWarpAmplitudeCm")
    if abs(value.r-20)>.1 or abs(value.g+15)>.1 or abs(value.b-12)>.1:
        raise RuntimeError("宏观扰动振幅并未实际写入")
    scale=edit.get_material_instance_vector_parameter_value(instance,"CliffWorldTextureSizeCm")
    if abs(scale.r-2000)>1:
        raise RuntimeError("扫描真实20m物理尺度未维持")
    print("CLIFFROCK_LOW_WARP_NATIVE_UE_VERIFY_PASS",TARGET,
          "amplitudeCm",value.r,value.g,value.b,"textureScaleCm",scale.r,
          "manual_visual_approval_pending=True")

if __name__=="__main__":main()