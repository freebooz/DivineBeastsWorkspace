#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟201项通用材质能力台账静态门禁。

本脚本只读检查Docs/Implementation/Materials的201项索引、唯一ID、14个类别、冻结合同、
P2首批29项归集、插件所有权与中文Markdown索引对照。默认不加载UE、不宣称资产验证成功。
--require-ue-evidence 仅用于真正UE扫描产物存在后的严格门禁；严禁用文本创建假资产证据。
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "Docs" / "Implementation" / "Materials"
REGISTRY = DOCS / "MaterialCapabilityRegistry_V1.json"
INDEX = DOCS / "MaterialFunctionLibraryIndex.md"
INVENTORY = DOCS / "MaterialAssetInventory.json"
UE_EVIDENCE = DOCS / "MaterialAssetInventory_UE58.json"
EXPECTED_CATEGORIES = (25, 16, 12, 14, 12, 12, 16, 12, 20, 14, 18, 10, 12, 8)
ALLOWED_MODES = {"ExistingContract", "NativeReuse", "MF", "ML", "Other"}
ALLOWED_OWNERS = {
    "GamePlatformSurface", "GamePlatformVFX", "GamePlatformUI",
    "GamePlatformPresentation", "GamePlatformSurfaceEditor",
    "DBAPresentationPack_Core",
}
EXPECTED_CONTRACTS = {
    "/GamePlatformSurface/MaterialFunctions/Masks/MF_GP_SlopeMask",
    "/GamePlatformSurface/MaterialFunctions/Masks/MF_GP_HeightMask",
    "/GamePlatformSurface/MaterialFunctions/Utility/MF_GP_WorldNoise",
    "/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_SnowLayer",
    "/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_MossLayer",
    "/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_WetnessLayer",
    "/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_PuddleLayer",
}
EXPECTED_MASTER_MPC = {
    "/GamePlatformSurface/Materials/M_GP_Surface_Master",
    "/GamePlatformSurface/Materials/M_GP_Surface_Lite",
    "/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal",
}


def audit(require_ue: bool) -> list[str]:
    """返回全部违例而非遇到第一个失败即吞掉后续错误。"""
    problems: list[str] = []
    dataset = json.loads(REGISTRY.read_text(encoding="utf-8"))
    stored = json.loads(INVENTORY.read_text(encoding="utf-8"))
    rows = dataset.get("items", [])
    categories = dataset.get("categorySummary", [])
    if dataset.get("candidateCount") != 201 or len(rows) != 201:
        problems.append("机器索引必须是201条；不得把文档内候选当已实现的MF数量")
    if [c.get("count") for c in categories] != list(EXPECTED_CATEGORIES):
        problems.append("14大类候选计数与计划不符")
    if len({r.get("CapabilityId") for r in rows}) != len(rows):
        problems.append("能力编号重复")
    core_count = 0
    group_counts: Counter[str] = Counter()
    contracts = set()
    for item in rows:
        ident = item.get("CapabilityId", "")
        if not re.fullmatch(r"MAT-\d{2}-\d{2}", ident):
            problems.append(f"能力ID不合规：{ident!r}")
        if not item.get("NameZH") or not item.get("NameEN"):
            problems.append(f"{ident} 中英文名称缺失")
        if item.get("ImplementationMode") not in ALLOWED_MODES:
            problems.append(f"{ident} 实现类型无效")
        if item.get("OwnerPlugin") not in ALLOWED_OWNERS:
            problems.append(f"{ident} 所有者无效：{item.get('OwnerPlugin')}")
        if item.get("OwnerLayer") == "DivineBeasts" and item.get("OwnerPlugin") != "DBAPresentationPack_Core":
            problems.append(f"{ident} 项目层所有者不符合既有内容包边界")
        if item.get("OwnerLayer") == "GamePlatform" and item.get("OwnerPlugin") == "DBAPresentationPack_Core":
            problems.append(f"{ident} 平台层不能引用项目表现包")
        if item.get("P2CoreCandidate") is True:
            core_count += 1
        if item.get("ContractAssetPath"):
            contracts.add(item["ContractAssetPath"])
            if item.get("ImplementationMode") != "ExistingContract":
                problems.append(f"{ident} 既有合同资源类型标记错误")
        if item.get("RuntimeAssetStatus") in {"已完成", "生产就绪", "通过"}:
            problems.append(f"{ident} 没有UE证据却标记实际资产通过")
        group_counts[item.get("Category", "")] += 1
    if core_count != 29:
        problems.append(f"P2首批候选必须恰好29项，实际{core_count}")
    if contracts != EXPECTED_CONTRACTS:
        problems.append("冻结的7个Surface MF合同路径不一致")
    if [(c.get("category"), c.get("count")) for c in categories] != [
        (c.get("category"), group_counts.get(c.get("category"))) for c in categories
    ]:
        problems.append("机器索引分类汇总与201条记录不一致")
    indexed = re.findall(r"\|\s*(MAT-\d{2}-\d{2})\s*\|", INDEX.read_text(encoding="utf-8"))
    if len(indexed) != 201 or set(indexed) != {r.get("CapabilityId") for r in rows}:
        problems.append("中文索引与201条机器数据不一致")
    contracts_from_inventory = {r["PackagePath"] for r in stored.get("contractEntries", [])}
    if contracts_from_inventory != EXPECTED_CONTRACTS | EXPECTED_MASTER_MPC:
        problems.append("10项稳定资产合同台账缺项、重名或发生身份漂移")

    if require_ue:
        if not UE_EVIDENCE.exists():
            problems.append("未找到真实UE编辑器只读扫描报告；严格验收拒绝成功")
        else:
            observed = json.loads(UE_EVIDENCE.read_text(encoding="utf-8"))
            if observed.get("mode") != "UEAssetRegistryReadOnly":
                problems.append("引擎扫描报告模式不正确")
            contracts_ue = observed.get("contracts", [])
            if len(contracts_ue) != 10 or not all(x.get("ClassMatches") for x in contracts_ue):
                problems.append("10项关键合同缺少真实引擎类别确认")
            # 只读扫描即使全命中，也不是Shader/Cook/视觉验收，故不输出生产通过。
    return problems


def main() -> int:
    """验证失败以非零退出码阻断CI，静态通过明确不意味着UE生产验收完成。"""
    parser = argparse.ArgumentParser(description="201项材质能力与索引静态一致性审查")
    parser.add_argument("--require-ue-evidence", action="store_true",
                        help="要求真实UE5.8扫描证据，不代表Shader/Cook/人工审核成功")
    args = parser.parse_args()
    try:
        problems = audit(args.require_ue_evidence)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("MATERIAL_LIBRARY_VALIDATION_ERROR", error, file=sys.stderr)
        return 2
    if problems:
        for problem in problems:
            print("MATERIAL_LIBRARY_FAIL", problem, file=sys.stderr)
        return 1
    print("MATERIAL_LIBRARY_STATIC_PASS：14类201项唯一索引、29项核心候选、7MF+2M+1MPC合同一致")
    print("注意：静态验证不代表UE资产、Shader、Cook或人工视觉通过")
    if args.require_ue_evidence:
        print("MATERIAL_LIBRARY_UE_REGISTRY_CHECK_PASS：仅表示十项关键资产已在真实UE注册表发现且类别正确")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
