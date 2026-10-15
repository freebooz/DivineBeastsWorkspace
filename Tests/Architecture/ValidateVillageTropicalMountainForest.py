#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM外围山体热带森林静态门禁；仅审查配置，不代替UE森林生成。"""
import ast
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
PACK=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village"
PLAN=PACK/"SourceArt/PCG/TropicalMountainForest_v1.json"
AUTHOR=ROOT/"Tools/Unreal/PCG/AuthorVillageTropicalMountainForest.py"

def main():
    data=json.loads(PLAN.read_text(encoding="utf-8"))
    assert data["schemaVersion"]==1
    assert data["projectCode"]=="SSLM"
    assert data["contentOwner"]=="DBAWorldPack_Village"
    assert data["worldScope"]["boundedVolumeRequired"]
    assert len(data["worldScope"]["allowed"])==3
    assert all("OuterMountain" in x for x in data["worldScope"]["allowed"])
    assert [x["id"] for x in data["layers"]]==["Canopy","Understory","Floor"]
    assert all(0<x["samplingDensityPerSquareMeter"]<=.005 for x in data["layers"])
    assert all(not x["meshPaths"] for x in data["layers"])
    assert not data["status"]["electricDreamsAssetsMigrated"]
    assert not data["status"]["releaseReady"]
    ast.parse(AUTHOR.read_text(encoding="utf-8"))
    print("VILLAGE_TROPICAL_FOREST_STATIC_CONTRACT_PASS")
    print("ElectricDreamsImported=False;ForestBaked=False")

if __name__=="__main__":
    main()
