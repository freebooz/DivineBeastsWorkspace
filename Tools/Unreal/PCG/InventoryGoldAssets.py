#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""金标准PCG（程序化生成）真实资源文件清单，只读检查与SHA-256审计。

注意：文件存在、非零长度和哈希稳定，不等于UE5.8 AssetRegistry、蓝图节点、
PCG执行结果、Gold G01～G16或Client/Server Cook通过。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[3]
ROOT = WORKSPACE / "Game/Content/Development/Foundation/PCG"
GROUPS = {
    "Definitions": (
        "Exec", "Priority", "MeshCanopy", "MeshCrop", "MeshFence",
        "PolicyForest", "PolicyCrop", "LayerCanopy", "LayerFloor", "LayerCrop",
        "Biome", "Exclusion", "Road", "Enclosure", "Parcel", "Crop", "Connector",
    ),
    "Realized": ("Canopy", "Rock", "Crop"),
    "Profiles": ("Canopy", "Rock", "Crop"),
}
MAP_NAME = "PCG_GoldLevel_M1.umap"


def audit() -> dict:
    result = {"schema_version": 1,
              "scope": "真实UE文件存在性/完整性，不证明引擎内执行或Cook",
              "passed": True, "groups": {}, "map": {}}
    for group, labels in GROUPS.items():
        expected = {
            ("DA_PCGGold_" if group != "Realized" else "PCG_Gold_")
            + name + ".uasset" for name in labels
        }
        folder = ROOT / group
        found = {p.name for p in folder.glob("*.uasset")} if folder.exists() else set()
        missing = sorted(expected - found)
        # Profiles可能包含其它合法PCG配置：仅检查本任务自己的DA_PCGGold_前缀。
        owned = found if group != "Profiles" else {f for f in found if f.startswith("DA_PCGGold_")}
        unknown = sorted(owned - expected)
        entries = []
        for name in sorted(found & expected):
            source = folder / name
            entries.append({
                "path": str(source.relative_to(WORKSPACE)).replace("\\", "/"),
                "length_bytes": source.stat().st_size,
                "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            })
        passed = not missing and not unknown and len(entries) == len(labels) and all(
            item["length_bytes"] > 0 for item in entries
        )
        result["groups"][group] = {
            "expected": len(labels), "found": len(entries),
            "missing": missing, "unexpected_owned": unknown,
            "files": entries, "passed": passed,
        }
        result["passed"] &= passed
    map_path = ROOT / "Validation" / MAP_NAME
    exists = map_path.is_file()
    result["map"] = {
        "path": str(map_path.relative_to(WORKSPACE)).replace("\\", "/"),
        "exists": exists,
        "length_bytes": map_path.stat().st_size if exists else 0,
        "sha256": hashlib.sha256(map_path.read_bytes()).hexdigest() if exists else None,
    }
    result["passed"] &= exists and result["map"]["length_bytes"] > 0
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="只读检查金标准PCG的24份UE资产文件")
    parser.add_argument("--report", type=Path, help="输出审计报告，必须位于Saved/Validation内")
    options = parser.parse_args()
    result = audit()
    for name, group in result["groups"].items():
        print(f"{name}: {group['found']}/{group['expected']}，完整性={'通过' if group['passed'] else '不通过'}")
        if group["missing"] or group["unexpected_owned"]:
            print("  缺少=", group["missing"], "未知同属资产=", group["unexpected_owned"])
    print("GoldLevelMap:", "已存在" if result["map"]["exists"] else "缺失")
    if options.report:
        destination = options.report.resolve()
        approved = (WORKSPACE / "Saved/Validation").resolve()
        if not destination.is_relative_to(approved):
            raise ValueError("仅允许在Saved/Validation中写入报告")
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
        print("REPORT", destination)
    print("GOLD_RESOURCE_FILE_AUDIT", "PASS" if result["passed"] else "FAIL")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
