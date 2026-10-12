#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Village雪苔材质桥三层架构/配置隔离回归：仅源码和真实文件检查。

检查内容：平台母材质和项目MI真实存在、Village客户端标签式软资源异步绑定、
恢复生命周期、服务器资源排除真源及两个客户端Cook软引用声明。
此测试不会运行Unreal Shader/联机/IoStore；实际构建和Pak仍是独立门禁。
"""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[2]
SURFACE = ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformSurface"
CLIENT = ROOT / "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient"
PRESET = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core"
POLICY = ROOT / "Build/Game/ServerPresentationExclusions.json"
MAP = ROOT / "Game/Content/Development/MossSnow/Maps/L_DBA_MossSnowReview.umap"

def require(text, label, name):
    """稳定语义字符串核验并给出明确失败文件。"""
    if text not in label:
        raise RuntimeError("缺少" + text + ": " + name)

def main():
    require_files = (
        SURFACE / "Content/Materials/M_GP_Surface_SnowMoss_PBR.uasset",
        PRESET / "Content/Materials/Environment/MI_DBA_MossSnow_Cliff.uasset",
        MAP,
    )
    for path in require_files:
        if not path.is_file() or path.stat().st_size < 1000:
            raise RuntimeError("预期真实UE资源缺失或异常小: " + str(path))
    for name in ("Albedo","Normal","Orm","Height","Coverage"):
        resource = SURFACE / ("Content/Textures/Moss/T_GP_Moss_" + name + ".uasset")
        if not resource.is_file():
            raise RuntimeError("平台缺少苔藓Texture2D: " + str(resource))

    desc = json.loads((ROOT / "Game/Plugins/DivineBeasts/DBAClient/DBAClient.uplugin").read_text(encoding="utf8"))
    if not any(m["Name"]=="DivineBeastsPresentationClient" and m["Type"]=="ClientOnly" for m in desc["Modules"]):
        raise RuntimeError("项目苔藓适配不是ClientOnly")
    settings = (CLIENT / "Public/Settings/DivineBeastsVillageSnowSettings.h").read_text(encoding="utf8")
    defaults = (CLIENT / "Private/Settings/DivineBeastsVillageSnowSettings.cpp").read_text(encoding="utf8")
    cpp = (CLIENT / "Private/World/DivineBeastsVillageMossSurface.cpp").read_text(encoding="utf8")
    bridge = (CLIENT / "Private/World/DivineBeastsVillageSnowWorldSubsystem.cpp").read_text(encoding="utf8")
    for value in ("bEnableTaggedMossSnowRocks","MossSnowComponentTag","MossSnowRockMaterial"):
        require(value,settings,str(CLIENT))
    require("MI_DBA_MossSnow_Cliff", defaults, str(CLIENT))
    for value in ("ComponentHasTag", "AcquireResources", "EGamePlatformDataLifetime::World",
                  "IGamePlatformDataService", "ApplyMossMaterialToActor",
                  "HandleMossMaterialLoaded"):
        require(value,cpp,str(CLIENT))
    for value in ("RetainedMossOriginalMaterials", "TryRequestMossMaterial",
                  "ReleaseResources(OldMossLease)"):
        require(value,bridge,str(CLIENT))
    if any(value in cpp for value in ("LoadSynchronous(","LoadObject<","PrimaryActorTick","EditorSetLandscapeMaterial")):
        raise RuntimeError("客户端苔藓路径含同步加载/逐帧Tick/EditorOnly")
    policy = json.loads(POLICY.read_text(encoding="utf8"))
    paths = {x["PackagePath"] for x in policy["CookDirectories"] if x["Profiles"]=="AllServers"}
    needed = {"/GamePlatformSurface/Textures/Moss",
              "/DBAPresentationPack_Core/Materials/Environment",
              "/Game/Development/MossSnow"}
    if not needed.issubset(paths):
        raise RuntimeError("专服排除真源缺少新增纯表现目录: " + str(needed-paths))
    for config in ("FrontEndClient", "VillageDevelopmentClient"):
        text=(ROOT / f"Game/Config/Custom/{config}/DefaultGame.ini").read_text(encoding="utf8")
        for directory in ("/GamePlatformSurface/Textures/Moss","/DBAPresentationPack_Core/Materials/Environment"):
            require(f'+DirectoriesToAlwaysCook=(Path="{directory}")',text,config)
    print("VILLAGE_MOSS_SNOW_3TIER_STATIC_PASS")
    print("VILLAGE_MOSS_SNOW_COOK_SCOPE_STATIC_PASS")
    print("VILLAGE_MOSS_SNOW_ASSET_SOURCE_FILE_PRESENT")

if __name__ == "__main__":
    main()
