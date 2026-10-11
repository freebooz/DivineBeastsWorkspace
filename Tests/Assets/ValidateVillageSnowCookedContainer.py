#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Village写实积雪双端真实IoStore清单验证：必须从新一轮UnrealPak生成CSV，不可伪造。

共享地图为了运行时MID必须拥有Server-safe的中性地形母材质和三张极小默认纹理；
高清雪图、Niagara及项目纯客户端MI只允许Cook到WindowsClient。
这不是源文件/INI的静态查找，必须提供真正的新Cook/Stage .utoc对应的UnrealPak CSV。
任何缺失、重复或服务器带入平台雪表现资源均返回非零退出码。
"""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

SHARED = (
    "DBAWorldPack_Village/Content/Maps/L_Village_Start.umap",
    "DBAWorldPack_Village/Content/Materials/Landscape/M_DBA_Village_LandscapeBridge.uasset",
    "DBAWorldPack_Village/Content/Materials/Landscape/Defaults/T_DBA_Village_SnowCoverageDefault.uasset",
    "DBAWorldPack_Village/Content/Materials/Landscape/Defaults/T_DBA_Village_SnowAlbedoDefault.uasset",
    "DBAWorldPack_Village/Content/Materials/Landscape/Defaults/T_DBA_Village_SnowNormalDefault.uasset",
)
CLIENT_ONLY = (
    "GamePlatformSurface/Content/Textures/Snow/T_GP_Snow_CoverageNoise.uasset",
    "GamePlatformSurface/Content/Textures/Weather/T_GP_Weather_SnowAlbedo.uasset",
    "GamePlatformSurface/Content/Textures/Weather/T_GP_Weather_SnowNormal.uasset",
    "GamePlatformVFX/Content/Weather/Niagara/NS_GP_Weather_Snow_Detailed.uasset",
    "GamePlatformVFX/Content/Weather/Definitions/DA_GP_VFX_Weather_Snow.uasset",
)
SERVER_REQUIRED = (
    "DBAWorldPack_Village/Content/Weather/Definitions/DA_DBA_Weather_HeavySnow.uasset",
)
FORBIDDEN_SERVER = (
    "/GamePlatformSurface/Content/",
    "/GamePlatformVFX/Content/",
    "/DBAPresentationPack_Core/Content/",
    "/Content/Development/Snow/",
)


def entries(source: Path) -> set[str]:
    if not source.is_file():
        raise FileNotFoundError(f"未发现该轮新生成的UnrealPak清单：{source}")
    with source.open(encoding="utf-8-sig", newline="", errors="replace") as stream:
        reader = csv.DictReader(stream, skipinitialspace=True)
        if not reader.fieldnames or "Filename" not in reader.fieldnames:
            raise ValueError(f"清单不含Filename列：{source}")
        result = {str(row["Filename"]).strip().replace("\\", "/").casefold()
                  for row in reader if row.get("Filename")}
    if len(result) < 20:
        raise ValueError("容器条目异常过少，不能认定为真实游戏Cook")
    return result


def require(actual: set[str], expected: tuple[str, ...], kind: str) -> None:
    for path in expected:
        matches = [s for s in actual if s.endswith(("/" + path).casefold())]
        if len(matches) != 1:
            raise RuntimeError(f"{kind}资产必须仅出现一次：{path}，实际{len(matches)}项")
        print(f"VILLAGE_SNOW_{kind}_ASSET_OK {path}")


def main() -> None:
    parser = argparse.ArgumentParser(description="验证正式Village双端IoStore中性地形与客户端雪纹理隔离")
    parser.add_argument("--client", type=Path, required=True)
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    client = entries(args.client)
    server = entries(args.server)
    require(client, SHARED + CLIENT_ONLY, "CLIENT")
    require(server, SHARED + SERVER_REQUIRED, "SERVER")
    forbidden = sorted(s for s in server if any(x.casefold() in s for x in FORBIDDEN_SERVER))
    if forbidden:
        raise RuntimeError("DedicatedServer非法携带客户端纯表现资源：" + ";".join(forbidden[:20]))
    report = {
        "clientPassed": True, "serverPassed": True,
        "sharedLandscapeAssetsRequired": len(SHARED),
        "clientOnlySnowAssetsRequired": len(CLIENT_ONLY),
        "serverOnlyWeatherAssetsRequired": len(SERVER_REQUIRED),
        "clientEntries": len(client), "serverEntries": len(server),
        "forbiddenServerCount": len(forbidden),
        "clientContainerManifest": str(args.client.resolve()),
        "serverContainerManifest": str(args.server.resolve()),
        "resultMeaningZh": "仅表示此轮真实UnrealPak容器内容正确，不能推导联机/风场/GUI视觉/帧率通过",
    }
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                               encoding="utf-8")
    print("VILLAGE_SNOW_REAL_IOSTORE_CROSS_TARGET_PASS",
          "client_entries", len(client), "server_entries", len(server),
          "forbidden_server", len(forbidden))


if __name__ == "__main__":
    main()
