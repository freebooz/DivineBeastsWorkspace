#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""真实PBR源纹理/Gaea兼容校准高程/地貌掩码质量门禁；仅只读，非UE资产验证。

验证源清单SHA、10×3 PBR纹理1024可平铺、16位Height精度、所有Mask/Weight与Height同尺寸，
权重严格每像素合计255、真实Biome不同于单一恒定平面、法线RGB及ORM压缩预设。
不将程序化样本冒充Gaea出口或最终扫描纹理；也不以文件存在替代UE DataValidation/Cook。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "Tools/Unreal/Terrain/TerrainSourceManifest_v1.json"
PRODUCTION = ROOT / "Tools/Unreal/Terrain/TerrainProductionPBRManifest_v1.json"
REQUIRED_LAYERS = ("Grass","ForestSoil","Dirt","Rock","Scree","Sand",
                   "Gravel","RoadDirt","FarmSoil","ShoreMud")
REQUIRED_MASKS = ("Slope","Erosion","Deposition","RockExposure","Flow",
                  "Shore","Forest","Buildable","Exclusion")


def main() -> None:
    if not MANIFEST.is_file():
        raise FileNotFoundError("地表源资源清单缺失")
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if (manifest.get("schemaVersion") != 1 or
        manifest.get("sourceKind") != "procedural_calibration_not_gaea" or
        manifest.get("baseLayers") != list(REQUIRED_LAYERS) or
        manifest.get("maskNames") != list(REQUIRED_MASKS) or
        len(manifest.get("files", [])) != 50):
        raise RuntimeError("地表技术纹理清单版本、权重层或来源身份非法")
    production = PRODUCTION.exists()
    # 历史V1清单仍是现有Gaea V1/V2数据投影的SHA真源；正式PBR独立从新清单签名。
    output = {}
    for record in manifest["files"]:
        if production and record["category"] == "PBR_TECHNICAL":
            continue  # 用户已授权同路径替换；旧校准PBR记录仅供回退比对。
        path = (ROOT / record["sourcePath"]).resolve()
        if not path.is_relative_to(ROOT) or not path.is_file():
            raise FileNotFoundError("非法/缺失源纹理："+str(path))
        if hashlib.sha256(path.read_bytes()).hexdigest() != record["sha256"]:
            raise RuntimeError("地形原图SHA256改变，禁止继续导入："+str(path))
        with Image.open(path) as im:
            im.load()
            if im.format != "PNG":
                raise RuntimeError("源图不是PNG："+str(path))
            arr = np.asarray(im)
            kind=record["category"]
            if kind == "PBR_TECHNICAL":
                if list(im.size)!=[1024,1024] or im.mode!="RGB":
                    raise RuntimeError("PBR贴图尺寸或通道不正确："+str(path))
                h=np.abs(arr[:,0,:].astype(np.int16)-arr[:,-1,:].astype(np.int16))
                v=np.abs(arr[0,:,:].astype(np.int16)-arr[-1,:,:].astype(np.int16))
                if np.percentile(h,99)>9 or np.percentile(v,99)>9:
                    raise RuntimeError("PBR源纹理边界存在明显贴片接缝："+str(path))
                if record["role"]=="ORM" and np.any(arr[:,:,2]!=0):
                    raise RuntimeError("非金属地表ORM Metallic必须为0："+str(path))
            elif kind == "HEIGHT_R16_CALIBRATION":
                if arr.dtype!=np.uint16 or list(im.size)!=[1009,1009]:
                    raise RuntimeError("校准高程必须是无损16位PNG/1009："+str(path))
                if int(arr.max())-int(arr.min())<4000:
                    raise RuntimeError("高程没有真实高度变化")
            else:
                if im.mode!="L" or list(im.size)!=[1009,1009]:
                    raise RuntimeError("校准Mask/Weight必须为同坐标1009 L8："+str(path))
            key=(kind,record.get("layer") or record.get("maskName") or "Height",record.get("role"))
            if key in output:
                raise RuntimeError("Manifest存在重复地貌资源身份："+str(key))
            output[key]=arr.copy()
    weights = np.stack([output[("LANDSCAPE_WEIGHT",name,None)] for name in REQUIRED_LAYERS],axis=0)
    total=weights.astype(np.uint16).sum(axis=0)
    if int(total.min())!=255 or int(total.max())!=255:
        raise RuntimeError("基础层所有像素权重不能严格合计255")
    masks=[output[("GAEA_COMPATIBLE_SAMPLE_MASK",name,None)] for name in REQUIRED_MASKS]
    if any(np.unique(layer).size<4 for layer in masks):
        raise RuntimeError("部分校准掩码为常量图，不具备完整技术审查价值")
    if not production:
        for layer in REQUIRED_LAYERS:
            for role in ("BaseColor","Normal","ORM"):
                if ("PBR_TECHNICAL",layer,role) not in output:
                    raise RuntimeError("地表技术PBR三通道缺失："+layer+"/"+role)
    else:
        # 真正扫描源由 ValidateTerrainProductionPBR.py 严格核验40张原图及官方哈希。
        data=json.loads(PRODUCTION.read_text(encoding="utf-8"))
        if len(data.get("files",[])) != 40 or data.get("sourceKind") != "official_polyhaven_CC0_matched_scan_maps":
            raise RuntimeError("正式PBR来源清单不完整")
    if not manifest.get("worldConvention",{}).get("flipY",None) is True:
        raise RuntimeError("高度图行方向与世界方向没有明确映射")
    print("TERRAIN_SOURCE_QA_PASS historical_manifest=50 calibration_geo=20 pbr_kind="+
          ("production_scan_40" if production else "technical_30")+
          " heightR16=1 semantics=9 layerWeights=10")
    print("TERRAIN_WEIGHT_SUM_VERIFIED min=255 max=255 pixels=1018081")
    for i,name in enumerate(REQUIRED_LAYERS):
        print("TERRAIN_WEIGHT_LAYER",name,"min",int(weights[i].min()),"max",int(weights[i].max()))
    print("TERRAIN_SOURCE_GAEA_STATUS calibration_only_not_genuine_gaea")


if __name__ == "__main__":
    main()
