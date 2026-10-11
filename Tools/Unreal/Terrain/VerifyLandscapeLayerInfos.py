#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8十份真实Landscape LayerInfo独立反射回读审计（只读）。

本脚本只能由DivineBeastsArena正式工程的原生UnrealEditor-Cmd Python插件执行。
必须确认AssetPath、ULandscapeLayerInfoObject类别、LayerName与十层母材质命名吻合，
且BlendMethod=FinalWeightBlending（权重混合）。不会创建、保存或更改资产/正式Village地图。
无论文件是否在磁盘存在，都必须由引擎成功加载实际UObject才算通过。
"""

from __future__ import annotations

BASE = "/DBAWorldPack_Village/Terrain/LayerInfos"
LAYERS = (
    "Grass", "ForestSoil", "Dirt", "Rock", "Scree",
    "Sand", "Gravel", "RoadDirt", "FarmSoil", "ShoreMud",
)


def verify() -> None:
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("必须运行真实UE5.8 UnrealEditor-Cmd，不可用普通Python伪造LayerInfo") from exc

    project = unreal.Paths.get_project_file_path().replace("\\", "/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("当前Editor加载的不是正式神兽联盟工程")

    library = unreal.EditorAssetLibrary
    expected = {f"{BASE}/LI_DBA_Terrain_{layer}" for layer in LAYERS}
    existing = {
        item.rsplit(".", 1)[0]
        for item in library.list_assets(BASE, recursive=False, include_folder=False)
        if item.endswith(".uasset")
    }
    # 不同UE版本EditorAssetLibrary返回的资产路径可能不带.uasset，故以真实UObject逐项检查。
    verified = []
    for layer in LAYERS:
        package = f"{BASE}/LI_DBA_Terrain_{layer}"
        if not library.does_asset_exist(package):
            raise FileNotFoundError("缺少真实UE5.8 Landscape LayerInfo：" + package)
        obj = library.load_asset(package)
        if not isinstance(obj, unreal.LandscapeLayerInfoObject):
            raise TypeError("目标资产不是ULandscapeLayerInfoObject：" + package)
        target_name = str(obj.get_editor_property("layer_name"))
        if target_name != f"LS_{layer}":
            raise ValueError("LayerName与十层Landscape材质不匹配：" + package + "=>" + target_name)
        blend = obj.get_editor_property("blend_method")
        if blend != unreal.LandscapeTargetLayerBlendMethod.FINAL_WEIGHT_BLENDING:
            raise ValueError("十层基础材质必须使用FinalWeightBlending：" + package)
        if obj.get_path_name() != f"{package}.LI_DBA_Terrain_{layer}":
            raise RuntimeError("引擎回读实际UObject路径与约定不一致：" + package)
        verified.append(package)
        print("TERRAIN_LAYERINFO_ASSET_VERIFIED",
              package, "LayerName=" + target_name, "WeightBlend=True")
    if len(verified) != 10 or len(set(verified)) != 10:
        raise RuntimeError("实际通过UE反射的图层数量不是10")
    print("TERRAIN_LAYERINFO_10_ASSETS_REAL_UE_VERIFY_PASS",
          "count=10", "project=" + project)


if __name__ == "__main__":
    verify()
