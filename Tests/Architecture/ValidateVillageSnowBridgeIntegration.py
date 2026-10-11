#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""新手村写实雪客户端桥静态门禁与UE地图硬引用审计证据汇总（只读）。

目标：运行时不能调用编辑器Only Landscape换材质；不得将纯客户端Surface/VFX硬链接
进共享Village地图；仅收到WeatherClient值事件后以GamePlatformData异步资源租约
更新已有Landscape MID。证据包括UE Editor实际输出的地图资产依赖JSON。
本脚本不代替C++完整编译、Cook、真实天气网络、人工截图。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GAME = ROOT / "Game"
MOD = GAME / "Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient"
WORLD = MOD / "Private/World/DivineBeastsVillageSnowWorldSubsystem.cpp"
SETTING = MOD / "Private/Settings/DivineBeastsVillageSnowSettings.cpp"
BUILDS = MOD / "DivineBeastsPresentationClient.Build.cs"
MAP = GAME / "Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap"
ART = GAME / "Saved/Validation/VillageSnowBridgeAssetAudit_20261011.json"
MANIFEST = ROOT / "Tools/Unreal/Weather/VillageLandscapeBridgeSourceManifest.json"

def require(text: str, term: str, file: Path) -> None:
    if term not in text:
        raise RuntimeError(f"{file}: 缺少必要协议：{term}")


def main() -> None:
    module = json.loads((GAME / "Plugins/DivineBeasts/DBAClient/DBAClient.uplugin").read_text(encoding="utf-8-sig"))
    matches = [m for m in module["Modules"] if m["Name"] == "DivineBeastsPresentationClient"]
    if len(matches) != 1 or matches[0]["Type"] != "ClientOnly":
        raise RuntimeError("项目雪地运行逻辑未隔离在ClientOnly模块")
    cpp = WORLD.read_text(encoding="utf-8")
    cfg = SETTING.read_text(encoding="utf-8")
    build = BUILDS.read_text(encoding="utf-8")
    for field in ("GetSubsystem<UGamePlatformWeatherClientWorldSubsystem>",
                  "AddVisualChangedHandler", "IGamePlatformDataService::Get",
                  "AcquireResources(", "EGamePlatformDataLifetime::World",
                  "SetLandscapeMaterialTextureParameterValue(",
                  "SetLandscapeMaterialScalarParameterValue(",
                  "GetNetMode() == NM_DedicatedServer",
                  "ReleaseResources(", "IsVillageWorldPackage"):
        require(cpp, field, WORLD)
    if any(symbol in cpp for symbol in ("EditorSetLandscapeMaterial", "LoadObject<",
                                         "LoadSynchronous(", "PrimaryActorTick")):
        raise RuntimeError("运行时违规执行EditorOnly材质变更、同步加载或逐帧Tick")
    for symbol in ("GamePlatformData", "GamePlatformWeatherClient", "Landscape"):
        require(build, f'"{symbol}"', BUILDS)
    for symbol in ("T_GP_Snow_CoverageNoise", "T_GP_Weather_SnowAlbedo",
                   "T_GP_Weather_SnowNormal"):
        require(cfg, symbol, SETTING)

    source = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if source.get("schemaVersion") != 1 or len(source.get("textures", [])) != 3:
        raise RuntimeError("共享地形基础纹理Manifest缺失或记录异常")
    for rec in source["textures"]:
        path = ROOT / rec["sourcePath"]
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != rec["sha256"]:
            raise RuntimeError("共享地形中性纹理被修改或缺失：" + str(path))
        if not rec.get("serverBaselineSafe") or not rec["assetPath"].startswith(
                "/DBAWorldPack_Village/Materials/Landscape/Defaults/"):
            raise RuntimeError("地形共用资源错误归属到纯客户端插件")
    if not MAP.is_file():
        raise RuntimeError("正式Village地图缺失")
    if not ART.is_file():
        raise RuntimeError("尚无UE原生Landscape地图硬引用审计记录")
    actual = json.loads(ART.read_text(encoding="utf-8"))
    if (actual["world"] != "/DBAWorldPack_Village/Maps/L_Village_Start"
            or actual["landscapeCount"] != 1
            or not actual["dynamicLandscapeMaterialInstancesEnabled"]
            or actual["materialExpressionCount"] < 40
            or actual["forbiddenClientOnlyMaterialDependencies"] != 0):
        raise RuntimeError("原生UE的地图/MID或依赖审计结果不符合生产要求")
    forbidden = ("/GamePlatformSurface/", "/GamePlatformVFX/", "/DBAPresentationPack_Core/")
    if any(path.startswith(forbidden) for path in
           actual["materialPackageDependencies"] + actual["worldPackageDependencies"]):
        raise RuntimeError("共享Village地图或母材质仍引用了客户端特效插件")

    configs = (GAME / "Config/Custom/VillageDevelopmentClient/DefaultGame.ini").read_text(
        encoding="utf-8-sig")
    if '+DirectoriesToAlwaysCook=(Path="/GamePlatformSurface/Textures/Snow")' not in configs:
        raise RuntimeError("Village开发客户端没有Cook异步高精积雪遮罩")
    server = (GAME / "Config/Custom/DedicatedServer/DefaultGame.ini").read_text(
        encoding="utf-8-sig")
    if '+DirectoriesToNeverCook=(Path="/GamePlatformSurface/Textures/Snow")' not in server:
        raise RuntimeError("DedicatedServer错误打包客户端Surface雪纹理")
    print("VILLAGE_SNOW_BRIDGE_3TIER_STATIC_PASS")
    print("VILLAGE_SNOW_MAP_HARD_REFERENCES_ISOLATED")
    print("VILLAGE_SNOW_DEFAULT_TEXTURE_SOURCE_QA_PASS 3")


if __name__ == "__main__":
    main()
