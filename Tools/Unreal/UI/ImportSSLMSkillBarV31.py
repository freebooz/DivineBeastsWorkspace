# -*- coding: utf-8 -*-
"""SSLM 技能栏 V3.1：人工审核后仅导入独立贴图到审核区，不修改正式 HUD 蓝图。"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import struct

import unreal

WORKSPACE = Path(__file__).resolve().parents[3]
DEFAULT_REVIEW_DIR = (
    WORKSPACE
    / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/"
      "DBAUIPack_Core/SourceArt/UI/SkillBarReview"
)
SOURCE = Path(os.environ.get("SSLM_SKILLBAR_REVIEW_DIR", DEFAULT_REVIEW_DIR))
MANIFEST = SOURCE / "04_审核记录与接入规范/SSLM_V31_ResourceManifest.json"
APPROVAL = SOURCE / "04_审核记录与接入规范/SSLM_V31_ApprovalRecord.json"
ASSETS_DIR = SOURCE / "01_独立RGBA纹理"
DESTINATION = "/DBAUIPack_Core/UI/Textures/SkillBarReview"


def read_json(path: Path) -> dict:
    if not path.is_file():
        raise RuntimeError("缺少审核文件：" + str(path))
    return json.loads(path.read_text(encoding="utf-8-sig"))


def validate_png(path: Path, width: int, height: int, expected_sha: str) -> None:
    # PNG 头格式检测：不依赖非标准的 PIL 扩展。
    header = path.read_bytes()[:26]
    if len(header) < 26 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise RuntimeError("PNG 格式或文件头无效：" + str(path))
    got_width, got_height = struct.unpack(">II", header[16:24])
    depth, color_type = header[24], header[25]
    if (got_width, got_height) != (width, height) or (depth, color_type) != (8, 6):
        raise RuntimeError("PNG 不是预期的8位RGBA或尺寸错误：" + str(path))
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest.lower() != expected_sha.lower():
        raise RuntimeError("PNG 哈希与审核样本不一致：" + path.name)


def require_approved() -> list[dict]:
    manifest = read_json(MANIFEST)
    approval = read_json(APPROVAL)
    items = manifest.get("assets", [])
    if manifest.get("schema_version") != "3.1" or not items:
        raise RuntimeError("资源台账不存在或版本不一致")
    if approval.get("schema_version") != "3.1":
        raise RuntimeError("审核记录版本错误")
    if approval.get("status") != "Approved" or not approval.get("allow_unreal_import", False):
        raise RuntimeError("纹理没有得到人工批准，导入已阻止")
    if not str(approval.get("reviewer", "")).strip():
        raise RuntimeError("缺少真实的人工审核人")
    approved = set(approval.get("approved_assets", []))
    expected = {i["file"] for i in items}
    if expected != approved or approval.get("rejected_assets") or approval.get("pending_assets"):
        raise RuntimeError("未逐张批准全部纹理，导入已阻止")
    if approval.get("allow_replace_production_blueprints", False):
        raise RuntimeError("审核导入与正式蓝图替换不能合并为一步")
    for item in items:
        path = ASSETS_DIR / item["file"]
        if not path.is_file():
            raise RuntimeError("缺少PNG：" + str(path))
        validate_png(path, *item["dimensions"], item["sha256"])
    return items


def set_property_if_supported(asset, prop: str, value) -> None:
    if value is None:
        return
    try:
        asset.set_editor_property(prop, value)
    except Exception as exc:
        unreal.log_warning("纹理属性不支持，需人工复核：%s -> %s (%s)" % (asset.get_name(), prop, exc))


def import_review_textures() -> None:
    items = require_approved()
    lib = unreal.EditorAssetLibrary
    # 提前发现目的路径已有资源，避免同名覆盖人工工作。
    for item in items:
        path = DESTINATION + "/" + Path(item["file"]).stem
        if lib.does_asset_exist(path):
            raise RuntimeError("审核目标目录已有资源，拒绝覆盖：" + path)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    tasks = []
    for item in items:
        task = unreal.AssetImportTask()
        task.filename = str(ASSETS_DIR / item["file"])
        task.destination_path = DESTINATION
        task.destination_name = Path(item["file"]).stem
        task.automated = True
        task.save = True
        task.replace_existing = False
        tasks.append(task)
    tools.import_asset_tasks(tasks)

    for item in items:
        asset_path = DESTINATION + "/" + Path(item["file"]).stem
        texture = lib.load_asset(asset_path)
        if not isinstance(texture, unreal.Texture2D):
            raise RuntimeError("导入后找不到 Texture2D（二维纹理）：" + asset_path)
        set_property_if_supported(
            texture, "lod_group", getattr(unreal.TextureGroup, "TEXTUREGROUP_UI", None)
        )
        set_property_if_supported(
            texture, "mip_gen_settings",
            getattr(unreal.TextureMipGenSettings, "TMGS_NO_MIPMAPS", None)
        )
        set_property_if_supported(
            texture, "compression_settings",
            getattr(unreal.TextureCompressionSettings, "TC_DEFAULT", None)
        )
        set_property_if_supported(texture, "srgb", True)
        set_property_if_supported(texture, "never_stream", True)
        set_property_if_supported(
            texture, "address_x", getattr(unreal.TextureAddress, "TA_CLAMP", None)
        )
        set_property_if_supported(
            texture, "address_y", getattr(unreal.TextureAddress, "TA_CLAMP", None)
        )
        if not lib.save_loaded_asset(texture):
            raise RuntimeError("导入纹理无法保存：" + asset_path)

    unreal.log("SSLM 技能栏 V3.1：人工审核通过的 %d 张纹理已导入审核目录。"
               "正式 HUD 蓝图未修改。" % len(items))


if __name__ == "__main__":
    import_review_textures()
