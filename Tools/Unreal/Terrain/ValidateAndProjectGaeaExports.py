#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""严格校验Gaea 2.3真实地貌源包，并生成同源的Landscape权重及PCG Schema v1调试点。

不允许把校准样本冒充真实Gaea导出；生产模式必须sourceKind=gaea_2_3_export、
gaeaVersion以2.3开头、具备Height R16及完整9张8位语义掩码/坐标、SHA256。
--calibration是显式程序化样本测试入口，数据与原PBR校准图相同。
只输出PNG/JSON供后续真正UE Editor生成/导入；不修改正式Landscape或服务器地图。
默认--verify-only只检查；新增数据时必须显式--output-dir并拒绝覆盖已有目录。
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path

import numpy as np
from PIL import Image

from TerrainMaskRules import LAYERS, MASK_NAMES, project_landscape_weights, sample_pcg_schema_v1

ROOT = Path(__file__).resolve().parents[3]
CALIBRATION = Path(__file__).with_name("TerrainSourceManifest_v1.json")

def read_path(spec:dict,depth:int,shape=None)->np.ndarray:
    src=(ROOT/spec["sourcePath"]).resolve()
    if not src.is_relative_to(ROOT) or not src.is_file():
        raise FileNotFoundError("Gaea源图必须在工作区且已存在："+str(src))
    digest=hashlib.sha256(src.read_bytes()).hexdigest()
    if digest!=spec["sha256"]:
        raise ValueError("Gaea源图SHA256变化："+str(src))
    if src.suffix.lower()==".r16":
        if depth!=16 or shape is None:
            raise ValueError("裸R16必须提供16位高程宽高")
        raw=np.fromfile(src,dtype="<u2")
        if raw.size!=shape[0]*shape[1]:
            raise ValueError("R16字节数不符合声明尺寸")
        return raw.reshape(shape[1],shape[0])
    if src.suffix.lower()!=".png":
        raise ValueError("仅支持精确R16与PNG源："+str(src))
    with Image.open(src) as im:
        im.load()
        a=np.asarray(im)
        if im.format!="PNG":
            raise ValueError("源图不是合法PNG")
    if a.ndim!=2 or (depth==16 and a.dtype!=np.uint16) or (
            depth==8 and a.dtype!=np.uint8):
        raise ValueError("源图必须为单通道"+str(depth)+"位："+str(src))
    if shape is not None and a.shape!=(shape[1],shape[0]):
        raise ValueError("掩码/高度尺寸不一致："+str(src))
    return a

def parse_source(doc:dict,calibration:bool):
    if calibration:
        if doc.get("sourceKind")!="procedural_calibration_not_gaea" or doc.get("schemaVersion")!=1:
            raise ValueError("--calibration仅支持技术校准来源")
        files=doc["files"]
        hrec=next(i for i in files if i["category"]=="HEIGHT_R16_CALIBRATION")
        dims=doc["terrainSamples"]
        h=read_path(hrec,16,dims)
        masks={}
        for name in MASK_NAMES:
            rec=next(i for i in files if i.get("maskName")==name and i.get("category")=="GAEA_COMPATIBLE_SAMPLE_MASK")
            masks[name]=read_path(rec,8,dims).astype(np.float32)/255
        old=doc["worldConvention"]
        geo={"worldOriginXcm":0.0,"worldOriginYcm":0.0,
             "worldSizeXcm":old["worldSizeCm"][0],"worldSizeYcm":old["worldSizeCm"][1],
             "minHeightCm":old["minHeightCm"],"maxHeightCm":old["maxHeightCm"]}
        kind="calibration_only_not_gaea"
    else:
        if (doc.get("schemaVersion")!=1 or doc.get("sourceKind")!="gaea_2_3_export"
                or not str(doc.get("gaeaVersion","")).startswith("2.3")):
            raise ValueError("正式模式必须提供真实Gaea 2.3导出manifest")
        full=doc["terrainWorld"]
        required=("worldOriginXcm","worldOriginYcm","worldSizeXcm","worldSizeYcm",
                  "minHeightCm","maxHeightCm","axisX","axisY","flipX","flipY")
        if not all(k in full for k in required) or full["axisX"]!="positive_right" or full["axisY"]!="positive_up":
            raise ValueError("Gaea坐标/翻转/高度单位没有显式校验")
        geo={k:full[k] for k in required[:6]}
        dims=doc["heightSamples"]
        h=read_path(doc["height"],16,dims)
        masks={}
        for name in MASK_NAMES:
            if name not in doc["masks"]:
                raise ValueError("Gaea缺少已批准Mask："+name)
            masks[name]=read_path(doc["masks"][name],8,dims).astype(np.float32)/255
        if full["flipX"]:
            h=np.fliplr(h)
            masks={k:np.fliplr(a) for k,a in masks.items()}
        if full["flipY"]:
            h=np.flipud(h)
            masks={k:np.flipud(a) for k,a in masks.items()}
        kind="gaea_2_3_export"
    if h.shape!=next(iter(masks.values())).shape:
        raise ValueError("Height与Mask像素网格不一致")
    return h,masks,geo,kind

def main():
    parser=argparse.ArgumentParser(description="Gaea源包到Landscape十权重与PCG候选点共同投影")
    parser.add_argument("--input-manifest",type=Path,default=CALIBRATION)
    parser.add_argument("--calibration",action="store_true")
    parser.add_argument("--verify-only",action="store_true")
    parser.add_argument("--output-dir",type=Path)
    args=parser.parse_args()
    source=args.input_manifest.resolve()
    if not source.is_relative_to(ROOT) or not source.is_file():
        raise FileNotFoundError("Gaea Manifest不存在于项目内："+str(source))
    doc=json.loads(source.read_text(encoding="utf-8"))
    height,masks,geo,origin=parse_source(doc,args.calibration)
    weights=project_landscape_weights(masks)
    pcg_points=sample_pcg_schema_v1(masks,weights,height,geo,grid_step=32)
    print("TERRAIN_GAEA_MASK_PROJECTOR_VALID",origin,
          "resolution",height.shape,"weights",len(weights),"pcg_candidates",len(pcg_points))
    if args.verify_only:
        print("TERRAIN_GAEA_VALIDATED_NO_UE_WRITE")
        return
    if args.output_dir is None:
        raise ValueError("未指定全新--output-dir，拒绝输出")
    out=args.output_dir.resolve()
    if not out.is_relative_to(ROOT) or out.exists():
        raise ValueError("导出目录必须是工作区新路径，拒绝覆盖")
    outputs=[]
    for name,img in weights.items():
        buf=io.BytesIO()
        Image.fromarray(img.astype(np.uint8),"L").save(buf,"PNG",compress_level=6)
        outputs.append((Path("Weights")/("LS_"+name+"_Import.png"),buf.getvalue()))
    point_json={"schemaVersion":1,"sourceKind":origin,"pointCount":len(pcg_points),"points":pcg_points}
    outputs.append((Path("PCG")/"PCG_SchemaV1_TerrainCandidates.json",
                    (json.dumps(point_json,ensure_ascii=False)+"\n").encode("utf-8")))
    receipt={"schemaVersion":1,"sourceKind":origin,"heightSamples":[height.shape[1],height.shape[0]],
             "terrainWorld":geo,"layers":list(LAYERS),"pcgSchemaVersion":"1.0",
             "sourceSHA256":hashlib.sha256(source.read_bytes()).hexdigest(),
             "generatedFiles":[{"relativePath":str(path).replace("\\","/"),
                               "sha256":hashlib.sha256(raw).hexdigest()}
                              for path,raw in outputs],
             "warningZh":"PNG/候选点不是已执行的UE Landscape/PCGGraph导入，不具备专服世界生成权威"}
    outputs.append((Path("TerrainProjectionManifest_v1.json"),
                    (json.dumps(receipt,ensure_ascii=False,indent=2)+"\n").encode("utf-8")))
    out.mkdir(parents=True)
    for rel,raw in outputs:
        file=out/rel
        file.parent.mkdir(parents=True,exist_ok=True)
        file.write_bytes(raw)
        print("TERRAIN_GAEA_PROJECTOR_OUTPUT",file.resolve().relative_to(ROOT),len(raw))
    print("TERRAIN_GAEA_PROJECTION_SAVED sourceKind="+origin+" outputFiles="+str(len(outputs)))

if __name__=="__main__":
    main()
