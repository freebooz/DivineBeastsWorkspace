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
DERIVED_V1=SOURCE/"CalibrationDerived"  # 历史V1导出只读兼容，不覆盖已保存的技术校准数据。
DERIVED_V2=SOURCE/"CalibrationDerivedV2"  # V2将通用排除区域与独立道路权重分离。
LAYERS=("Grass","ForestSoil","Dirt","Rock","Scree","Sand","Gravel","RoadDirt","FarmSoil","ShoreMud")

def sha(content:bytes)->str: return hashlib.sha256(content).hexdigest()

def main():
    parser=argparse.ArgumentParser(description="校准Terrain同网格R16/R8裸数据及UE导入合同")
    parser.add_argument("--verify",action="store_true")
    parser.add_argument("--projection-version",type=int,choices=(1,2),default=1,
                        help="1=保留历史校准，2=采用分离道路/排除的地貌语义")
    opts=parser.parse_args()
    derived=DERIVED_V2 if opts.projection_version==2 else DERIVED_V1
    output=derived/"Raw"
    manifest=output/"TerrainUEImportRaw_v1.json"
    projection=None
    if opts.projection_version==2:
        # 所有V2权重必须来自完整并有SHA256合同的独立投影，不能借用V1数据冒充道路。
        projection=json.loads((derived/"TerrainProjectionManifest_v1.json").read_text(encoding="utf-8"))
        if projection.get("projectionVersion")!=2 or projection.get("sourceKind")!="calibration_only_not_gaea":
            raise RuntimeError("V2地貌投影来源/版本不符，不允许生成UE原始权重")
        if projection.get("roadOverlaySHA256") is not None:
            raise RuntimeError("技术校准V2无独立道路源，不应带有RoadOverlay")
    with Image.open(SOURCE/"Calibration/Height_1009_R16_Source.png") as im:
        height=np.asarray(im)
    if height.dtype!=np.uint16 or height.shape!=(1009,1009):
        raise RuntimeError("Height必须1009×1009真正16bit unsigned")
    out=[(output/"Height_1009_LE.r16",height.astype("<u2",copy=False).tobytes(order="C"))]
    all_weights=[]
    for name in LAYERS:
        path=derived/"Weights"/("LS_"+name+"_Import.png")
        if projection is not None:
            approved=next((x["sha256"] for x in projection["generatedFiles"]
                           if x["relativePath"]=="Weights/LS_"+name+"_Import.png"),None)
            if approved is None or hashlib.sha256(path.read_bytes()).hexdigest()!=approved:
                raise RuntimeError("V2地形权重PNG未通过原始投影指纹验证："+name)
        with Image.open(path) as im:
            weight=np.asarray(im)
        if weight.dtype!=np.uint8 or weight.shape!=height.shape:
            raise ValueError("该图层权重不匹配Height真实网格："+name)
        all_weights.append(weight)
        out.append((output/"Weights"/("LS_"+name+".r8"),weight.tobytes(order="C")))
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
                   "rawSource":(output/"Weights"/("LS_"+s+".r8")).relative_to(ROOT).as_posix()}
                  for s in LAYERS],
        "heightSource":(output/"Height_1009_LE.r16").relative_to(ROOT).as_posix(),
        "gridSamplesX":1009,"gridSamplesY":1009,"sectionSizeQuads":63,"numSubsections":2,
        "componentsX":8,"componentsY":8,
        "axisConvention":"pixel_row_down_world_y_positive_up_flip_import_y",
        "worldSizeCm":[100800.0,100800.0],
        "minHeightCm":-10000.0,"maxHeightCm":20000.0,
        "files":[{"relativePath":p.relative_to(ROOT).as_posix(),"sha256":sha(content),"bytes":len(content)}
                 for p,content in out],
        "noteZh":"校准样本转裸R16/R8，仅供独立技术审查。尚未执行真正Landscape或Gaea导入"
    }
    if opts.projection_version==2:
        doc["projectionVersion"]=2
        doc["projectionManifestSHA256"]=hashlib.sha256(
            (derived/"TerrainProjectionManifest_v1.json").read_bytes()).hexdigest()
        doc["noteZh"]="V2校准权重保留玩法排除独立语义；未提供道路源时RoadDirt必须为零，仍非真实Gaea 2.3出口"
        if np.any(all_weights[LAYERS.index("RoadDirt")]):
            raise RuntimeError("V2校准场景禁止将Gameplay.Exclusion误记为RoadDirt")
    out.append((manifest,(json.dumps(doc,ensure_ascii=False,indent=2)+"\n").encode("utf-8")))
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
