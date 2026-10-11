#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟项目层复用现有PCG平台放置器Blueprint：真实UE5.8复制与域配置。

不创建平行PCG运行框架，来源是当前已登记DBAWorldPack_Village的
BP_PCG_Forest/Rock/WaterBank/Road四份真实蓝图，复制至Terrain/PCG/Blueprints下独占名称。
四个新蓝图用于独立Gaea地貌QA场景，配置已批准Domain、WorldStage、Schema主版本和稳定Seed。
Graph/Definition必须由后来受控QA关卡工具按真实LandscapeLayerWeights绑定，创建此Blueprint
**不表示PCG真正生成，不能算作G01-G16通过**。不修改已有原始BP及正式世界。
默认TERRAIN_PCG_BLUEPRINT_MODE=inspect；apply显式创建/保存/回读，不覆盖未知资产。
"""

from __future__ import annotations

import os

MODE=os.getenv("TERRAIN_PCG_BLUEPRINT_MODE","inspect").lower().strip()
ROOT="/DBAWorldPack_Village"
SRC=ROOT+"/PCG/Blueprints"
TARGET=ROOT+"/Terrain/PCG/Blueprints"
SPEC=(
    ("BP_PCG_Forest","BP_DBA_Terrain_ForestBiome","Forest.Canopy","SCATTER",20261011),
    ("BP_PCG_Rock","BP_DBA_Terrain_RockBiome","Rock.Scatter","SCATTER",20261013),
    ("BP_PCG_WaterBank","BP_DBA_Terrain_ShoreBiome","Water.Bank","INTERFACE_BANDS",20261017),
    ("BP_PCG_Road","BP_DBA_Terrain_RoadBiome","Road.Surface","NETWORKS",20261019),
)

def execute()->None:
    if MODE not in ("inspect","apply","verify"):
        raise ValueError("TERRAIN_PCG_BLUEPRINT_MODE仅允许inspect/apply/verify")
    for src,name,domain,stage,seed in SPEC:
        print("TERRAIN_PCG_BLUEPRINT_PLAN",SRC+"/"+src,TARGET+"/"+name,domain,stage,seed)
    if MODE=="inspect":
        return
    try:
        import unreal  # type: ignore
    except ImportError as e:
        raise RuntimeError("PCG蓝图只能在真实UE Editor制作") from e

    lib=unreal.EditorAssetLibrary
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("未加载正确神兽联盟UE5.8工程")
    stages=getattr(unreal,"GamePlatformPCGWorldStage",None)
    if stages is None:
        raise RuntimeError("不存在已加载的平台PCG WorldStage枚举")
    base_class=getattr(unreal,"GamePlatformPCGActorBase",None)
    if base_class is None:
        raise RuntimeError("不存在已加载平台PCG放置器类型")
    original={}
    for src,name,domain,stage,seed in SPEC:
        source=SRC+"/"+src
        cl=lib.load_blueprint_class(source)
        if not lib.does_asset_exist(source) or cl is None:
            raise FileNotFoundError("既有PCG蓝图未加载："+source)
        obj=unreal.get_default_object(cl)
        for field in ("domain_id","world_stage","required_schema_major","seed","locked_graph"):
            # 缺少任何业务关键值的编辑入口则禁止创建半成品子蓝图。
            try: obj.get_editor_property(field)
            except Exception as ex:
                raise RuntimeError("现有平台PCG Blueprint不支持字段"+field+" "+str(ex))
        if getattr(stages,stage,None) is None:
            raise RuntimeError("未找到正式PCG阶段："+stage)
        original[source]=cl
        target=TARGET+"/"+name
        if lib.does_asset_exist(target) and not isinstance(lib.load_asset(target),unreal.Blueprint):
            raise TypeError("目标蓝图名被其他UObject占用："+target)
        if MODE=="verify" and not lib.does_asset_exist(target):
            raise FileNotFoundError("已有项目Gaea PCG验证蓝图未创建："+target)
    if MODE=="apply" and not lib.does_directory_exist(TARGET):
        lib.make_directory(TARGET)

    created=0
    for src,name,domain,stage,seed in SPEC:
        target=TARGET+"/"+name
        if not lib.does_asset_exist(target):
            duplicated=lib.duplicate_asset(SRC+"/"+src,target)
            if not isinstance(duplicated,unreal.Blueprint):
                raise RuntimeError("UE AssetTools没有真正复制PCG ChildBlueprint："+target)
            created+=1
        cl=lib.load_blueprint_class(target)
        if cl is None or not unreal.MathLibrary.class_is_child_of(cl,base_class):
            raise TypeError("Blueprint生成类不继承GamePlatformPCGActorBase："+target)
        default=unreal.get_default_object(cl)
        if MODE=="apply":
            default.set_editor_property("domain_id",unreal.Name(domain))
            default.set_editor_property("world_stage",getattr(stages,stage))
            default.set_editor_property("required_schema_major",1)
            default.set_editor_property("seed",seed)
            default.set_editor_property("locked_graph",True)
            # Blueprint只维护可复用项目放置器默认值；Graph/Profile由独立场景配置并校验。
            asset=lib.load_asset(target)
            if not lib.save_loaded_asset(asset,only_if_is_dirty=False):
                raise RuntimeError("UE蓝图真实SavePackage失败："+target)
        refreshed=unreal.get_default_object(lib.load_blueprint_class(target))
        if (str(refreshed.get_editor_property("domain_id"))!=domain or
                refreshed.get_editor_property("seed")!=seed or
                refreshed.get_editor_property("required_schema_major")!=1):
            raise RuntimeError("UE蓝图的PCG Domain/Seed/Schema回读不一致："+target)
        print("TERRAIN_PCG_BLUEPRINT_REAL_ASSET_VERIFIED",target,
              domain,stage,"source",src)
    print("TERRAIN_PCG_BLUEPRINT_UE_PASS count=4 newlyCreated=",created,
          "graph_execution_unverified=True")

if __name__=="__main__":
    execute()
