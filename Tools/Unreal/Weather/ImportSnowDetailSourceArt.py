#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8积雪细节PNG的真实Texture2D导入与校验。

所属：Tools/Unreal/Weather；平台资产归GamePlatformSurface或GamePlatformVFX。
调用：在锁定Game/DivineBeastsArena.uproject的Unreal Editor Python插件运行。
默认只读核验，环境变量 SNOW_ASSET_IMPORT_MODE=apply 才导入。
拒绝覆盖任何现有资产；不通过写文件伪造.uasset；专用服务器不使用本工具。
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = Path(__file__).with_name("SnowDetailSourceManifest.json")
MODE = os.getenv("SNOW_ASSET_IMPORT_MODE", "inspect").strip().lower()


def inventory() -> list[dict]:
    """先完整校验来源及所有目标身份，异常时不改变引擎状态。"""
    records = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if records.get("schemaVersion") != 1 or len(records.get("textures", [])) != 3:
        raise ValueError("积雪素材清单版本或数量不正确")
    outputs = []
    seen = set()
    for rec in records["textures"]:
        path = (ROOT / rec["sourcePath"]).resolve()
        asset = rec["assetPath"]
        if not path.is_relative_to(ROOT) or not path.is_file() or path.suffix.lower() != ".png":
            raise FileNotFoundError("积雪纹理源文件缺失或跨工作空间：" + str(path))
        if hashlib.sha256(path.read_bytes()).hexdigest() != rec["sha256"]:
            raise ValueError("积雪源图SHA256不符：" + str(path))
        if (not asset.startswith(("/GamePlatformSurface/Textures/Snow/",
                                  "/GamePlatformVFX/Weather/Textures/")) or
                asset in seen or not asset.rsplit("/", 1)[-1].isascii()):
            raise ValueError("非法或重复引擎资产地址：" + asset)
        if rec["recommendedCompression"] not in ("Masks", "Default"):
            raise ValueError("未知纹理压缩配置：" + asset)
        seen.add(asset)
        outputs.append({"source": path, "asset": asset, "spec": rec})
    return outputs


def create_assets(items: list[dict]) -> None:
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("请通过真实UE Editor执行，不能直接构造.uasset") from exc

    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    pending = []
    # 支持安全断点续作：已经导入的纹理只核对类别，不修改它们。
    for item in items:
        target = item["asset"]
        if assets.does_asset_exist(target):
            if not isinstance(assets.load_asset(target), unreal.Texture2D):
                raise TypeError("已有资源类型不是Texture2D：" + target)
            print("SNOW_TEXTURE_ALREADY_PRESENT", target)
            continue
        dest, name = target.rsplit("/", 1)
        if not assets.does_directory_exist(dest):
            assets.make_directory(dest)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(item["source"]))
        task.set_editor_property("destination_path", dest)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", False)
        pending.append((item, task))
    if pending:
        tools.import_asset_tasks([task for _, task in pending])
    compression = {
        "Masks": unreal.TextureCompressionSettings.TC_MASKS,
        "Default": unreal.TextureCompressionSettings.TC_DEFAULT,
    }
    for item, task in pending:
        if not list(task.get_editor_property("imported_object_paths")):
            raise RuntimeError("UE未创建Texture2D：" + item["asset"])
        texture = assets.load_asset(item["asset"])
        if not isinstance(texture, unreal.Texture2D):
            raise TypeError("新建资源不是Texture2D：" + item["asset"])
        texture.set_editor_property("srgb", bool(item["spec"]["sourceSRGB"]))
        texture.set_editor_property("compression_settings",
                                    compression[item["spec"]["recommendedCompression"]])
        if not assets.save_loaded_asset(texture, only_if_is_dirty=False):
            raise RuntimeError("UE纹理保存失败：" + item["asset"])
    for item in items:
        loaded = assets.load_asset(item["asset"])
        if not isinstance(loaded, unreal.Texture2D):
            raise TypeError("UE纹理回读失败：" + item["asset"])
        print("SNOW_TEXTURE_UE_VERIFIED", item["asset"])


def main() -> None:
    items = inventory()
    for item in items:
        print("SNOW_TEXTURE_PLAN", item["asset"], str(item["source"]))
    if MODE == "inspect":
        print("SNOW_TEXTURE_SOURCE_OK", len(items))
        return
    if MODE != "apply":
        raise ValueError("SNOW_ASSET_IMPORT_MODE仅允许inspect或apply")
    create_assets(items)


if __name__ == "__main__":
    main()
