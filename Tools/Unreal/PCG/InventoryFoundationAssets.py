#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PCG基础资源只读清单与SHA-256基线。

不读取/修改UE二进制内部结构，不伪称反射、蓝图逻辑、Cook验证通过。
对12模板、7子图、11项目蓝图建立路径、长度与哈希，防止增量工具误覆盖既有资产。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
FOUNDATION = ROOT / "Game/Content/Development/Foundation/PCG"
VILLAGE = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/PCG/Blueprints"
EXPECTED = {
    "Templates": (
        "TPL_Base", "TPL_ScatterSurface", "TPL_BiomeGenerator", "TPL_LinearDresser",
        "TPL_Enclosure", "TPL_EnclosureClosed", "TPL_Connector", "TPL_GateInsert",
        "TPL_ParcelFill", "TPL_CropField", "TPL_AssemblySpawn", "TPL_InterfaceBand",
    ),
    "Subgraphs": (
        "SG_ProjectOnLandscape", "SG_PriorityCarve", "SG_ApplySpawnPolicy",
        "SG_AssignMeshSet", "SG_FitPostsToSpline", "SG_BreakByIntersection",
        "SG_WriteClosedExclude",
    ),
    "VillageBlueprints": (
        "BP_PCG_Forest", "BP_PCG_Rock", "BP_PCG_Road", "BP_PCG_Field", "BP_PCG_Crops",
        "BP_PCG_Bridge", "BP_PCG_Exclusion", "BP_PCG_WaterBank",
        "BP_PCG_Building", "BP_PCG_Resource", "BP_PCG_Spawn",
    ),
}


def inventory() -> dict:
    report = {"schema_version": 1, "created_utc": datetime.now(timezone.utc).isoformat(),
              "purpose": "文件完整性，不代表UE资产反射/执行验收",
              "groups": {}, "passed": True}
    for group, names in EXPECTED.items():
        folder = VILLAGE if group == "VillageBlueprints" else FOUNDATION / group
        expected_names = {n + ".uasset" for n in names}
        existing = {f.name for f in folder.glob("*.uasset")} if folder.exists() else set()
        missing = sorted(expected_names - existing)
        unexpected = sorted(existing - expected_names)
        files = []
        for name in sorted(expected_names & existing):
            path = folder / name
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            files.append({"path": str(path.relative_to(ROOT)).replace("\\", "/"),
                          "bytes": path.stat().st_size, "sha256": digest})
        group_ok = not missing and not unexpected and all(item["bytes"] > 0 for item in files)
        report["groups"][group] = {"expected": len(expected_names), "actual": len(existing),
                                    "missing": missing, "unexpected": unexpected,
                                    "passed": group_ok, "files": files}
        report["passed"] &= group_ok
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description="检查PCG30份真实资产文件完整性")
    parser.add_argument("--report", type=Path, help="可选报告路径，建议Saved/Validation而非正式Content")
    args = parser.parse_args()
    data = inventory()
    for group, part in data["groups"].items():
        print(f"{group}: {part['actual']}/{part['expected']} 项；完整性={'通过' if part['passed'] else '失败'}")
        if part["missing"] or part["unexpected"]:
            print("  缺少：", part["missing"], "额外：", part["unexpected"])
    if args.report:
        output = args.report.resolve()
        if not output.is_relative_to((ROOT / "Saved/Validation").resolve()):
            raise ValueError("报告只能写入Saved/Validation审计目录，禁止污染Game内容包")
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
        print("PCG_ASSET_INVENTORY_REPORT", output)
    print("PCG_ASSET_FILE_INTEGRITY", "PASS" if data["passed"] else "FAIL")
    return 0 if data["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
