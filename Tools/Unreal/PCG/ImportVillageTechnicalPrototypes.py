#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""真实UE5.8导入Village PCG六类技术网格（不属于正式3A资产）。

仅操作 /DBAWorldPack_Village/Development/PCG/Meshes（打包排除目录）；
禁止在项目已保存正式PCG网格/材质/地图的路径覆盖已有资产。
OBJ源必须有固定SHA-256 Manifest，执行前须是真正的DivineBeastsArena.uproject。
模式inspect默认只列计划，apply经UE Editor导入真实UStaticMesh并回读；
verify只校验已经导入的六项资源。任何源/尺寸/输出类型不符均失败关闭。
导入成功也不代表PCG实际生成、Gameplay权威、碰撞NavMesh或AAA视觉验收完成。
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[3]
SOURCE = WORKSPACE / (
    "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/"
    "SourceArt/PCG/TechnicalPrototype_v1"
)
MANIFEST = SOURCE / "VillagePCGTechnicalSourceManifest_v1.json"
DEST = "/DBAWorldPack_Village/Development/PCG/Meshes"
MODE = os.environ.get("VILLAGE_PCG_MESH_IMPORT_MODE", "inspect").lower().strip()

EXPECTED = (
    "ForestCanopy", "RockScatter", "AgriCrop", "FieldFence", "FarmGate", "WoodBridge"
)


def sha(file: Path) -> str:
    return hashlib.sha256(file.read_bytes()).hexdigest()


def manifest():
    if not MANIFEST.is_file():
        raise FileNotFoundError("技术模型真实源清单缺失：" + str(MANIFEST))
    doc = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if (doc.get("schemaVersion") != 1 or
            doc.get("sourceKind") != "procedural_technical_prototype_not_approved" or
            doc.get("units") != "centimeters_Z_up" or
            doc.get("humanArtApproval") != "pending" or
            doc.get("unrealAssetCreated") is not False):
        raise ValueError("技术样本不能冒充正式美术批准资产或已导入UE的成果")
    rows = doc.get("meshSources")
    if not isinstance(rows, list) or {x.get("role") for x in rows} != set(EXPECTED):
        raise ValueError("六种技术原型网格清单不完整或出现重复")
    for row in rows:
        source = (SOURCE / row["relativePath"]).resolve()
        if not source.is_relative_to(SOURCE.resolve()) or not source.is_file() or sha(source) != row["sha256"]:
            raise ValueError("技术源OBJ缺失或SHA-256不符：" + str(row.get("relativePath")))
        if row["triangles"] < 20 or any(a[1] < a[0] for a in row["boundsCm"]):
            raise ValueError("模型三角数或厘米坐标范围非法：" + row["role"])
    mtl = doc["material"]
    material = (SOURCE / mtl["relativePath"]).resolve()
    if (not material.is_relative_to(SOURCE.resolve()) or not material.is_file()
            or sha(material) != mtl["sha256"]):
        raise ValueError("非发行技术材料源丢失或SHA错误")
    return rows


def main():
    if MODE not in ("inspect", "apply", "verify"):
        raise ValueError("模式只能为inspect、apply或verify")
    rows = manifest()
    for row in rows:
        print("VILLAGE_PCG_TECH_MESH_PLAN", row["role"], row["triangles"],
              row["sha256"], DEST + "/SM_DBA_PCG_" + row["role"] + "_Tech")
    if MODE == "inspect":
        print("TECHNICAL_MESH_INSPECT_NO_UE_ASSETS_WRITTEN")
        return
    try:
        import unreal  # type: ignore
    except ImportError as e:
        raise RuntimeError("必须由真实UE5.8编辑器导入OBJ，不能在普通Python伪造.uasset") from e
    project_path = unreal.Paths.get_project_file_path().replace("\\", "/")
    if not project_path.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("UE编辑器不是神兽联盟唯一正式工程")
    if "-VillagePCGTechMesh" not in unreal.SystemLibrary.get_command_line():
        raise RuntimeError("缺少本任务明确命令行标志-VillagePCGTechMesh，拒绝操作编辑器资产")

    library = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mesh_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if mesh_editor is None:
        raise RuntimeError("锁定UE5.8未能创建StaticMeshEditorSubsystem，无法强制剥离技术模型碰撞")
    expected_targets = {DEST + "/SM_DBA_PCG_" + row["role"] + "_Tech" for row in rows}
    if MODE == "apply" and any(library.does_asset_exist(path) for path in expected_targets):
        raise FileExistsError("任一技术静态网格已存在，拒绝半批创建或覆盖")
    if MODE == "verify" and any(not library.does_asset_exist(path) for path in expected_targets):
        raise FileNotFoundError("没有找到完整六项真实UE StaticMesh（必须先正式导入）")

    for row in rows:
        name = "SM_DBA_PCG_" + row["role"] + "_Tech"
        target = DEST + "/" + name
        if MODE == "apply":
            if not library.does_directory_exist(DEST):
                library.make_directory(DEST)
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", str((SOURCE / row["relativePath"]).resolve()))
            task.set_editor_property("destination_path", DEST)
            task.set_editor_property("destination_name", name)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", False)
            task.set_editor_property("save", False)
            tools.import_asset_tasks([task])
            paths = list(task.get_editor_property("imported_object_paths"))
            if not paths:
                raise RuntimeError("Interchange OBJ导入没有产生UObject路径：" + name)
        mesh = library.load_asset(target)
        if mesh is None or not isinstance(mesh, unreal.StaticMesh):
            raise TypeError("UE导入结果不是原生UStaticMesh：" + target)
        if not mesh.get_num_lods() >= 1:
            raise RuntimeError("StaticMesh没有LOD0几何：" + name)
        # 技术样例不允许直接提供服务端阻挡/门洞/桥面权威碰撞；即使源领域叫StaticWorld也仅供预览。
        # OBJ默认导入器有时生成整体碰撞盒，若保留会堵住门洞或桥头并污染导航，因此必须清理。
        collision_count = mesh_editor.get_simple_collision_count(mesh)
        if collision_count < 0:
            raise RuntimeError("UE StaticMesh简单碰撞数量读取失败：" + name)
        if MODE == "apply" and collision_count > 0:
            if not mesh_editor.remove_collisions(mesh):
                raise RuntimeError("UE技术网格默认碰撞无法安全移除：" + name)
            collision_count = mesh_editor.get_simple_collision_count(mesh)
        if collision_count != 0:
            raise RuntimeError("技术原型必须不带SimpleCollision，静态碰撞需要另行Gameplay审核：" + name)

        # UE5.8 OBJ导入器是否将厘米源额外放大100倍由真实Bounds判定，不用默认设置推断。
        size = mesh.get_bounds().box_extent
        source_bounds = row["boundsCm"]
        expected = max((axis[1] - axis[0]) / 2 for axis in source_bounds)
        actual = max(size.x, size.y, size.z)
        if actual < expected * .75 or actual > expected * 1.25:
            raise RuntimeError(
                "技术源模型厘米单位/朝向导入错误；不能保存不符合PCG尺寸合同的模型："
                + name + " actual=" + str(actual) + " expected=" + str(expected))
        if MODE == "apply" and not library.save_loaded_asset(mesh, only_if_is_dirty=False):
            raise RuntimeError("UE未能真实保存技术静态网格：" + target)
        print("UE_VILLAGE_PCG_TECH_MESH_ASSET_OK", target,
              "trianglesSource", row["triangles"], "boundsMaxExtentCm", actual,
              "simpleCollisions", collision_count, "gameplayCollision", "not_approved")
    print("UE_VILLAGE_PCG_TECH_MESHES_REAL_IMPORT_OR_VERIFY_SUCCESS", len(rows))
    print("NOT_PRODUCTION_AAA_ART_NOT_SERVER_BAKE_NO_GAEA_INPUT")


if __name__ == "__main__":
    main()
