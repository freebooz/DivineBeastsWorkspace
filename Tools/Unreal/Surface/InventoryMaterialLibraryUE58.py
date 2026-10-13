#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 通用材质库只读资产清查。

职责：在正确的神兽联盟编辑器内，通过资产注册表读取材质相关UE资产和已声明的稳定合同；
只把审计证据写入Docs/Implementation/Materials，不生成/覆盖任何.uasset和.umap。
运行模式：inspect（默认，仅检查需求与路径）；scan（仅限真实UE编辑器，写实际审计报告）。
不负责：编辑材质图、重新保存项目资源、编译Shader或人工美术验收。
"""
from __future__ import annotations

import argparse
import datetime
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
DOCS = ROOT / "Docs" / "Implementation" / "Materials"
INVENTORY = DOCS / "MaterialAssetInventory.json"
OUTPUT = DOCS / "MaterialAssetInventory_UE58.json"
REGISTRY = DOCS / "MaterialCapabilityRegistry_V1.json"

SCOPES = (
    "/GamePlatformSurface",
    "/GamePlatformVFX",
    "/GamePlatformUI",
    "/DBAPresentationPack_Core",
    "/DBAUIPack_Core",
    "/DBAWorldPack_Village",
    "/Game",
)
CLASSES = {
    "Material", "MaterialFunction", "MaterialFunctionInstance",
    "MaterialInstance", "MaterialInstanceConstant", "MaterialLayer",
    "MaterialLayerBlend", "MaterialParameterCollection", "Texture2D",
    "RuntimeVirtualTexture", "NiagaraSystem", "NiagaraEmitter",
}
MAX_REFERENCERS_PER_CONTRACT = 50


def read_inputs() -> tuple[dict, dict]:
    """读取脚本控制范围：正式合同与候选能力，绝不根据建议路径断言资源存在。"""
    inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
    registry = json.loads(REGISTRY.read_text(encoding="utf-8"))
    if registry.get("candidateCount") != 201 or len(registry.get("items", ())) != 201:
        raise ValueError("201项能力索引未闭环，拒绝产生表面真实资产报告")
    if len(inventory.get("contractEntries", ())) != 10:
        raise ValueError("关键资产合同必须是7材质函数+2母版+1参数集合")
    return inventory, registry


def asset_type(asset_data) -> str:
    """兼容引擎版本的资产类别字段；字段不可解析时明确返回未知而不猜测。"""
    try:
        path = asset_data.get_editor_property("asset_class_path")
        return str(path.asset_name)
    except (AttributeError, RuntimeError, TypeError):
        try:
            return str(asset_data.get_editor_property("asset_class"))
        except (AttributeError, RuntimeError, TypeError):
            return "Unknown（UE资产类别解析失败）"


def asset_package(asset_data) -> str:
    """包名称需取资源真实注册表身份，不能通过文件扩展名推断。"""
    return str(asset_data.get_editor_property("package_name"))


def editor_scan() -> dict:
    """读取真实编辑器资产注册表；结果仅证明注册/类型/有限图检查，不证明渲染正确。"""
    import unreal  # type: ignore[import-not-found]

    active_project = str(unreal.Paths.get_project_file_path()).replace("\\", "/")
    if not active_project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("拒绝审计其他UE工程：" + active_project)

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(list(SCOPES), force_rescan=True)

    records: dict[str, dict] = {}
    for scope in SCOPES:
        if not unreal.EditorAssetLibrary.does_directory_exist(scope):
            continue
        try:
            matched = registry.get_assets_by_path(unreal.Name(scope), recursive=True)
        except TypeError:
            matched = registry.get_assets_by_path(unreal.Name(scope), True)
        for item in matched:
            klass = asset_type(item)
            if klass not in CLASSES:
                continue
            package = asset_package(item)
            if package in records:
                continue
            records[package] = {
                "PackagePath": package,
                "NativeClass": klass,
                "PrimaryOwnerScope": scope,
                "AssetRegistryPresent": True,
                "ShaderCompileStatus": "未验证",
                "VisualReviewStatus": "待人工验收",
            }

    inventory, _ = read_inputs()
    contracts = []
    for expected in inventory["contractEntries"]:
        path = expected["PackagePath"]
        asset_record = records.get(path)
        # 只对十个已冻结资源做定向Load，并检查实际类别，避免海量资产强制加载。
        loaded = unreal.EditorAssetLibrary.load_asset(path) if asset_record else None
        actual_class = loaded.get_class().get_name() if loaded else None
        graph_nodes = None
        if loaded and actual_class == "Material":
            try:
                graph_nodes = unreal.MaterialEditingLibrary.get_num_material_expressions(loaded)
            except Exception as error:
                graph_nodes = "检查失败：" + str(error)
        elif loaded and actual_class == "MaterialFunction":
            try:
                graph_nodes = unreal.MaterialEditingLibrary.get_num_material_expressions_in_function(loaded)
            except Exception as error:
                graph_nodes = "检查失败：" + str(error)

        referencers = []
        if loaded:
            try:
                refs = unreal.EditorAssetLibrary.find_package_referencers_for_asset(path, load_assets_to_confirm=False)
                referencers = [str(p) for p in refs[:MAX_REFERENCERS_PER_CONTRACT]]
            except Exception as error:
                referencers = ["引用扫描失败：" + str(error)]

        contracts.append({
            "PackagePath": path,
            "NameZH": expected["NameZH"],
            "ExpectedClass": expected["AssetClass"],
            "AssetExists": bool(loaded),
            "ObservedClass": actual_class,
            "ClassMatches": actual_class == expected["AssetClass"] if loaded else False,
            "MaterialGraphNodeCount": graph_nodes,
            "ReferencersSample": referencers,
            "ShaderCompileStatus": "未执行，不能由节点统计推断通过",
            "SavedAndReloaded": "未执行，当前为只读扫描",
            "VisualReviewStatus": "待人工验收",
        })

    return {
        "schemaVersion": 1,
        "mode": "UEAssetRegistryReadOnly",
        "dateUTC": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "editorProject": active_project,
        "scopes": list(SCOPES),
        "trackedClassNames": sorted(CLASSES),
        "totalMatchingAssets": len(records),
        "assets": [records[p] for p in sorted(records)],
        "contracts": contracts,
        "shaderAndCookNotInferred": True,
        "noteZH": "本报告仅证明实际编辑器资产注册信息、少量真实类型和材质节点数量，不代表Shader、打包或视觉验收。",
    }


def main() -> None:
    """inspect无任何写盘副作用；scan只写文档审计报告并防止写入项目资源。"""
    args = argparse.ArgumentParser(description="UE5.8平台通用材质只读资产审查（中文）")
    args.add_argument("--mode", choices=("inspect", "scan"),
                      default=os.environ.get("MATERIAL_LIBRARY_INVENTORY_MODE", "inspect"))
    opts = args.parse_args()
    inventory, capabilities = read_inputs()
    print("MATERIAL_LIBRARY_PROJECT", ROOT)
    print("MATERIAL_LIBRARY_CANDIDATES", len(capabilities["items"]))
    print("MATERIAL_LIBRARY_STABLE_CONTRACTS", len(inventory["contractEntries"]))
    if opts.mode == "inspect":
        print("INSPECT_ONLY：不载入引擎、不生成.uasset、不写证据。")
        for item in inventory["contractEntries"]:
            print("CONTRACT", item["AssetClass"], item["PackagePath"])
        return
    try:
        report = editor_scan()
    except ImportError as error:
        raise RuntimeError("UE扫描必须在虚幻编辑器PythonScriptPlugin中运行；当前Python不能代替UE") from error

    DOCS.mkdir(parents=True, exist_ok=True)
    temporary = OUTPUT.with_suffix(".tmp")
    temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(OUTPUT)
    print("UE_ASSET_INVENTORY_SAVED", OUTPUT)
    print("ASSET_COUNT", report["totalMatchingAssets"])
    print("CONTRACT_FOUND", sum(x["AssetExists"] for x in report["contracts"]))


if __name__ == "__main__":
    main()
