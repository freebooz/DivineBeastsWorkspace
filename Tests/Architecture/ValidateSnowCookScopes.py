#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""雪天气通用插件层级、Client/Server Cook和审查地图隔离门禁（只读）。

注意：这是配置/静态门禁，不是实际Cook，无法验证动态硬引用形成的烘焙闭包。
最终仍须独立Client Cook与DedicatedServer Cook + AssetRegistry/Pak检查。
"""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GAME = ROOT / "Game"
SURFACE = GAME / "Plugins/GamePlatform/Presentation/GamePlatformSurface"
VFX = GAME / "Plugins/GamePlatform/Presentation/GamePlatformVFX"
PRESENT = GAME / "Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core"
CLIENT_CONFIG = GAME / "Config/Custom/FrontEndClient/DefaultGame.ini"
SERVER_CONFIG = GAME / "Config/Custom/DedicatedServer/DefaultGame.ini"
PAK_RULES = GAME / "Config/Custom/DedicatedServer/DefaultPakFileRules.ini"


def main() -> None:
    client = CLIENT_CONFIG.read_text(encoding="utf-8-sig")
    server = SERVER_CONFIG.read_text(encoding="utf-8-sig")
    pak = PAK_RULES.read_text(encoding="utf-8-sig")
    desc = json.loads((PRESENT / "DBAPresentationPack_Core.uplugin").read_text(encoding="utf-8-sig"))
    plugins = {node["Name"]: node for node in desc["Plugins"]}
    assert desc["CanContainContent"] and not desc["EnabledByDefault"]
    for name in ("GamePlatformVFX", "GamePlatformSurface", "GamePlatformData"):
        assert name in plugins, f"第三层缺少跨游戏通用插件依赖：{name}"
        assert plugins[name]["TargetAllowList"] == ["Client", "Editor"], f"服务器不可链接表现插件：{name}"
    for package in ("/GamePlatformSurface/Textures/Snow", "/DBAPresentationPack_Core/Materials/Snow"):
        allow = f'+DirectoriesToAlwaysCook=(Path="{package}")'
        deny = f'+DirectoriesToNeverCook=(Path="{package}")'
        assert allow in client, f"客户端缺少雪资源Cook合同: {allow}"
        assert deny in server, f"DedicatedServer缺少排除合同: {deny}"
    assert '+DirectoriesToNeverCook=(Path="/Game/Development/Snow")' in server
    assert '+Files=".../Content/Development/Snow/..."' in pak
    assert '+Files=".../Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/..."' in pak
    assert '+DirectoriesToAlwaysCook=(Path="/Game/Development/Snow")' not in client

    # 检查物理成果：文件存在只证明资产落盘，不能据此宣称资产可被引擎加载。
    targets = (
        SURFACE / "Content/Materials/M_GP_SnowCover_Detailed.uasset",
        SURFACE / "Content/Textures/Snow/T_GP_Snow_MicroHeight.uasset",
        SURFACE / "Content/Textures/Snow/T_GP_Snow_CoverageNoise.uasset",
        VFX / "Content/Weather/Niagara/NS_GP_Weather_Snow_Detailed.uasset",
        VFX / "Content/Weather/Materials/M_GP_VFX_SnowGroundPuff.uasset",
        VFX / "Content/Weather/Definitions/DA_GP_VFX_Weather_Snow.uasset",
        PRESENT / "Content/Materials/Snow/MI_DBA_Snow_Detailed.uasset",
        GAME / "Content/Development/Snow/Maps/L_DBA_SnowReview.umap",
    )
    for file in targets:
        assert file.is_file() and file.stat().st_size > 512, f"目标资产文件未落盘：{file}"
    assert "GamePlatformSurface" in plugins and "GamePlatformVFX" in plugins
    print("SNOW_3TIER_STATIC_PASS platform=Surface,Weather,VFX project=DBAPresentationPack_Core")
    print("SNOW_CLIENT_SERVER_COOK_RULES_PASS server_never_cook=3 snow_assets=8")
    print("SNOW_COOK_EXECUTION_REMAINS_UNVERIFIED")


if __name__ == "__main__":
    main()
