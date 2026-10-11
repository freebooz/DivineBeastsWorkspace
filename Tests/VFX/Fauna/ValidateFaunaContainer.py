#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""蝴蝶与鱼群交付包审计：只读取真实 UnrealPak 的 IoStore CSV 清单。

说明：
- Client（客户端）核对两种物种合计20个必须运行的资产包。
- Server（专用服务器）不得包含任何 Fauna（环境生物）纯视觉资源、源美术或审核地图。
- 支持多容器分别导出的 CSV；所有文件都必须标识准确目标平台，不能以空文件或空列表冒充通过。
- 本工具不生成资源、不修改Cook配置；报告不等于实际Cook/Stage已成功。
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
from pathlib import Path


PLATFORM_ROOT = (
    "/plugins/gameplatform/presentation/gameplatformvfx/"
    "content/environment/fauna"
)
PROJECT_ROOT = (
    "/plugins/divinebeasts/contentpacks/presentation/"
    "dbapresentationpack_core/content/vfx/environment/fauna"
)
SPECIES = ("ButterflyAzure", "FishSilver")


def required_client_assets() -> list[str]:
    """平台四包+项目每物种八包；Niagara编辑器模块图不单独作为运行必需包。"""
    result = [
        f"{PLATFORM_ROOT}/effecttypes/NET_GP_Fauna_Ambient.uasset",
        f"{PLATFORM_ROOT}/materials/M_GP_Fauna_Master.uasset",
        f"{PLATFORM_ROOT}/materials/T_GP_Fauna_NeutralRoughness.uasset",
        f"{PLATFORM_ROOT}/niagara/NS_GP_Fauna_Base.uasset",
    ]
    for species in SPECIES:
        base = f"{PROJECT_ROOT}"
        result.extend(
            [
                f"{base}/blueprints/BP_DBA_{species}Habitat.uasset",
                f"{base}/definitions/DA_DBA_VFX_Fauna_{species}.uasset",
                f"{base}/materials/MI_DBA_{species}.uasset",
                f"{base}/meshes/SM_DBA_{species}.uasset",
                f"{base}/niagara/NS_DBA_{species}.uasset",
                f"{base}/textures/T_DBA_{species}_BaseColor.uasset",
                f"{base}/textures/T_DBA_{species}_Normal.uasset",
                f"{base}/textures/T_DBA_{species}_Roughness.uasset",
            ]
        )
    return result


def normalize(value: str) -> str:
    """标准化清单中的斜杠和大小写；仅用于比较，不修改磁盘路径。"""
    return value.strip().replace("\\", "/").casefold()


def is_development_fauna(value: str) -> bool:
    p = normalize(value)
    return any(marker in p for marker in (
        "/game/development/fauna/",
        "/development/fauna/",
        "__externalactors__/development/fauna/",
        "__externalobjects__/development/fauna/",
        "__externalactors__/game/development/fauna/",
        "__externalobjects__/game/development/fauna/",
    ))


def is_fauna_asset(value: str) -> bool:
    p = normalize(value)
    return (
        "/environment/fauna/" in p
        or p.endswith("/environment/fauna")
        or "/sourceart/fauna/" in p
        or is_development_fauna(value)
    )


def read_manifest(path: Path, expected_platform: str) -> tuple[bytes, set[str], int]:
    """严格校验CSV列、行及平台。任何错误必须使本次验收失败。"""
    if not path.is_file():
        raise FileNotFoundError(f"未找到真实IoStore容器清单：{path}")
    raw = path.read_bytes()
    try:
        parsed = csv.DictReader(io.StringIO(raw.decode("utf-8-sig")), skipinitialspace=True)
        columns = parsed.fieldnames or []
        clean_columns = [col.strip() for col in columns]
        if len(clean_columns) != len(set(clean_columns)):
            raise ValueError("CSV列名重复")
        if "Filename" not in clean_columns or "Platform" not in clean_columns:
            raise ValueError("CSV缺少Filename/Platform列")
        filename_column = columns[clean_columns.index("Filename")]
        platform_column = columns[clean_columns.index("Platform")]
        found: set[str] = set()
        row_count = 0
        for row_index, row in enumerate(parsed, 2):
            if None in row:
                raise ValueError(f"CSV第{row_index}行字段数量错误")
            filename = row.get(filename_column)
            platform = row.get(platform_column)
            if filename is None or not filename.strip() or platform is None:
                raise ValueError(f"CSV第{row_index}行字段缺失")
            if normalize(platform) != expected_platform.casefold():
                raise ValueError(
                    f"CSV第{row_index}行目标不匹配：{platform}，期望{expected_platform}"
                )
            found.add(normalize(filename))
            row_count += 1
        if not found:
            raise ValueError(f"CSV没有实际文件：{path}")
    except UnicodeError as error:
        raise ValueError(f"CSV编码无效：{path}") from error
    return raw, found, row_count


def validate(csv_paths: list[Path], target: str) -> dict:
    if target not in ("Client", "Server"):
        raise ValueError("必须明确目标Client或Server")
    if not csv_paths:
        raise ValueError("未提供任何真实容器CSV清单")
    expected_platform = "Windows" + target
    all_files: set[str] = set()
    sources: list[dict] = []
    for path in csv_paths:
        raw, found, total_rows = read_manifest(path, expected_platform)
        all_files.update(found)
        sources.append({
            "path": str(path),
            "sha256": hashlib.sha256(raw).hexdigest(),
            "rows": total_rows,
            "uniqueFiles": len(found),
        })
    required = required_client_assets() if target == "Client" else []
    missing = [file for file in required if not any(
        filename.endswith(file.casefold()) for filename in all_files
    )]
    forbidden = sorted(
        file for file in all_files
        if (target == "Server" and is_fauna_asset(file))
        or (target == "Client" and (
            is_development_fauna(file) or "/sourceart/fauna/" in file
            or ("/environment/fauna/" in file and file.endswith((".fbx", ".blend", ".png", ".py")))
        ))
    )
    return {
        "status": "passed" if not missing and not forbidden else "failed",
        "schemaVersion": 1,
        "target": target,
        "expectedPlatform": expected_platform,
        "containerCount": len(sources),
        "containerSources": sources,
        "uniqueContainerFiles": len(all_files),
        "faunaFiles": sum(is_fauna_asset(file) for file in all_files),
        "requiredCount": len(required),
        "required": required,
        "missing": missing,
        "forbidden": forbidden,
        "source": "UnrealPak CSV manifests, not source UAssets or configuration",
        "manualArtAcceptance": "pending",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", type=Path, action="append", required=True,
                        help="每个IoStore容器各指定一次--csv，不接受未导出的容器")
    parser.add_argument("--target", choices=("Client", "Server"), required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = validate(args.csv, args.target)
    except (OSError, ValueError, csv.Error) as error:
        report = {
            "status": "failed", "schemaVersion": 1, "target": args.target,
            "inputError": str(error), "missing": [], "forbidden": [],
            "requiredCount": 0, "containerSources": [],
            "manualArtAcceptance": "pending",
        }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(
        "FAUNA_CONTAINER_AUDIT", report["status"], args.target,
        "required", report["requiredCount"], "missing", len(report["missing"]),
        "forbidden", len(report["forbidden"])
    )
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
