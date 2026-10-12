#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""验证最新真实UnrealPak -ListContainer的雪苔Client/Server IoStore资产分层。

只读取UnrealPak导出的Filename CSV；无实际CSV不得以项目源文件存在代替Cook。
--development 指VillageDevelopmentClient必须包含开发审核地图。
专服应有Village权威地图/天气事实，且绝不能包含雪苔Shader、Niagara及项目MI。
"""
import argparse
import csv
import json
from pathlib import Path

COMMON={
    "DBAWorldPack_Village/Content/Maps/L_Village_Start.umap",
    "DBAWorldPack_Village/Content/Materials/Landscape/M_DBA_Village_LandscapeBridge.uasset",
}
CLIENT={
    "GamePlatformSurface/Content/Materials/M_GP_Surface_SnowMoss_PBR.uasset",
    "DBAPresentationPack_Core/Content/Materials/Environment/MI_DBA_MossSnow_Cliff.uasset",
    "GamePlatformSurface/Content/ParameterCollections/MPC_GP_SurfaceGlobal.uasset",
    "GamePlatformSurface/Content/Textures/Weather/T_GP_Weather_SnowAlbedo.uasset",
    "GamePlatformSurface/Content/Textures/Snow/T_GP_Snow_CoverageNoise.uasset",
    *(f"GamePlatformSurface/Content/Textures/Moss/T_GP_Moss_{role}.uasset"
      for role in ("Albedo","Normal","Orm","Height","Coverage"))
}
DEV_MAP="Content/Development/MossSnow/Maps/L_DBA_MossSnowReview.umap"
SERVER={"DBAWorldPack_Village/Content/Weather/Definitions/DA_DBA_Weather_HeavySnow.uasset"}
FORBIDDEN=(
    "/GamePlatformSurface/Content/",
    "/GamePlatformVFX/Content/",
    "/DBAPresentationPack_Core/Content/",
    "/Content/Development/MossSnow/",
    "/Content/Development/Snow/",
)

def load(path):
    """仅接受真实UnrealPak的CSV列结构，不忽略缺失字段和短容器。"""
    if not path.is_file(): raise FileNotFoundError("不存在该次Cook容器CSV："+str(path))
    with path.open(encoding="utf8-sig",newline="",errors="replace") as file:
        reader=csv.DictReader(file,skipinitialspace=True)
        if not reader.fieldnames or "Filename" not in reader.fieldnames:
            raise RuntimeError("UnrealPak CSV缺少Filename列："+str(path))
        result={str(row["Filename"]).strip().replace("\\","/").casefold()
                for row in reader if row.get("Filename")}
    if len(result)<30:raise RuntimeError("UnrealPak条目不足30条，无法确认有效Cook："+str(path))
    return result

def require(actual,targets,scope):
    for path in sorted(targets):
        occurrences=[name for name in actual if name.endswith(("/"+path).casefold())]
        if len(occurrences)!=1:
            raise RuntimeError(f"{scope}必须有且仅有一个资产：{path}，实际{len(occurrences)}")
        print("MOSS_SNOW_"+scope+"_ASSET_OK",path)

def main():
    cli=argparse.ArgumentParser(description="双端真实IoStore雪苔分层核验")
    cli.add_argument("--client",type=Path,required=True)
    cli.add_argument("--server",type=Path,required=True)
    cli.add_argument("--development",action="store_true")
    cli.add_argument("--report",type=Path)
    args=cli.parse_args()
    client=load(args.client)
    server=load(args.server)
    require(client,COMMON|CLIENT|({DEV_MAP} if args.development else set()),"CLIENT")
    require(server,COMMON|SERVER,"SERVER")
    if not args.development and any(x.endswith(("/"+DEV_MAP).casefold()) for x in client):
        raise RuntimeError("正式前端客户端非法带入专用雪苔审核地图")
    forbidden=sorted(x for x in server if any(fragment.casefold() in x for fragment in FORBIDDEN))
    if forbidden:raise RuntimeError("DedicatedServer泄漏纯表现资源："+str(forbidden[:15]))
    report={"clientAssetCount":len(client),"serverAssetCount":len(server),
            "requiredClientCount":len(COMMON|CLIENT)+int(args.development),
            "serverForbiddenCount":0,"development":args.development,
            "clientCsv":str(args.client.resolve()),"serverCsv":str(args.server.resolve()),
            "resultBoundaryZh":"仅真实IoStore资源边界验收，不证明网络天气、可见性或GPU帧率"}
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
    print("MOSS_SNOW_CLIENT_SERVER_IOSTORE_PASS",report["requiredClientCount"],len(server),"server_forbidden=0")

if __name__=="__main__":
    main()
