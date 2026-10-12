#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""《神兽联盟》Village PCG（新手村程序化内容）发行资源前置门禁。

只依据真实Gaea来源、当前项目层实际资产及经过人工审定的MeshSet来源做只读审查。
目录/Blueprint（蓝图）存在不代表StaticMesh（静态网格）或3A艺术制作完成。
不读取/篡改二进制uasset、不调用UE编辑器或触碰正式地图；失败输出具体缺口。
全部通过也不替代UE AssetRegistry、G01~G16、导航、Cook及人工美术验收。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
VILLAGE = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village"
CONTENT = VILLAGE / "Content"
DEFAULT_PROJECTION = VILLAGE / "SourceArt/Terrain/CalibrationDerivedV2/TerrainProjectionManifest_v1.json"
DEFAULT_BINDINGS = VILLAGE / "SourceArt/PCG/VillagePCGMeshBindings_v1.json"
REQUIRED = {
    "Forest.Canopy": "ClientCosmetic",
    "Rock.Scatter": "ClientCosmetic",
    "Agri.Crop": "ClientCosmetic",
    "Encl.FieldFence": "StaticWorld",
    "Connector.Gate": "StaticWorld",
    "Connector.Bridge": "StaticWorld",
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def normalize(asset: str) -> Path | None:
    """只能引用当前Village内容包的真实资产，不接受Foundation/Engine BasicShapes测试替身。"""
    if not isinstance(asset, str) or not asset.startswith("/DBAWorldPack_Village/"):
        return None
    relative = asset.removeprefix("/DBAWorldPack_Village/")
    # 研发原型即使通过了文件SHA，也不能被人为换到正式MeshSet映射里冒充3A美术。
    if relative.startswith("Development/") or relative.endswith("_Tech") or ".Technical" in relative:
        return None
    if "." in relative:
        basename, object_name = relative.rsplit(".", 1)
        if object_name != basename.rsplit("/", 1)[-1]:
            return None
        relative = basename
    parts = Path(relative).parts
    if not relative or ".." in parts or not all(parts):
        return None
    result = (CONTENT / (relative + ".uasset")).resolve()
    return result if result.is_relative_to(CONTENT.resolve()) else None


def check(projection: Path, bindings: Path) -> dict:
    issues: list[str] = []
    meta: dict = {"schemaVersion": 1, "passed": False,
                  "checkType": "项目静态源门禁，不是UE自动化/Cook验收",
                  "terrainManifest": str(projection),
                  "meshManifest": str(bindings), "missingRoles": [],
                  "checkedMeshes": []}
    project = ROOT / "Game/DivineBeastsArena.uproject"
    config = ROOT / "Game/Config/DefaultGame.ini"
    if not project.is_file():
        issues.append("正式DivineBeastsArena.uproject不存在")
    if not config.is_file() or '/Game/Development/Foundation' not in config.read_text(encoding="utf-8-sig"):
        issues.append("Foundation开发模板排除Cook的项目配置缺失")

    if not projection.is_file():
        issues.append("没有可读的正式Gaea地貌投影manifest")
    else:
        try:
            doc = json.loads(projection.read_text(encoding="utf-8-sig"))
            meta["terrainSourceKind"] = doc.get("sourceKind")
            meta["projectionVersion"] = doc.get("projectionVersion")
            if doc.get("schemaVersion") != 1 or doc.get("projectionVersion") != 2:
                issues.append("必须使用PCG Schema v1与道路/排除区已分离的投影版本2")
            if doc.get("sourceKind") != "gaea_2_3_export":
                issues.append("当前来源是技术校准或未知数据，不是可追溯的真实Gaea 2.3导出")
            else:
                # 仅改sourceKind字符串不能把技术校准冒充真实Gaea；复核原始清单和九Mask/高度。
                src_name = doc.get("sourceManifestPath")
                if not isinstance(src_name, str) or not src_name:
                    issues.append("正式Gaea投影缺少原始sourceManifestPath来源清单")
                else:
                    src = (ROOT / src_name).resolve()
                    if not src.is_relative_to(ROOT) or not src.is_file():
                        issues.append("原始Gaea清单不在工作区或不可读取")
                    elif digest(src) != doc.get("sourceSHA256"):
                        issues.append("原始Gaea清单SHA256与投影来源记录不一致")
                    else:
                        raw_doc = json.loads(src.read_text(encoding="utf-8-sig"))
                        if (raw_doc.get("sourceKind") != "gaea_2_3_export" or
                                not str(raw_doc.get("gaeaVersion", "")).startswith("2.3")):
                            issues.append("原始地形来源不是真实Gaea 2.3")
                        masks = raw_doc.get("masks")
                        required = {
                            "Slope", "Erosion", "Deposition", "RockExposure", "Flow",
                            "Shore", "Forest", "Buildable", "Exclusion"
                        }
                        if not isinstance(masks, dict) or set(masks) != required:
                            issues.append("Gaea原始九种语义掩码来源不完整")
                        else:
                            for label, spec in [("Height", raw_doc.get("height"))] + list(masks.items()):
                                if not isinstance(spec, dict):
                                    issues.append("Gaea原始源条目格式非法：" + label)
                                    continue
                                raw_path = spec.get("sourcePath")
                                if not isinstance(raw_path, str):
                                    issues.append("Gaea源路径缺失：" + label)
                                    continue
                                raw_file = (ROOT / raw_path).resolve()
                                if (not raw_file.is_relative_to(ROOT) or not raw_file.is_file() or
                                        digest(raw_file) != spec.get("sha256")):
                                    issues.append("Gaea高度/掩码SHA256缺失或已变化：" + label)
            if doc.get("roadOverlaySHA256") is None:
                issues.append("未提供项目道路覆盖图源；需要正式道路Spline/Mask衔接证明，不能把Exclusion当RoadDirt")
            outputs = doc.get("generatedFiles")
            if not isinstance(outputs, list) or len(outputs) < 11:
                issues.append("10地形Weight和PCG Schema点的投影源文件清单缺失")
            else:
                for item in outputs:
                    rel = item.get("relativePath", "")
                    path = (projection.parent / rel).resolve()
                    if (not path.is_relative_to(projection.parent.resolve())
                            or not path.is_file() or digest(path) != item.get("sha256")):
                        issues.append("Gaea投影文件缺失、越界或来源哈希不符：" + str(rel))
        except (ValueError, OSError, KeyError, TypeError) as error:
            issues.append("投影清单解析/文件指纹失败：" + str(error))

    if not bindings.is_file():
        issues.append("缺少项目美术批准的6类MeshSet静态资源映射清单：" + str(bindings))
        meta["missingRoles"] = sorted(REQUIRED)
    else:
        try:
            catalog = json.loads(bindings.read_text(encoding="utf-8-sig"))
            if catalog.get("schemaVersion") != 1 or catalog.get("project") != "DivineBeasts":
                issues.append("项目MeshSet清单的Schema或归属不符")
            records = catalog.get("meshBindings")
            if not isinstance(records, list):
                raise ValueError("meshBindings必须为列表")
            indexed: dict[str, dict] = {}
            for record in records:
                role = record.get("domainId")
                if role not in REQUIRED or role in indexed:
                    issues.append("无效或重复的项目PCG资源领域：" + str(role))
                    continue
                indexed[role] = record
            meta["missingRoles"] = sorted(set(REQUIRED) - set(indexed))
            if meta["missingRoles"]:
                issues.append("缺少平台已登记领域的项目静态网格：" + ",".join(meta["missingRoles"]))
            for role, record in indexed.items():
                if record.get("usage") != REQUIRED[role]:
                    issues.append("资源碰撞/权威用途与领域不符：" + role)
                path = normalize(record.get("meshAssetPath"))
                if path is None or not path.is_file() or path.stat().st_size == 0:
                    issues.append("UE项目内容包内不存在已批准真实StaticMesh：" + role)
                    continue
                h = digest(path)
                if h != record.get("sha256"):
                    issues.append("用户已审批StaticMesh来源SHA-256不匹配：" + role)
                meta["checkedMeshes"].append(
                    {"role": role, "path": str(path.relative_to(ROOT)), "sha256": h})
        except (ValueError, OSError, TypeError, AttributeError) as error:
            issues.append("项目MeshSet清单解析失败：" + str(error))

    meta["issues"] = sorted(set(issues))
    meta["passed"] = not meta["issues"]
    return meta


def main() -> int:
    parser = argparse.ArgumentParser(description="Village PCG发行美术/Gaea来源安全门禁")
    parser.add_argument("--projection-manifest", type=Path, default=DEFAULT_PROJECTION)
    parser.add_argument("--mesh-manifest", type=Path, default=DEFAULT_BINDINGS)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = check(args.projection_manifest.resolve(), args.mesh_manifest.resolve())
    print("VILLAGE_PCG_PRODUCTION_SOURCE_READY", result["passed"])
    for issue in result["issues"]:
        print("PCG_PRODUCTION_BLOCKER", issue)
    if args.report:
        path = args.report.resolve()
        approved = (ROOT / "Saved/Validation").resolve()
        if not path.is_relative_to(approved):
            raise ValueError("审计报告只允许写入Saved/Validation，不修改正式资源或源清单")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
        print("PCG_PRODUCTION_READINESS_REPORT", path)
    if not result["passed"]:
        print("PCG_PRODUCTION_BLOCKED：源码/校准不等于正式UE生产资源")
        return 2
    print("PCG_PRODUCTION_SOURCE_CHECK_ONLY_PASS：仍须UE AssetRegistry、G01~G16和双Cook")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
