#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01世界对齐材质的2米Scale纯审核实例，不进入正式Village。

因正式扫描宽20米，默认材质预览球直径约1米，采样面积仅占贴图很小区域；
另外制作专门的开发目录MI使用200cm三轴投射尺度，仅用于识别UE纹理是否真正
投影与法线是否正确，不修改正式WorldAligned 2000cm MI和全部原Texture2D。
CLIFFROCK_PREVIEW_MI_MODE=apply/verify/inspect，真实UE创建并回读。
"""
from __future__ import annotations

import os

PARENT="/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials/M_DBA_CliffRock01_WorldAligned"
ROOT="/Game/Development/Terrain/Materials"
TARGET=ROOT+"/MI_DBA_CliffRock01_WorldAligned_Review2m"
MODE=os.getenv("CLIFFROCK_PREVIEW_MI_MODE","inspect").lower().strip()


def main()->None:
    if MODE not in ("inspect","apply","verify"):
        raise ValueError("调试三向MI只允许inspect/apply/verify")
    print("CLIFFROCK_WORLDALIGN_QA_MI_PLAN",MODE,TARGET)
    if MODE=="inspect":return
    try:
        import unreal # type: ignore
    except ImportError as e:
        raise RuntimeError("必须在真实UE5.8 Editor创建调试MI") from e
    lib=unreal.EditorAssetLibrary
    edit=unreal.MaterialEditingLibrary
    parent=lib.load_asset(PARENT)
    if not isinstance(parent,unreal.Material):
        raise FileNotFoundError("已保存的完整WorldAligned母材质不可加载")
    if edit.get_num_material_expressions(parent)<14:
        raise RuntimeError("WorldAligned母材质实际连接节点不完整")
    if lib.does_asset_exist(TARGET):
        mi=lib.load_asset(TARGET)
        if not isinstance(mi,unreal.MaterialInstanceConstant):
            raise TypeError("开发调试目录存在同名非材质实例")
    else:
        if MODE=="verify":
            raise FileNotFoundError("审核专用MI尚未创建")
        if not lib.does_directory_exist(ROOT):
            lib.make_directory(ROOT)
        name=TARGET.rsplit("/",1)[-1]
        mi=unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name,ROOT,unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(mi,unreal.MaterialInstanceConstant):
            raise RuntimeError("UE开发审核MI创建失败")
        edit.set_material_instance_parent(mi,parent)
        edit.set_material_instance_vector_parameter_value(
            mi,"CliffWorldTextureSizeCm",unreal.LinearColor(200,200,200,1))
        if not lib.save_loaded_asset(mi,only_if_is_dirty=False):
            raise RuntimeError("UE审核MI SavePackage失败")
        print("CLIFFROCK_WORLDALIGN_QA_MI_SAVED",TARGET)
    if mi.get_editor_property("parent")!=parent:
        raise RuntimeError("审核MI未继承项目原生三向投射母材质")
    tex_size=edit.get_material_instance_vector_parameter_value(mi,"CliffWorldTextureSizeCm")
    if any(abs(a-200)>1 for a in (tex_size.r,tex_size.g,tex_size.b)):
        raise RuntimeError("审核2米MI纹理采样尺度不正确")
    print("CLIFFROCK_WORLDALIGN_QA_MI_2M_VERIFY_PASS",TARGET,"worldTextureCm",tex_size.r)


if __name__=="__main__":
    main()
