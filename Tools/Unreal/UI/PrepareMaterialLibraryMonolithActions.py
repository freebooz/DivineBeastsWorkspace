#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 通用材质函数库：生成P6用户界面Monolith制作动作清单。

所属架构：GamePlatformUI（平台界面材质）唯一拥有通用MF算法；
第三层DBAUIPack_Core只消费平台函数创建项目专属主题和展示Widget。
本脚本不直接调用UE或Monolith，不生成虚假的.uasset或WBP文件，
只把7个已经登记的独立材质函数配方转换为可逐项交给Monolith MCP的请求及断点状态。
原生UI资产必须由当前UE5.8中实际运行的Monolith 0.23.0操作完成；
保存回读与原生Shader/Cook分别列入后续证据阶段。
安全策略：不使用clear_existing、delete/replace、重命名、全库清图或覆盖现有图。
普通Python允许inspect与export动作清单；用户要求最终统一编译，当前不执行它。
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[3]
RECIPES = ROOT / "Tools/Unreal/UI/MaterialLibraryUIMonolithRecipes_V1.json"
INDEX = ROOT / "Docs/Implementation/Materials/MaterialCapabilityRegistry_V1.json"
OUTPUT = ROOT / "Docs/Implementation/Materials/MaterialLibraryUIActionQueue_V1.json"
EXPECTED_UNAUTHORED = 7
OWNER = "GamePlatformUI"
MOUNT = "/GamePlatformUI/Materials/Functions/MF_GP_UI_"
ACTION_PREFIX = "P6-UI"


def load_entries() -> list[dict[str, Any]]:
    """检查现行台账来源与配方一一对应，防止生成无归属或重名的UI资产请求。"""
    registry = json.loads(INDEX.read_text(encoding="utf-8"))
    recipes = json.loads(RECIPES.read_text(encoding="utf-8"))
    if registry.get("candidateCount") != 201 or len(registry.get("items", [])) != 201:
        raise ValueError("必须先读取201项唯一能力台账，拒绝猜测或补填UI函数身份")
    table = {row["CapabilityId"]: row for row in registry["items"]}
    entries = recipes.get("items", [])
    if recipes.get("schemaVersion") != 1 or len(entries) != EXPECTED_UNAUTHORED:
        raise ValueError("P6待创UI配方必须恰有7项，不能把已创建的5项再次批量覆盖")
    if len({row["capabilityId"] for row in entries}) != len(entries):
        raise ValueError("UI函数制作能力编号重复")
    if len({row["assetPath"] for row in entries}) != len(entries):
        raise ValueError("UI函数建议资产包路径重复")

    for row in entries:
        cid = row["capabilityId"]
        known = table.get(cid)
        path = row["assetPath"]
        if not known or known["OwnerPlugin"] != OWNER or known["ImplementationMode"] != "MF":
            raise ValueError("P6配方违反三层插件归属：" + cid)
        if known["NameEN"] != row["name"] or path != MOUNT + row["name"]:
            raise ValueError("UI函数英文身份或包路径与台账合同不一致：" + cid)
        if known.get("ProposedAssetPath") != path:
            raise ValueError("UI函数引用路径未获201项主台账登记：" + cid)
        graph = row["graphSpec"]
        inputs = graph.get("inputs", [])
        outputs = graph.get("outputs", [])
        hsls = graph.get("custom_hlsl_nodes", [])
        connections = graph.get("connections", [])
        if not 1 <= len(inputs) <= 12 or not 1 <= len(outputs) <= 3 or len(hsls) != 1:
            raise ValueError("每份P6 UI函数必须具有独立输入、单输出和明确HLSL算法：" + cid)
        all_ids = {node.get("id") for node in inputs + outputs + hsls}
        if len(all_ids) != len(inputs) + len(outputs) + len(hsls) or None in all_ids:
            raise ValueError("UI材质函数节点ID重复或为空：" + cid)
        if len({item.get("name") for item in inputs}) != len(inputs):
            raise ValueError("UI材质函数输入名重复：" + cid)
        if not isinstance(hsls[0].get("code"), str) or "return" not in hsls[0]["code"]:
            raise ValueError("配方必须包含可审核的真实HLSL返回路径：" + cid)
        for connection in connections:
            if connection.get("from") not in all_ids or connection.get("to") not in all_ids:
                raise ValueError("UI图连接引用了不存在的节点：" + cid)
        if any(not any(x.get("to") == out["id"] for x in connections) for out in outputs):
            raise ValueError("UI函数存在没有真实数据来源的输出节点：" + cid)
        hlsl_additional = {x.get("name") for x in hsls[0].get("additional_outputs", [])}
        for out in outputs[1:]:
            if out.get("name") not in hlsl_additional:
                raise ValueError("额外UI函数输出必须声明真实HLSL额外输出：" + cid)
    return entries


def authoring_sequence(row: dict[str, Any]) -> list[dict[str, Any]]:
    """Monolith原生分步作者协议；每一动作成功与否必须由真实工具响应决定。"""
    path = row["assetPath"]
    cid = row["capabilityId"]
    description = row["descriptionZH"]
    # 执行方必须先通过Monolith material.get_function_info检查是否已存在；
    # 有已创建的同名函数仅准核对，不允许再次build_function_graph增加重复节点。
    requests = [
        ("preflight", "get_function_info", {"asset_path": path}),
        ("create_if_missing", "create_material_function", {
            "asset_path": path,
            "description": description,
            "expose_to_library": True,
            "library_categories": ["GamePlatform|UI|Reusable"],
            "type": "MaterialFunction",
        }),
        ("author_if_created", "build_function_graph", {
            "asset_path": path, "graph_spec": row["graphSpec"], "clear_existing": False,
        }),
        ("metadata_if_created", "set_function_metadata", {
            "asset_path": path, "description": description,
            "expose_to_library": True, "library_categories": ["GamePlatform|UI|Reusable"],
        }),
        # 仅做函数自有资源更新和保存；不在当前资源制作阶段批量编译审核母版。
        ("save_if_created", "save_material", {"asset_path": path, "only_if_dirty": True}),
        ("readback", "get_function_info", {"asset_path": path}),
    ]
    result = []
    for ordinal, (gate, action, params) in enumerate(requests, start=1):
        result.append({
            "sequenceId": f"{ACTION_PREFIX}-{cid}-{ordinal:02d}",
            "capabilityId": cid,
            "namespace": "material",
            "action": action,
            "parameters": params,
            "gate": gate,
            "status": "not_executed",
            "meaningZH": {
                "preflight": "先检查资产是否已经存在，禁止盲目重建",
                "create_if_missing": "仅在不存在时建立真实UMaterialFunction",
                "author_if_created": "仅首次创建时写真实表达式图，禁止覆盖旧图",
                "metadata_if_created": "仅对新函数设置平台分类与中文用途",
                "save_if_created": "保存原生材质函数资源",
                "readback": "重新读取函数输入、输出和节点，不代表Shader通过",
            }[gate],
        })
    return result


def build_queue() -> dict[str, Any]:
    """让每个Monolith动作具备唯一能力ID、前置条件和失败中止策略。"""
    records = load_entries()
    return {
        "schemaVersion": 1,
        "purposeZH": "SSLM通用材质库P6剩余7个UI函数，Monolith原生制作与逐项回读队列",
        "project": "DivineBeastsWorkspace",
        "ueProject": "Game/DivineBeastsArena.uproject",
        "owner": OWNER,
        "source": str(RECIPES.relative_to(ROOT)).replace("\\", "/"),
        "expectedFunctions": EXPECTED_UNAUTHORED,
        "enginePreconditionsZH": [
            "UE5.8编辑器真正运行且Monolith状态返回正式DivineBeastsArena主工程",
            "首次创建前AssetRegistry（资源注册表）不存在相同目标；若存在仅只读核对",
            "每一函数操作返回真实成功后才更新SourceStatus/RuntimeAssetStatus",
            "此资源阶段禁止统一Shader编译、PIE、Cook和人工视觉结论",
        ],
        "existingAssetPolicy": "skip_and_verify（存在则跳过写入，仅验证）",
        "abortPolicy": "fail_closed（失败立即停下；不自动清图、删除、重建或回滚人工资源）",
        "items": [
            {
                "capabilityId": row["capabilityId"],
                "name": row["name"],
                "nameZH": row["descriptionZH"],
                "packagePath": row["assetPath"],
                "state": "source_recipe_ready_native_asset_unverified",
                "operations": authoring_sequence(row),
            }
            for row in records
        ],
        "deferredZH": "母版MD_UI实际Shader和Widget蓝图编译等全部资源完成后统一验证",
    }


def main() -> None:
    """inspect只显示计划；export将可审核动作JSON写入文档目录，不写UE二进制文件。"""
    parser = argparse.ArgumentParser(description="P6平台UI通用函数Monolith动作队列生成器")
    parser.add_argument("--mode", choices=("inspect", "export"), default="inspect")
    opts = parser.parse_args()
    plan = build_queue()
    for row in plan["items"]:
        print("P6_UI_AUTHOR_SOURCE_PLAN", row["capabilityId"], row["packagePath"],
              "monolith_steps", len(row["operations"]))
    if opts.mode == "export":
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_text(json.dumps(plan, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print("P6_UI_ACTION_QUEUE_WRITTEN", OUTPUT)
    else:
        print("INSPECT_ONLY：不写本地文件，也不调用UE和Monolith")


if __name__ == "__main__":
    main()
