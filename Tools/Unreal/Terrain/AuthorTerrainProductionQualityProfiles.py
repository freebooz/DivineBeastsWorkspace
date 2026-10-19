#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM正式扫描地形PBR三档质量候选材质实例制作与独立验证。

归属：第三层纯客户端DBAPresentationPack_Core；只创建新MI，不替换正式Village共享地图、
已有审核MI、平台母材质或服务器权威图层。高档以静态开关启用微高度混合，
中低档禁用微高度采样；草地等十基础层依旧共同可见，不伪报GPU性能改善。
真实质量/帧率必须经UE5.8真实GPU与Client/Server Cook分别验收。
入口环境变量TERRAIN_QUALITY_PROFILES_MODE：inspect/apply/verify（默认inspect）。
"""
from __future__ import annotations
import os

MODE=os.environ.get("TERRAIN_QUALITY_PROFILES_MODE","inspect").strip().lower()
BASE="/DBAPresentationPack_Core/Materials/Terrain"
MASTER=BASE+"/M_DBA_Terrain_Production10"
CONFIG={
    "High": {"HeightBlend":True,"TerrainHeightBlendStrength":0.20,
             "TerrainDetailTiling":12.0,"TerrainMossAmount":0.48,
             "TerrainSnowMacroTiling":2.2},
    "Medium":{"HeightBlend":False,"TerrainHeightBlendStrength":0.0,
             "TerrainDetailTiling":10.0,"TerrainMossAmount":0.35,
             "TerrainSnowMacroTiling":2.0},
    "Low":  {"HeightBlend":False,"TerrainHeightBlendStrength":0.0,
             "TerrainDetailTiling":8.0,"TerrainMossAmount":0.20,
             "TerrainSnowMacroTiling":1.8},
}


def execute():
    if MODE not in ("inspect","apply","verify"):
        raise ValueError("质量实例制作模式必须为inspect、apply、verify")
    if MODE=="inspect":
        print("TERRAIN_QUALITY_PROFILES_PLAN quality=High,Medium,Low"
              " high_optional_height=on medium_low_optional_height=off"
              " no_authoritative_server_changes")
        return
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("只能由正式UE5.8编辑器创建材质实例") from exc
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("不允许修改非SSLM正式项目")
    assets=unreal.EditorAssetLibrary
    edit=unreal.MaterialEditingLibrary
    master=assets.load_asset(MASTER)
    if not isinstance(master,unreal.Material):
        raise RuntimeError("须先完成正式十层母材质保存及独立重载")
    set_switch=getattr(edit,"set_material_instance_static_switch_parameter_value",None)
    get_switch=getattr(edit,"get_material_instance_static_switch_parameter_value",None)
    if not callable(set_switch) or not callable(get_switch):
        raise RuntimeError("锁定UE5.8缺少MI静态开关原生接口，不创建半成品")
    paths={level:BASE+"/MI_DBA_Terrain_Production10_"+level for level in CONFIG}
    existing={level:assets.does_asset_exist(path) for level,path in paths.items()}
    if MODE=="apply" and any(existing.values()) and not all(existing.values()):
        raise RuntimeError("已有不完整质量实例集合，禁止覆盖/隐式合并")
    if MODE=="verify" and not all(existing.values()):
        raise RuntimeError("三档原生质量MI尚未全部生成")
    if MODE=="apply" and not all(existing.values()):
        tool=unreal.AssetToolsHelpers.get_asset_tools()
        for level,spec in CONFIG.items():
            path=paths[level]
            name=path.rsplit("/",1)[1]
            inst=tool.create_asset(name,BASE,unreal.MaterialInstanceConstant,
                                   unreal.MaterialInstanceConstantFactoryNew())
            if not isinstance(inst,unreal.MaterialInstanceConstant):
                raise RuntimeError("MI创建失败："+path)
            edit.set_material_instance_parent(inst,master)
            if not set_switch(inst,"TerrainEnableHeightBlend",spec["HeightBlend"]):
                raise RuntimeError("原生高度混合Static Switch参数未写入："+level)
            for param,value in spec.items():
                if param=="HeightBlend":
                    continue
                if not edit.set_material_instance_scalar_parameter_value(inst,param,float(value)):
                    raise RuntimeError("质量参数未写入："+level+"/"+param)
            edit.update_material_instance(inst)
            if not assets.save_loaded_asset(inst,only_if_is_dirty=False):
                raise RuntimeError("原生MI保存失败："+path)
            print("TERRAIN_QUALITY_PROFILE_SAVED",level,path)
    for level,spec in CONFIG.items():
        path=paths[level]
        inst=assets.load_asset(path)
        if not isinstance(inst,unreal.MaterialInstanceConstant):
            raise RuntimeError("质量实例不存在："+path)
        if inst.get_editor_property("parent")!=master:
            raise RuntimeError("质量MI父材质偏离正式十层母材质："+level)
        if get_switch(inst,"TerrainEnableHeightBlend")!=spec["HeightBlend"]:
            raise RuntimeError("HeightBlend静态开关实际读取不符："+level)
        for param,expected in spec.items():
            if param=="HeightBlend":
                continue
            actual=edit.get_material_instance_scalar_parameter_value(inst,param)
            if abs(float(actual)-float(expected))>0.0001:
                raise RuntimeError("质量档真实参数不符："+level+"/"+param)
        print("TERRAIN_QUALITY_PROFILE_VALIDATED",level,
              "height_switch="+str(spec["HeightBlend"]),
              "moss="+str(spec["TerrainMossAmount"]))
    print("TERRAIN_QUALITY_PROFILES_UE_VERIFIED count=3"
          " only_parameter_presets_no_measured_gpu_savings")


if __name__=="__main__":
    execute()
