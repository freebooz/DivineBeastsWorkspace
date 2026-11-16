#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM正式扫描地形PBR三档质量候选材质实例制作与独立验证。

归属：第三层DBAWorldPack_Village/Development/Terrain/Production10的客户端审核内容；只创建新MI，不替换正式Village共享地图、
已有审核MI、平台母材质或服务器权威图层。旧母材质原配方保留高档启用高度混合；
新函数化候选因UE5.8静态开关写入限制，三个档位仅继承关闭高度混合并用有效标量调节，
不得假称高档已启用HeightBlend。十基础层保持可见，不伪报GPU性能改善。
真实质量/帧率必须经UE5.8真实GPU与Client/Server Cook分别验收。
入口环境变量TERRAIN_QUALITY_PROFILES_MODE：inspect/apply/verify（默认inspect）。
"""
from __future__ import annotations
import os

MODE=os.environ.get("TERRAIN_QUALITY_PROFILES_MODE","inspect").strip().lower()
# 原有质量档保持不变；仅显式 opt-in 才给函数化候选生成独立三档MI。
FUNCTION_INTEGRATION=os.environ.get("TERRAIN_FUNCTION_INTEGRATION","0").strip()=="1"
BASE="/DBAWorldPack_Village/Development/Terrain/Production10"
MASTER=BASE+("/M_DBA_Terrain_Production10_FunctionIntegrated" if FUNCTION_INTEGRATION
             else "/M_DBA_Terrain_Production10")
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
if FUNCTION_INTEGRATION:
    # 现有UE5.8原生MI静态开关写入返回False（实际运行证据）；候选三档不宣称启用HeightBlend。
    # 保留原母材质默认HeightBlend=false，三档只对PBR扫描色彩/UV/距离做视觉参数变体。
    # 原正式版高档HeightBlend=true的设计不改动；其应用需另经引擎静态开关API专项修复。
    for spec in CONFIG.values():
        spec["HeightBlend"]=False
        # 高度采样已被静态开关剥离，该参数不可作为有效MI覆盖项。
        spec.pop("TerrainHeightBlendStrength",None)
    CONFIG["High"].update({"TerrainMacroColorStrength":0.18,
                           "TerrainDistanceFadeStartCm":4000.0,
                           "TerrainDistanceFadeEndCm":18000.0})
    CONFIG["Medium"].update({"TerrainMacroColorStrength":0.12,
                             "TerrainDistanceFadeStartCm":4500.0,
                             "TerrainDistanceFadeEndCm":17000.0})
    CONFIG["Low"].update({"TerrainMacroColorStrength":0.05,
                          "TerrainDistanceFadeStartCm":6000.0,
                          "TerrainDistanceFadeEndCm":16000.0})


def execute():
    if MODE not in ("inspect","apply","verify"):
        raise ValueError("质量实例制作模式必须为inspect、apply、verify")
    if MODE=="inspect":
        print("TERRAIN_QUALITY_PROFILES_PLAN quality=High,Medium,Low"
              " height_switch="+("all_inherit_false" if FUNCTION_INTEGRATION
                                   else "legacy_high_on_medium_low_off")
              +" no_authoritative_server_changes")
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
    prefix=("MI_DBA_Terrain_Production10_FunctionIntegrated_"
            if FUNCTION_INTEGRATION else "MI_DBA_Terrain_Production10_")
    paths={level:BASE+"/"+prefix+level for level in CONFIG}
    existing={level:assets.does_asset_exist(path) for level,path in paths.items()}
    if MODE=="apply" and any(existing.values()) and not all(existing.values()):
        raise RuntimeError("已有不完整质量实例集合，禁止覆盖/隐式合并")
    if MODE=="verify" and not all(existing.values()):
        raise RuntimeError("三档原生质量MI尚未全部生成")
    # 严格用真实母材质的标量参数列表限定可写项，防止错拼参数名恰好返回默认0导致误报。
    parent_scalars={str(expr.get_editor_property("parameter_name"))
                    for expr in edit.get_material_expressions(master)
                    if isinstance(expr,unreal.MaterialExpressionScalarParameter)}
    for level,spec in CONFIG.items():
        for param in spec:
            if param!="HeightBlend" and param not in parent_scalars:
                raise RuntimeError("质量档引用了不存在的母材质ScalarParameter："+level+"/"+param)
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
            # UE5.8必须先刷新父材质参数缓存，再给新建MI写StaticSwitch，否则未注册时返回False。
            edit.update_material_instance(inst)
            did_override=bool(set_switch(inst,"TerrainEnableHeightBlend",spec["HeightBlend"]))
            if not did_override:
                # 只有函数化候选且目标就是母材质默认false时，允许继承默认，不宣称override成功。
                active=[expr for expr in edit.get_material_expressions(master)
                        if isinstance(expr,unreal.MaterialExpressionStaticSwitchParameter)
                        and str(expr.get_editor_property("parameter_name"))=="TerrainEnableHeightBlend"]
                if (not FUNCTION_INTEGRATION or spec["HeightBlend"] or len(active)!=10 or
                    any(bool(expr.get_editor_property("default_value")) for expr in active)):
                    raise RuntimeError("原生高度混合Static Switch参数未写入："+level)
                print("TERRAIN_QUALITY_STATIC_SWITCH_INHERITS_DEFAULT_FALSE",level,
                      "override_api_unavailable=true")
            for param,value in spec.items():
                if param=="HeightBlend":
                    continue
                # UE5.8对“与父材质默认相同”的写入可能返回False；只在回读真值相等时认可继承。
                written=bool(edit.set_material_instance_scalar_parameter_value(inst,param,float(value)))
                if not written:
                    inherited=float(edit.get_material_instance_scalar_parameter_value(inst,param))
                    if abs(inherited-float(value))>0.0001:
                        raise RuntimeError("质量参数未写入："+level+"/"+param+
                                           " actual="+str(inherited))
                    print("TERRAIN_QUALITY_SCALAR_INHERITS_MATCHING_DEFAULT",level,param)
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
          " function_integrated="+str(FUNCTION_INTEGRATION)
          +" only_parameter_presets_no_measured_gpu_savings")


if __name__=="__main__":
    execute()
