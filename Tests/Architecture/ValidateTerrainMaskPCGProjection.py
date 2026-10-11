#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Gaea掩码→Landscape10层→既有PCG Schema v1共享数据合同回归。

所属Tests/Architecture，只使用NumPy/Pillow和正式项目SourceArt。验证三个性质：
1. 真Gaea身份失败关闭，不能把本脚本的Calibration冒充真实导出；
2. 10材质层权重逐像素严格为255，非法/无效Mask拒绝，RoadDirt优先压制草地；
3. 与同一张1009x1009地形图同步产生稳定PCG候选点，属性名全部来自已有SchemaV1。
这不是UE中的真正PCG Graph运行、Landscape LayerInfo导入或客户端Cook，不允许用于后者签收。
"""

from __future__ import annotations

import copy
import json
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT / "Tools/Unreal/Terrain"))
from TerrainMaskRules import (LAYERS, MASK_NAMES, coerce_masks,
                              project_landscape_weights, sample_pcg_schema_v1)
from ValidateAndProjectGaeaExports import parse_source

MANIFEST=ROOT/"Tools/Unreal/Terrain/TerrainSourceManifest_v1.json"
DERIVED=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Terrain/CalibrationDerived"
REQUIRED_SCHEMA={
    "Pcg.Biome.Id","Pcg.Biome.Weight","Pcg.Biome.Priority","Pcg.Layer.Name",
    "Pcg.Layer.Index","Pcg.Rule.Slope","Pcg.Rule.Height","Pcg.Rule.Wetness",
    "Pcg.Exclude.Mask","Pcg.Exclude.Source","Pcg.Exec.Seed"
}

def raises(fn, message:str) -> None:
    try:
        fn()
    except (ValueError,TypeError,RuntimeError,FileNotFoundError,KeyError):
        return
    raise AssertionError("非法输入被意外接纳："+message)

def main() -> None:
    doc=json.loads(MANIFEST.read_text(encoding="utf-8"))
    assert doc["sourceKind"]=="procedural_calibration_not_gaea"
    h,masks,geo,kind=parse_source(doc,True)
    assert kind=="calibration_only_not_gaea" and h.dtype==np.uint16 and h.shape==(1009,1009)
    expected=project_landscape_weights(masks)
    derived={}
    for layer in LAYERS:
        path=DERIVED/"Weights"/("LS_"+layer+"_Import.png")
        with Image.open(path) as im:
            derived[layer]=np.asarray(im).copy()
        assert np.array_equal(expected[layer],derived[layer]), ("同源地貌Weight不稳定："+layer)
    stack=np.stack([expected[k] for k in LAYERS],axis=0)
    totals=stack.astype(np.uint16).sum(axis=0)
    assert int(totals.min())==int(totals.max())==255
    assert int(np.max(stack[LAYERS.index("RoadDirt")]))>100
    assert int(np.max(stack[LAYERS.index("Rock")]))>55
    assert int(np.max(stack[LAYERS.index("Grass")]))>80
    points=sample_pcg_schema_v1(masks,expected,h,geo,grid_step=32)
    json_points=json.loads((DERIVED/"PCG"/"PCG_SchemaV1_TerrainCandidates.json").read_text(encoding="utf-8"))
    assert len(points)==1024
    assert json_points["pointCount"]==len(points)
    assert json_points["points"]==points
    for point in points:
        keys={k for k in point if k.startswith("Pcg.")}
        assert keys==REQUIRED_SCHEMA
        assert point["Pcg.Biome.Id"] in LAYERS
        assert np.isfinite(point["WorldPositionCm"]).all()
        assert 0<=point["Pcg.Biome.Weight"]<=1
        assert 0<=point["Pcg.Exclude.Mask"]<=1
        assert point["Pcg.Exec.Seed"]>=0
        if point["Pcg.Exclude.Mask"]>=.5:
            assert point["Pcg.Exclude.Source"]=="Road.Network"
    assert points==sample_pcg_schema_v1(masks,expected,h,geo,grid_step=32)
    # 与GamePlatformPCGEditor命令中的默认0.18阈值保持同源；每个区域都必须有
    # 至少一批实际Landscape采样点，否则Graph虽然能创建，但QA场景是空图。
    sample_indices=np.arange(0,h.shape[0],32)
    biome_hits={}
    for label in ("Grass","Rock","ShoreMud","RoadDirt"):
        image=derived[label][np.ix_(sample_indices,sample_indices)]
        biome_hits[label]=int(np.count_nonzero(image.astype(np.float32)/255.0>=.18))
        if biome_hits[label]<10:
            raise AssertionError("PCG图层阈值导致"+label+"产生不足10个样本点")
    print("TERRAIN_PCG_BIOME_THRESHOLD_QA",biome_hits,"threshold=.18")

    raises(lambda:parse_source(doc,False),"没有真实Gaea的技术样本")
    raises(lambda:coerce_masks({"Slope":masks["Slope"]}),"缺少其他八种语义Mask")
    bad=dict(masks)
    bad["Slope"]=bad["Slope"].copy()
    bad["Slope"][0,0]=float("nan")
    raises(lambda:project_landscape_weights(bad),"NaN坡度不能写入地表")
    bad=dict(masks)
    bad["Slope"]=np.ones((16,16),dtype=np.float32)
    raises(lambda:project_landscape_weights(bad),"Mask尺寸不一致")
    bad=dict(masks)
    bad["RockExposure"]=np.full(masks["RockExposure"].shape,-.1,np.float32)
    raises(lambda:project_landscape_weights(bad),"负掩码非法")
    bad=dict(expected)
    bad["Dirt"]=bad["Dirt"].copy()
    bad["Dirt"][0,0]=0
    raises(lambda:sample_pcg_schema_v1(masks,bad,h,geo,grid_step=32),"基础层缺权重")
    badgeo=dict(geo)
    badgeo["worldSizeXcm"]=float("nan")
    raises(lambda:sample_pcg_schema_v1(masks,expected,h,badgeo),"非法世界范围")
    print("TERRAIN_GAEA_PCG_SHARED_SEMANTIC_TEST_PASS weight_pixels",totals.size,
          "pcg_points",len(points),"layers",len(LAYERS),"invalid_input_cases",7)
    print("TERRAIN_GAEA_AUTHENTICITY_FAIL_CLOSED verified=calibration_only")

if __name__=="__main__":
    main()
