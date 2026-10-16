#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM水体通用源贴图UE5.8制作。
归平台表现层GamePlatformSurface，只允许Editor导入供客户端消费。
inspect默认只读，apply只新增缺失的Texture2D，verify仅复查；不覆盖第三方或人工资源。
不触碰游戏世界、权威涉水服务器逻辑、现有材质引用与服务器Cook。
"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "Docs/Implementation/Water/SourceTextures"
MANIFEST = SOURCE / "WaterTextureSourceManifest.json"
MOUNT = "/GamePlatformSurface/Textures/Water"
MODE = os.environ.get("SSLM_WATER_TEXTURE_MODE", "inspect").strip().lower()

def load_inputs():
    """先对4张真实原图校验完整性/像素尺寸/颜色空间/路径，再允许写包。"""
    if MODE not in ("inspect", "apply", "verify"):
        raise ValueError("非法水体导入模式")
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    items = data.get("items")
    if data.get("version") != 1 or not isinstance(items, list) or len(items) != 4:
        raise ValueError("水体源纹理应严格为V1四张")
    result, seen = [], set()
    for row in items:
        filename = str(row["file"])
        if not re.fullmatch(r"T_GP_Water_[A-Za-z0-9_]+_Source\.png", filename) or filename in seen:
            raise ValueError("重复或非法的水体源图身份：" + filename)
        seen.add(filename)
        src = (SOURCE / filename).resolve()
        if src.parent != SOURCE.resolve() or not src.is_file():
            raise FileNotFoundError("水体原图缺失：" + filename)
        content = src.read_bytes()
        if hashlib.sha256(content).hexdigest() != row["sha256"]:
            raise ValueError("原图SHA-256不匹配：" + filename)
        if len(content) < 24 or content[:8] != b"\x89PNG\r\n\x1a\n":
            raise ValueError("文件不是有效PNG：" + filename)
        if struct.unpack(">II", content[16:24]) != (1024, 1024):
            raise ValueError("原图不是1024×1024：" + filename)
        if (row["width"], row["height"]) != (1024, 1024) or row["format"] != "RGBA8" or row["sRGB"] is not False:
            raise ValueError("RGBA线性技术纹理合同失配：" + filename)
        kind = str(row["kind"])
        if not kind.startswith(("NormalMap", "FoamMask", "FlowMap", "Caustics")):
            raise ValueError("未知水体原图用途：" + kind)
        result.append((src, MOUNT + "/" + Path(filename).stem, kind.startswith("NormalMap")))
    return result

def require_unreal():
    """禁止普通Python伪造资产，保证引擎版本与唯一正式主工程准确匹配。"""
    try:
        import unreal
    except ImportError as exc:
        raise RuntimeError("仅UE5.8内嵌Python能够执行原生贴图导入") from exc
    current = str(unreal.Paths.get_project_file_path()).replace("\\", "/").lower()
    expected = str(ROOT / "Game/DivineBeastsArena.uproject").replace("\\", "/").lower()
    if current != expected or not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("工作区或UE5.8锁定版本不匹配")
    if unreal.AssetRegistryHelpers.get_asset_registry().is_loading_assets():
        raise RuntimeError("AssetRegistry仍在扫描，禁止资源落盘")
    return unreal

def main():
    specs = load_inputs()
    print("SSLM_WATER_TEXTURE_INPUTS_VERIFIED", len(specs))
    if MODE == "inspect":
        print("SSLM_WATER_TEXTURE_READ_ONLY", [x[1] for x in specs])
        return
    unreal = require_unreal()
    # UE命令行冷启动时AssetRegistry可能尚未索引内容插件中的真实文件。
    # 必须先扫描正式挂载点，避免把已有.uasset误认为不存在后尝试覆盖。
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([MOUNT], force_rescan=True)
    lib = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    created, preserved = [], []
    for source, target, normal in specs:
        if lib.does_asset_exist(target):
            obj = lib.load_asset(target)
            if not isinstance(obj, unreal.Texture2D):
                raise TypeError("既有资源不是Texture2D：" + target)
            preserved.append(target)
            continue
        if MODE == "verify":
            raise FileNotFoundError("尚缺UE Texture2D：" + target)
        # 文件存在但资源未被引擎识别时拒绝写入；不能通过ImportTask覆盖磁盘原资产。
        physical = ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Content/Textures/Water" / (target.rsplit("/", 1)[1] + ".uasset")
        if physical.exists():
            raise RuntimeError("现有.uasset磁盘文件未被UE识别，停止以保护原文件：" + str(physical))
        folder, name = target.rsplit("/", 1)
        if not lib.does_directory_exist(folder):
            lib.make_directory(folder)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", folder)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", False)
        tools.import_asset_tasks([task])
        if not task.get_editor_property("imported_object_paths"):
            raise RuntimeError("UE没有实际生成纹理：" + target)
        obj = lib.load_asset(target)
        if not isinstance(obj, unreal.Texture2D):
            raise TypeError("UE导入对象类别错误：" + target)
        obj.set_editor_property("srgb", False)
        obj.set_editor_property("compression_settings",
            unreal.TextureCompressionSettings.TC_NORMALMAP if normal else unreal.TextureCompressionSettings.TC_MASKS)
        if not lib.save_loaded_asset(obj, only_if_is_dirty=False):
            raise RuntimeError("UE原生资源保存失败：" + target)
        created.append(target)
        print("SSLM_WATER_TEXTURE_CREATED", target)
    for _, target, _ in specs:
        obj = lib.load_asset(target)
        if not isinstance(obj, unreal.Texture2D):
            raise RuntimeError("UE导入后回读失败：" + target)
    print("SSLM_WATER_TEXTURE_FINISHED", json.dumps({"created": created, "preserved": preserved,
        "total": len(specs), "shaderCookGpuVisual": "pending"}, ensure_ascii=False))

# UE PythonScriptCommandlet不保证设置__name__为__main__。
# 此文件仅作独立制作入口，直接执行一次，禁止只加载定义却输出虚假成功。
main()
