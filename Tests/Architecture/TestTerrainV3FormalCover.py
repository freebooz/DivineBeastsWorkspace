#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V3.0正式新手村地形覆盖只读门禁；不冒充UE资产或人工验收。

验证已真实存在的V3 Texture2D、UE软资源装配来源、三层依赖、安全回退与服务端排除。
发布前仍须UE5.8编辑器十份资产独立冷重载、客户端MID及GPU画面/专服Cook证据。
"""
from __future__ import annotations
import json
import re
import argparse
import hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
SETTINGS=ROOT/"Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Settings/DivineBeastsVillageSnowSettings.cpp"
CLIENT=ROOT/"Game/Config/Custom/VillageDevelopmentClient/DefaultGame.ini"
SERVER=ROOT/"Build/Game/ServerPresentationExclusions.json"
UPROJECT=ROOT/"Game/DivineBeastsArena.uproject"
TOOL=ROOT/"Tools/Unreal/Terrain/ApplyTerrainV3ToFormalTexturesUE58.py"
BACKUP=ROOT/"Saved/ArtReview/TerrainColorV3FormalCover/Before_20261018/BackupManifest.json"

def check(require_native: bool = False):
    settings=SETTINGS.read_text(encoding="utf-8")
    client=CLIENT.read_text(encoding="utf-8")
    policy=json.loads(SERVER.read_text(encoding="utf-8"))
    uproject=json.loads(UPROJECT.read_text(encoding="utf-8"))
    assert len(policy["Profiles"])==3
    assert "/DBAPresentationPack_Core/Textures/Terrain/Production10/T_DBA_Terrain_Grass_BaseColor_V3.T_DBA_Terrain_Grass_BaseColor_V3" in settings
    for role in ["Normal","ORM"]:
        expected=f"/DBAWorldPack_Village/Terrain/Textures/Grass/T_DBA_Terrain_Grass_{role}.T_DBA_Terrain_Grass_{role}"
        assert expected in settings and expected in client, f"草地{role}匹配纹理配置错误"
    assert "GroundAlbedoTexture=/DBAPresentationPack_Core/Textures/Terrain/Production10/" in client
    assert '+DirectoriesToAlwaysCook=(Path="/DBAPresentationPack_Core/Textures/Terrain/Production10")' in client
    excludes={x["PackagePath"] for x in policy["CookDirectories"]}
    assert "/DBAWorldPack_Village/Development/Terrain" in excludes
    assert "/DBAPresentationPack_Core/Textures/Terrain/Production10" in excludes
    assert "/DBAPresentationPack_Core/Textures/Terrain/Grass" in excludes
    packs={x["Name"]:x for x in uproject["Plugins"]}
    assert "Server" not in packs["DBAPresentationPack_Core"]["TargetAllowList"]
    # DBAClient是客户端Target注入、未在uproject全局列出，真实边界以其模块Host Type为准。
    client_plugin=json.loads((ROOT/"Game/Plugins/DivineBeasts/DBAClient/DBAClient.uplugin").read_text(encoding="utf-8"))
    presentation=[m for m in client_plugin["Modules"] if m["Name"]=="DivineBeastsPresentationClient"]
    assert len(presentation)==1 and presentation[0]["Type"]=="ClientOnly"
    assert "Server" not in presentation[0].get("TargetAllowList", [])
    script=TOOL.read_text(encoding="utf-8")
    assert 'replace_existing", True' in script
    assert 'MODE == "apply"' in script and 'MODE == "verify"' in script
    assert "EditorAssetLibrary" in script and "save_loaded_asset" in script
    # 新Village共享母材质已经真实升级；Dirt修复必须接受其已审计的81节点SHA，
    # 但不能直接跳过执行前World/材质回退快照、实际UE证据或冷重载门禁。
    assert "SSLM_V3_FORMAL_ACCEPT_VERIFIED_BOMBING_BRIDGE_SHA" in script
    assert 'evidence.get("materialSha256") != actual_bridge_sha' in script
    assert 'not evidence.get("coldVerifyPassed")' in script
    assert "delete_asset(" not in script and "set_editor_property(\"landscape_material\"" not in script
    assert BACKUP.is_file(), "必须先备份12份原资产"
    backup=json.loads(BACKUP.read_text(encoding="utf-8-sig"))
    assert len(backup["files"])==12
    sources=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/Terrain/V3"
    assert len(list(sources.glob("*_BaseColor_V3_Source.png")))==10
    v3asset=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/Textures/Terrain/Production10"
    assert len(list(v3asset.glob("*_BaseColor_V3.uasset")))==10
    if require_native:
        manifest=ROOT/"Saved/ArtReview/TerrainColorV3FormalCover/AppliedManifest.json"
        assert manifest.is_file(), "还没有UE5.8完成全部十层正式纹理的真实清单"
        actual=json.loads(manifest.read_text(encoding="utf-8"))
        assert len(actual.get("rows",[]))==10 and actual.get("coldVerifyPassed") is True
        assert actual.get("humanReviewPassed") is False, "未经人工审批不得标为通过"
        assert len({r["layer"] for r in actual["rows"]})==10
        for item in actual["rows"]:
            name=Path(item["destination"])
            if not name.is_absolute():name=ROOT/name
            assert name.is_file(), "正式十层原生Texture2D资产文件丢失："+item["layer"]
            h=hashlib.sha256()
            with name.open("rb") as handle:
                for buf in iter(lambda:handle.read(1024*1024),b""):
                    h.update(buf)
            assert h.hexdigest()==item["actualSha256"], "正式十层原生资产不等于独立UE冷重载SHA："+item["layer"]
        print("SSLM_V3_FORMAL_NATIVE_TEN_TEXTURES_VERIFIED layers=10 cold_verify=true user_visual_approval=false")
    print("SSLM_V3_FORMAL_STATIC_PASS sources=10 v3_texture2d_files=10 rollback=12 client_mid_binding=true server_excluded=true")

if __name__=="__main__":
    parser=argparse.ArgumentParser(description="SSLM真实十层地形基础色与服务器隔离门禁")
    parser.add_argument("--require-native",action="store_true",help="要求十层真实UE重导入及跨进程冷重载证明")
    args=parser.parse_args()
    check(args.require_native)
