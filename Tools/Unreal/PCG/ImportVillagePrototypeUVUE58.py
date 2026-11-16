#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""通过真实UE5.8导入/重载带UV的Village技术PCG静态模型。

唯一资源目录 /DBAWorldPack_Village/Development/PCG/MeshesUV，受项目NeverCook保护。
仅消费Saved/Validation中的SHA绑定OBJ，不覆盖旧UV缺失的六份原型或正式世界资产。
模式由外部受控脚本设置；apply要求全部六项不存在，verify要求全部存在。
网格只供编辑器PCG技术验证：去除默认简单碰撞，绝不赋予NavMesh/服务器Gameplay权威。
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
STAGE = ROOT / "Saved/Validation/GamePlatformPCG/TechnicalUVStageV1"
STAGE_MANIFEST = STAGE / "TechnicalUVStageManifest.json"
ORIGINAL_MANIFEST = ROOT / (
    "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/"
    "SourceArt/PCG/TechnicalPrototype_v1/VillagePCGTechnicalSourceManifest_v1.json"
)
DEST = "/DBAWorldPack_Village/Development/PCG/MeshesUV"
MODE = os.getenv("SSLM_VILLAGE_UV_TECH_IMPORT_MODE", "inspect").strip().lower()
ROLES = ("ForestCanopy", "RockScatter", "AgriCrop", "FieldFence", "FarmGate", "WoodBridge")


def check_inputs() -> list[dict]:
    """检查完整性、阶段字节哈希、来源三角数及开发专用性，不接纳临时外部路径。"""
    stage = json.loads(STAGE_MANIFEST.read_text(encoding="utf-8"))
    original = json.loads(ORIGINAL_MANIFEST.read_text(encoding="utf-8"))
    if (stage.get("schemaVersion") != 1 or stage.get("cannotCook") is not True or
            stage.get("sourceScope") != "NonProductionTechnicalOnly"):
        raise ValueError("UV衍生物缺少开发资源NeverCook限制")
    rows = stage["files"]
    if len(rows) != 6 or {r["role"] for r in rows} != set(ROLES):
        raise ValueError("技术模型UV来源必须恰好6种")
    original_by_role = {r["role"]: r for r in original["meshSources"]}
    for row in rows:
        name = row["relativePath"]
        if name != "TP_DBA_PCG_" + row["role"] + ".obj" or row["role"] not in original_by_role:
            raise ValueError("技术源身份不匹配")
        if row["sourceSha256"] != original_by_role[row["role"]]["sha256"]:
            raise ValueError("UV来源与原始SourceArt哈希不符")
        staged = STAGE / name
        if not staged.is_file() or not staged.resolve().is_relative_to(STAGE.resolve()):
            raise ValueError("技术UV派生文件路径不合法")
        if hashlib.sha256(staged.read_bytes()).hexdigest() != row["stagedSha256"]:
            raise ValueError("UV派生源哈希不匹配")
    return rows


def main() -> None:
    if MODE not in ("inspect", "apply", "verify"):
        raise ValueError("仅支持inspect、apply或verify")
    print("SSLM_VILLAGE_UV_SCRIPT_START", MODE, flush=True)
    rows = check_inputs()
    if MODE == "inspect":
        print("SSLM_VILLAGE_UV_INPUTS_OK", len(rows))
        return

    import unreal as ue  # type: ignore
    if not str(ue.Paths.get_project_file_path()).replace("\\", "/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("无法识别唯一SSLM正式工程")
    if "-VillagePCGTechUV" not in ue.SystemLibrary.get_command_line():
        raise RuntimeError("必须由显式技术导入命令行授权执行")
    editor = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    if editor is None:
        raise RuntimeError("无法获得UE5.8 StaticMeshEditorSubsystem")
    assets = ue.EditorAssetLibrary
    tools = ue.AssetToolsHelpers.get_asset_tools()
    names = {row["role"]: DEST + "/SM_DBA_PCG_" + row["role"] + "_TechUV" for row in rows}
    present = {r: assets.does_asset_exist(p) for r, p in names.items()}
    if MODE == "apply" and any(present.values()):
        raise FileExistsError("已有目标技术资产，拒绝覆盖或继续半批导入：" + str(present))
    if MODE == "verify" and not all(present.values()):
        raise FileNotFoundError("六个技术UV资产不完整，禁止假装UE导入成功：" + str(present))
    original = json.loads(ORIGINAL_MANIFEST.read_text(encoding="utf-8"))
    source = {r["role"]: r for r in original["meshSources"]}
    imported = 0
    for row in rows:
        name = "SM_DBA_PCG_" + row["role"] + "_TechUV"
        destination = names[row["role"]]
        if MODE == "apply":
            if not assets.does_directory_exist(DEST):
                assets.make_directory(DEST)
            task = ue.AssetImportTask()
            task.set_editor_property("filename", str((STAGE / row["relativePath"]).resolve()))
            task.set_editor_property("destination_path", DEST)
            task.set_editor_property("destination_name", name)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", False)
            task.set_editor_property("save", False)
            tools.import_asset_tasks([task])
            if not task.get_editor_property("imported_object_paths"):
                raise RuntimeError("UE Interchange未创建真实UV模型：" + name)
        mesh = assets.load_asset(destination)
        if mesh is None or not isinstance(mesh, ue.StaticMesh) or mesh.get_num_lods() < 1:
            raise RuntimeError("UStaticMesh没有可用LOD0：" + destination)
        collisions = editor.get_simple_collision_count(mesh)
        if collisions < 0:
            raise RuntimeError("简单碰撞计数不可读取：" + name)
        if MODE == "apply" and collisions > 0:
            if not editor.remove_collisions(mesh):
                raise RuntimeError("未能移除技术样例的非授权碰撞：" + name)
            collisions = editor.get_simple_collision_count(mesh)
        if collisions != 0:
            raise RuntimeError("技术样例不允许游戏碰撞：" + name)
        expected_extent = max((axis[1] - axis[0]) * 0.5 for axis in source[row["role"]]["boundsCm"])
        bounds = mesh.get_bounds().box_extent
        actual_extent = max(bounds.x, bounds.y, bounds.z)
        if not expected_extent * 0.75 <= actual_extent <= expected_extent * 1.25:
            raise RuntimeError("UE导入后厘米尺寸不合法：" + name)
        if MODE == "apply" and not assets.save_loaded_asset(mesh, only_if_is_dirty=False):
            raise RuntimeError("UE资产保存失败：" + name)
        print("SSLM_VILLAGE_UV_MESH_OK", destination,
              "extentCm", actual_extent, "simpleCollision", collisions, flush=True)
        imported += 1
    print("SSLM_VILLAGE_UV_IMPORT_PASS", imported, flush=True)
    print("TECHNICAL_EDITOR_ONLY_NEVER_COOK_NOT_PRODUCTION_MESH", flush=True)


if __name__ == "__main__":
    main()
