#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Electric Dreams 官方森林示例本地准入检查（不复制或修改UE二进制资源）。

职责：从已授权下载的独立示例工程识别真实来源、PCG森林关卡和相关资产，
检查SSLM第三层内容包的公开Git隔离。输出只读证据；不得把扫描通过当作Fab权益证明。
所属：SSLM项目层资源导入工具，调用方为技术美术/资源集成人员。
生命周期：单次命令运行，不联网、不访问用户凭据、不触碰现有Village地图。
状态：source_missing/source_incomplete/source_on_disk；三种状态均不代表已经迁移。
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
from typing import Any

WORKSPACE = Path(__file__).resolve().parents[3]
CONFIG_REL = ("Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/"
              "SourceArt/PCG/TropicalMountainForest_v1.json")
FOREST_MAP_REL = "Levels/PCG/Breakdown_Levels/ElectricDreams_PCGForest.umap"
# UE包头魔数仅排除显然伪造或不完整文件；完整有效性仍由UE AssetRegistry负责。
UE_PACKAGE_TAG = bytes((0xC1, 0x83, 0x2A, 0x9E))
SAMPLE_BASE = Path("D:/UnrealSamples")
# 这些路径在公开仓库必须保持Git忽略；UE资源仍由第三层内容包持有。
IMPORTED_REL = ("Game/Plugins/DivineBeasts/ContentPacks/Worlds/"
                "DBAWorldPack_Village/Content/Environment/ThirdParty/ElectricDreams")
QUARANTINE_REL = "Game/Content/ExternalImports/ElectricDreams"
PROBES = (
    IMPORTED_REL + "/Meshes/SM_ThirdParty_LicenseProbe.uasset",
    QUARANTINE_REL + "/Meshes/SM_ThirdParty_LicenseProbe.uasset",
)


def sample_project_candidates(source: Path) -> list[Path]:
    """只搜索显式目录及其下一层项目，不全盘搜索，不读取Epic账号数据。"""
    source = source.expanduser().resolve()
    if source.is_file():
        return [source] if source.suffix.lower() == ".uproject" else []
    if not source.is_dir():
        return []
    paths = list(source.glob("*.uproject"))
    paths.extend(p for d in source.iterdir() if d.is_dir() for p in d.glob("*.uproject"))
    return sorted(set(p.resolve() for p in paths))


def has_native_package_header(path: Path) -> bool:
    """只读检查真实UE包前四字节；不是解析/制造UE二进制资产。"""
    if not path.is_file() or path.stat().st_size < 64:
        return False
    with path.open("rb") as stream:
        return stream.read(4) == UE_PACKAGE_TAG


def inspect_source(sample: Path, workspace: Path) -> dict[str, Any]:
    """磁盘来源结构盘点；虚构的uproject/umap文件并不构成Epic许可或引擎可加载证明。"""
    result: dict[str, Any] = {
        "sourceStatus": "source_missing", "sourceProject": None, "forestMapPresent": False,
        "assetCount": 0, "contentSizeBytes": 0, "pcgCandidates": [],
        "assetRegistryVerified": False, "fabEntitlementVerified": False,
        "migrationPerformed": False, "villageMapModified": False,
    }
    for project in sample_project_candidates(sample):
        if project.is_relative_to(workspace.resolve()):
            continue  # 禁止把SSLM主工程误认为Electric Dreams原始示例。
        try:
            info = json.loads(project.read_text(encoding="utf-8-sig"))
        except (OSError, UnicodeDecodeError, ValueError):
            continue
        if not isinstance(info, dict) or "FileVersion" not in info:
            continue
        content = project.parent / "Content"
        forest_map = content / FOREST_MAP_REL
        if not content.is_dir():
            continue
        count = 0
        size_bytes = 0
        candidates: list[str] = []
        for path in content.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in (".uasset", ".umap"):
                continue
            if not has_native_package_header(path):
                continue  # 仅统计具有UE包头的磁盘资源，拒绝测试夹具冒充示例资源。
            count += 1
            size_bytes += path.stat().st_size
            rel = path.relative_to(content).as_posix()
            if ("forest" in rel.lower() or "pcg" in rel.lower()) and len(candidates) < 40:
                candidates.append(rel)
        forest_map_valid = has_native_package_header(forest_map)
        status = "source_on_disk" if forest_map_valid and count >= 2 else "source_incomplete"
        result.update({
            "sourceStatus": status, "sourceProject": str(project),
            "sourceEngineAssociation": info.get("EngineAssociation"),
            "forestMapPresent": forest_map_valid,
            "assetCount": count, "contentSizeBytes": size_bytes,
            "pcgCandidates": candidates,
        })
        # 已定位实际项目后不再误选旁边其他UE工作区。
        break
    return result


def inspect_git_protection(workspace: Path) -> dict[str, Any]:
    """验证Git忽略与已跟踪清单，禁止公开仓库包含Fab第三方源资源。"""
    output: dict[str, Any] = {"gitCheckAvailable": False, "ignoredProbes": {},
                              "trackedThirdPartyPaths": [], "safeForPublicGit": False}
    try:
        for probe in PROBES:
            x = subprocess.run(
                ["git", "check-ignore", "--no-index", "-q", "--", probe],
                cwd=workspace, capture_output=True, text=True, timeout=12, check=False)
            output["ignoredProbes"][probe] = x.returncode == 0
        tracked = subprocess.run(
            ["git", "ls-files", "--", IMPORTED_REL, QUARANTINE_REL],
            cwd=workspace, capture_output=True, text=True, timeout=12, check=False)
        output["gitCheckAvailable"] = tracked.returncode == 0
        if tracked.returncode == 0:
            output["trackedThirdPartyPaths"] = tracked.stdout.splitlines()[:100]
    except (OSError, subprocess.TimeoutExpired):
        return output
    output["safeForPublicGit"] = (output["gitCheckAvailable"]
                                   and all(output["ignoredProbes"].values())
                                   and not output["trackedThirdPartyPaths"])
    return output


def inspect(workspace: Path, sample: Path) -> dict[str, Any]:
    """统一报告：先执行项目归属校验，再盘点磁盘和公开Git边界。"""
    workspace = workspace.resolve()
    config = json.loads((workspace / CONFIG_REL).read_text(encoding="utf-8"))
    if (config.get("projectCode") != "SSLM"
            or config.get("contentOwner") != "DBAWorldPack_Village"
            or config.get("biomeId") != "Village.OuterMountains.TropicalRainforest"):
        raise RuntimeError("不允许在其它项目或内容包导入本项目森林资源")
    boundaries = config["importBoundary"]
    if boundaries["publicGitRedistributionForbidden"] is not True:
        raise RuntimeError("公开仓库第三方资产禁分发门禁被关闭")
    if (boundaries["localImportedRelativePath"] != IMPORTED_REL
            or boundaries["quarantineRelativePath"] != QUARANTINE_REL):
        raise RuntimeError("项目官方资源隔离路径未与仓库规则一致")
    source = inspect_source(sample, workspace)
    git = inspect_git_protection(workspace)
    ready = source["sourceStatus"] == "source_on_disk" and git["safeForPublicGit"]
    return {
        "schemaVersion": 1, "projectCode": "SSLM",
        "sourceListing": config["source"]["fab"], "officialForestMap": FOREST_MAP_REL,
        "targetMountPath": boundaries["targetMountPath"],
        "sample": source, "gitBoundary": git,
        "sourceDiskPreflightReady": ready,
        "mayMigrateWithoutFabEntitlementCheck": False,
        "importStatus": "awaiting_authorized_sample" if not ready else "awaiting_epic_ownership_and_ue_asset_registry",
        "note": ("只读磁盘检查；不能证明Fab账户已授权、UE5.8依赖兼容、"
                 "材质已导入、林木已生成、Cook或关卡烘焙通过。"),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="SSLM Electric Dreams 官方森林示例只读预检")
    parser.add_argument("--sample-root", type=Path, default=SAMPLE_BASE,
                        help="已用Epic Launcher创建的示例工程目录或其父目录")
    parser.add_argument("--workspace-root", type=Path, default=WORKSPACE,
                        help="SSLM DivineBeastsWorkspace工作空间根")
    parser.add_argument("--report", type=Path, default=None,
                        help="可选JSON报告位置，必须位于工作空间的Saved目录内")
    parser.add_argument("--require-source", action="store_true",
                        help="若无完整示例工程，以退出码2阻断正式迁移流程")
    args = parser.parse_args()
    report = inspect(args.workspace_root, args.sample_root)
    if args.report is not None:
        target = args.report.resolve()
        evidence_root = (args.workspace_root.resolve() / "Saved").resolve()
        if not target.is_relative_to(evidence_root):
            raise ValueError("证据报告只能写到工作空间Saved，避免把资产扫描结果提交公开仓库")
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists():
            raise FileExistsError("拒绝覆盖已有来源审查证据：" + str(target))
        target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                          encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    if not report["gitBoundary"]["safeForPublicGit"]:
        return 3
    if args.require_source and not report["sourceDiskPreflightReady"]:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
