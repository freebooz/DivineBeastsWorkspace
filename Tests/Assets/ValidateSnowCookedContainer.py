#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从真实UnrealPak -ListContainer CSV校验积雪Client/Server IoStore边界。

作用：只读检查容器条目，不凭.uasset源文件或ini静态存在报告Cook已通过。
项目三层边界：客户端必须包含通用雪Surface/VFX及神兽联盟项目MI；
Dedicated Server必须包含Village世界/天气权威定义，但不能含任何纯表现雪资源。
使用方式：python Tests/Assets/ValidateSnowCookedContainer.py --client <CSV> --server <CSV>。
客户端CSV必需；服务器CSV可暂时不传，但不得因此宣称服务器已验收。
CSV必须来自本次真实UnrealPak -ListContainer命令，不得用人工伪造清单。
"""
from __future__ import annotations
import argparse
import csv
import json
from pathlib import Path

CLIENT_REQUIRED = (
    "GamePlatformSurface/Content/Materials/M_GP_SnowCover_Detailed.uasset",
    "GamePlatformSurface/Content/Textures/Snow/T_GP_Snow_MicroHeight.uasset",
    "GamePlatformSurface/Content/Textures/Snow/T_GP_Snow_CoverageNoise.uasset",
    "GamePlatformSurface/Content/ParameterCollections/MPC_GP_SurfaceGlobal.uasset",
    "GamePlatformVFX/Content/Weather/Niagara/NS_GP_Weather_Snow_Detailed.uasset",
    "GamePlatformVFX/Content/Weather/Definitions/DA_GP_VFX_Weather_Snow.uasset",
    "GamePlatformVFX/Content/Weather/Materials/M_GP_VFX_SnowGroundPuff.uasset",
    "GamePlatformVFX/Content/Weather/Textures/T_GP_Snow_GroundPuff.uasset",
    "DBAPresentationPack_Core/Content/Materials/Snow/MI_DBA_Snow_Detailed.uasset",
    "Content/Development/Snow/Maps/L_DBA_SnowReview.umap",
)
SERVER_REQUIRED = (
    "DBAWorldPack_Village/Content/Maps/L_Village_Start.umap",
    "DBAWorldPack_Village/Content/Weather/Definitions/DA_DBA_Weather_HeavySnow.uasset",
)
SERVER_FORBIDDEN = (
    "/GamePlatformVFX/Content/",
    "/GamePlatformSurface/Content/",
    "/DBAPresentationPack_Core/Content/",
    "/Content/Development/Snow/",
)

def read_filenames(csv_path: Path) -> set[str]:
    """引擎CSV按文件条目读取；空行及缺失Filename字段均是失败。"""
    if not csv_path.is_file():
        raise FileNotFoundError(f"未生成真实UnrealPak清单：{csv_path}")
    with csv_path.open("r", encoding="utf-8-sig", errors="replace", newline="") as handle:
        reader = csv.DictReader(handle, skipinitialspace=True)
        if reader.fieldnames is None or "Filename" not in reader.fieldnames:
            raise RuntimeError(f"UnrealPak CSV缺Filename字段：{csv_path}")
        filenames = {str(row.get("Filename", "")).strip().replace("\\", "/").casefold()
                     for row in reader if row and row.get("Filename")}
    if len(filenames) < 2:
        raise RuntimeError(f"UnrealPak实际资源条目过少，拒绝验收：{csv_path}")
    return filenames

def verify(required: tuple[str, ...], actual: set[str], scope: str) -> None:
    """所有必须存在的包均按完整相对路径和扩展名严格匹配。"""
    for asset in required:
        suffix = ("/" + asset).casefold()
        matches = [file for file in actual if file.endswith(suffix)]
        if len(matches) != 1:
            raise RuntimeError(f"{scope}资源数量不符合期望1：{asset}，实际{len(matches)}")
        print(f"SNOW_{scope}_ASSET_OK {asset}")

def main() -> None:
    parser = argparse.ArgumentParser(description="真实IoStore客户端与DedicatedServer积雪资源隔离验收")
    parser.add_argument("--client", type=Path, required=True, help="UnrealPak生成的WindowsClient CSV")
    parser.add_argument("--server", type=Path, help="UnrealPak生成的WindowsServer CSV")
    parser.add_argument("--report", type=Path, help="可选的机器可读验收摘要文件")
    args = parser.parse_args()
    client = read_filenames(args.client)
    verify(CLIENT_REQUIRED, client, "CLIENT")
    report = {"clientPassed": True, "clientEntries": len(client), "clientRequired": len(CLIENT_REQUIRED),
              "serverPassed": None, "serverForbiddenCount": None}
    print(f"SNOW_CLIENT_IOSTORE_PASS count={len(CLIENT_REQUIRED)} entries={len(client)}")
    if args.server is not None:
        server = read_filenames(args.server)
        verify(SERVER_REQUIRED, server, "SERVER")
        hits = [f for f in server if any(fragment.casefold() in f for fragment in SERVER_FORBIDDEN)]
        if hits:
            raise RuntimeError("DedicatedServer非法包含纯视觉资源：" + "; ".join(sorted(hits)[:12]))
        report.update(serverPassed=True, serverEntries=len(server), serverForbiddenCount=0)
        print(f"SNOW_SERVER_IOSTORE_ISOLATION_PASS entries={len(server)} forbidden=0")
    else:
        print("SNOW_SERVER_IOSTORE_NOT_CHECKED")
    if args.report is not None:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

if __name__ == "__main__":
    main()
