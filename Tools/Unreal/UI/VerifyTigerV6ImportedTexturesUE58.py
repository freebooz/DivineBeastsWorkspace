# -*- coding: utf-8 -*-
"""
SSLM 寅虎 V6 技能图标 UE5.8 独立冷重载核验（只读）。

职责：用新的 UnrealEditor-Cmd 进程核实八个来自 Monolith MCP 的真实 Texture2D，
核对稳定技能身份、所选PNG SHA、导入包路径、48像素尺寸、色彩/压缩/UI组及纹理属性。
归属：DivineBeasts 项目层的审计工具；不创建/修改/保存任何UObject，
不写 G A S 授权、UI Profile 或图标；不作为新资产的作者接口。
退出：异常时通过 -ScriptErrorsAreFatal 返回失败，成功输出唯一完成标记；
UE冷重载是资产层确认，并不意味着真实HUD场景/客户端Cook或美术人工批准。
"""
import hashlib
import json
import os
import struct

import unreal


def require(condition, explanation):
    """任一不合约项均拒绝成功，避免用文件存在冒充引擎验证。"""
    if not condition:
        raise RuntimeError("SSLM_TIGER_UE_COLD_VERIFY_FAILED: " + explanation)


def verify():
    """只读校验所有8个实际原生纹理，逐项构造包含来源哈希的运行证据。"""
    project_dir = os.path.normpath(unreal.Paths.project_dir())
    project_root = os.path.normpath(os.path.join(project_dir, ".."))
    project_name = os.path.basename(os.path.normpath(project_dir))
    engine_version = unreal.SystemLibrary.get_engine_version()
    require(project_name.lower() == "game", "工程目录不是 Game")
    require("5.8" in engine_version, "引擎版本不是预期UE5.8：" + engine_version)

    queue_path = os.path.join(
        project_root,
        "Docs", "Implementation", "GamePlatformAbilitySystem",
        "TigerV6MonolithImportQueue_20261018.json"
    )
    with open(queue_path, "r", encoding="utf-8-sig") as stream:
        queue = json.load(stream)
    require(queue.get("heroId") == "Hero.Zodiac.Tiger", "角色身份不一致")
    entries = queue.get("items", [])
    require(len(entries) == 8, "实际队列必须有8个独立技能")
    expected_slots = ("Primary", "Slot1", "Slot2", "Slot4",
                      "Slot3", "Slot5", "Slot6", "Slot7")
    records = []
    native_hashes = set()
    source_hashes = set()

    for i, entry in enumerate(entries):
        index = i + 1
        name = entry["assetName"]
        require(entry["order"] == index, "技能顺序不连续")
        require(entry["slotId"] == expected_slots[i], "输入槽位不正确：" + name)
        require(name.startswith("T_DBA_Tiger_S{:02d}_".format(index))
                and name.endswith("_V6"), "资源名称与槽号不一致：" + name)

        png_file = os.path.join(project_root,
                                os.path.normpath(entry["sourcePng48"]))
        require(os.path.isfile(png_file), "48像素PNG不存在：" + png_file)
        with open(png_file, "rb") as stream:
            raw = stream.read()
        require(len(raw) >= 33 and raw[:8] == b"\x89PNG\r\n\x1a\n",
                "PNG文件签名错误：" + name)
        width, height = struct.unpack(">II", raw[16:24])
        require((width, height) == (48, 48), "PNG不是48×48：" + name)
        source_sha = hashlib.sha256(raw).hexdigest()
        require(source_sha == entry["sourcePngSha256"],
                "原始PNG来源SHA不匹配：" + name)
        require(source_sha not in source_hashes, "重复的图像内容：" + name)
        source_hashes.add(source_sha)

        asset_path = "/DBAHeroPack_Tiger/UI/Abilities/" + name
        require(entry["targetAssetPath"] == asset_path + "." + name,
                "清单中的软对象路径错误：" + name)
        require(unreal.EditorAssetLibrary.does_asset_exist(asset_path),
                "引擎挂载点没有原生资产：" + name)
        texture = unreal.EditorAssetLibrary.load_asset(asset_path)
        require(texture is not None
                and texture.get_class().get_name() == "Texture2D",
                "资源不存在或并非Texture2D：" + name)
        require(texture.blueprint_get_size_x() == 48
                and texture.blueprint_get_size_y() == 48,
                "引擎实际贴图尺寸错误：" + name)
        expected_props = {
            "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
            "srgb": True,
            "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
            "never_stream": True,
            "filter": unreal.TextureFilter.TF_BILINEAR,
            "address_x": unreal.TextureAddress.TA_CLAMP,
            "address_y": unreal.TextureAddress.TA_CLAMP,
        }
        for prop, expect in expected_props.items():
            actual = texture.get_editor_property(prop)
            require(actual == expect,
                    name + " 的" + prop + "属性与SSLM UI标准不同："
                    + str(actual))

        package_file = os.path.join(
            project_root, "Game", "Plugins", "DivineBeasts",
            "ContentPacks", "Heroes", "DBAHeroPack_Tiger",
            "Content", "UI", "Abilities", name + ".uasset"
        )
        require(os.path.isfile(package_file), "原生.uasset文件不存在：" + name)
        with open(package_file, "rb") as stream:
            asset_sha = hashlib.sha256(stream.read()).hexdigest()
        require(asset_sha not in native_hashes, "原生资产字节重复：" + name)
        native_hashes.add(asset_sha)
        records.append({
            "order": index,
            "abilityId": entry["abilityId"],
            "slotId": entry["slotId"],
            "asset": asset_path,
            "size": [48, 48],
            "pngSha256": source_sha,
            "nativeAssetSha256": asset_sha,
            "settingsReadback": True,
            "freshEngineProcess": True,
        })

    # 只输出审计数据；由调用脚本按结束码、标记和8项记录共同判断是否可宣称通过。
    print("SSLM_TIGER_V6_NATIVE_EIGHT_TEXTURES_COLD_VERIFY_PASS")
    print("SSLM_TIGER_V6_COLD_VERIFY_JSON="
          + json.dumps({
              "heroId": "Hero.Zodiac.Tiger",
              "engineVersion": engine_version,
              "projectDir": project_dir,
              "count": len(records),
              "records": records,
          }, ensure_ascii=False))


if __name__ == "__main__":
    verify()
