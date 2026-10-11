#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从已签署Gaea兼容高程/权重PNG导出UE5.8 ALandscape::Import需要的原生R16/R8字节。

专供独立技术审核地图L_DBA_TerrainMaskReview使用。数据仍是程序化Calibration，
不是Gaea真实出口；不修改Village正式地图、物理碰撞或服务器PCG权威。
校准地形1009顶点=8组件×(2子节×63四边形)+1，Landscape每顶点RAW为little endian
unsigned16，权重R8按图像行顺序（上到下）与Manifest的flipY规范转换一致。
输出带SHA256并拒绝覆盖任何不同的旧原图；--verify仅检查，绝不伪装UE资源。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[3]
SOURCE=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Terrain"
DERIVED=SOURCE/"CalibrationDerived"
OUTPUT=DERIVED/"Raw"
MANIFEST=DERIVED/"Raw/TerrainUEImportRaw_v1.json"
LAYERS=("Grass","ForestSoil","Dirt","Rock","Scree","Sand","Gravel","RoadDirt","FarmSoil","ShoreMud")

def sha(content:bytes)->str: return hashlib.sha256(content).hexdigest()

def main():
    parser=argparse.ArgumentParser(description="校准Terrain同网格R16/R8裸数据及UE导入合同")
    parser.add_argument("--verify",action="store_true")
    opts=parser.parse_args()
    with Image.open(SOURCE/"Calibration/Height_1009_R16_Source.png") as im:
        height=np.asarray(im)
    if height.dtype!=np.uint16 or height.shape!=(1009,1009):
        raise RuntimeError("Height必须1009×1009真正16bit unsigned")
    out=[(OUTPUT/"Height_1009_LE.r16",height.astype("<u2",copy=False).tobytes(order="C"))]
    all_weights=[]
    for name in LAYERS:
        path=DERIVED/"Weights"/("LS_"+name+"_Import.png")
        with Image.open(path) as im:
            weight=np.asarray(im)
        if weight.dtype!=np.uint8 or weight.shape!=height.shape:
            raise ValueError("该图层权重不匹配Height真实网格："+name)
        all_weights.append(weight)
        out.append((OUTPUT/"Weights"/("LS_"+name+".r8"),weight.tobytes(order="C")))
    if not np.all(np.stack(all_weights,axis=0).astype(np.uint16).sum(axis=0)==255):
        raise RuntimeError("原生导入前发现多图层权重总和不为255")
    doc={
        "schemaVersion":1,
        "sourceKind":"calibration_only_not_gaea",
        "engine":"UE5.8",
        "mapPath":"/Game/Development/Terrain/Maps/L_DBA_TerrainMaskReview",
        "materialPath":"/DBAWorldPack_Village/Terrain/Materials/M_DBA_Terrain_MultiLayer10",
        "layerInfoRoot":"/DBAWorldPack_Village/Terrain/LayerInfos",
        "layers":[{"layerId":s,"layerName":"LS_"+s,
                   "assetPath":"/DBAWorldPack_Village/Terrain/LayerInfos/LI_DBA_Terrain_"+s,
                   "rawSource":(OUTPUT/"Weights"/("LS_"+s+".r8")).relative_to(ROOT).as_posix()}
                  for s in LAYERS],
        "heightSource":(OUTPUT/"Height_1009_LE.r16").relative_to(ROOT).as_posix(),
        "gridSamplesX":1009,"gridSamplesY":1009,"sectionSizeQuads":63,"numSubsections":2,
        "componentsX":8,"componentsY":8,
        "axisConvention":"pixel_row_down_world_y_positive_up_flip_import_y",
        "worldSizeCm":[100800.0,100800.0],
        "minHeightCm":-10000.0,"maxHeightCm":20000.0,
        "files":[{"relativePath":p.relative_to(ROOT).as_posix(),"sha256":sha(content),"bytes":len(content)}
                 for p,content in out],
        "noteZh":"校准样本转裸R16/R8，仅供独立技术审查。尚未执行真正Landscape或Gaea导入"
    }
    out.append((MANIFEST,(json.dumps(doc,ensure_ascii=False,indent=2)+"\n").encode("utf-8")))
    for path,content in out:
        if opts.verify and not path.is_file():
            raise FileNotFoundError("真实原生导入数据不存在："+str(path))
        if path.exists() and path.read_bytes()!=content:
            raise RuntimeError("不允许覆盖已人工修改的地形导入数据："+str(path))
    if not opts.verify:
        for path,content in out:
            if not path.exists():
                path.parent.mkdir(parents=True,exist_ok=True)
                path.write_bytes(content)
                print("TERRAIN_RAW_CREATED",path.resolve().relative_to(ROOT),len(content))
    print("TERRAIN_RAW_IMPORT_QA_PASS r16=1 r8=10 pixels=1018081 weight_sum=255",
          "mode="+("verify" if opts.verify else "generate"))
if __name__=="__main__":
    main()
