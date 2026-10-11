#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""跨游戏地貌语义投影：Gaea地貌Mask→10地表层权重与PCG Schema v1候选。

源码归Tools/Unreal/Terrain；既有GamePlatformPCG Schema v1是PCG字段唯一真源，
此处只构造可校验的项目数据，不添加未知Pcg.*字段、不改变PCG服务器权威。
岩石/道路/农田等具体十基础层属于第三层DBA项目映射；平台未来复用函数机制，
但不得把项目名字硬编码到GamePlatformPCG公共头文件。
"""
from __future__ import annotations

import math
from typing import Mapping

import numpy as np

LAYERS = (
    "Grass","ForestSoil","Dirt","Rock","Scree","Sand","Gravel",
    "RoadDirt","FarmSoil","ShoreMud"
)
MASK_NAMES = (
    "Slope","Erosion","Deposition","RockExposure","Flow","Shore",
    "Forest","Buildable","Exclusion"
)
# 应用已有DomainCatalog中明确登记的PCG Domain，不新建另一套地貌身份协议。
DOMAIN_BY_LAYER = {
    "Grass":"Forest.Floor", "ForestSoil":"Forest.Canopy",
    "Dirt":"Forest.Floor", "Rock":"Rock.Formation",
    "Scree":"Rock.Scatter", "Sand":"Water.Bank", "Gravel":"Rock.Scatter",
    "RoadDirt":"Road.Surface", "FarmSoil":"Agri.Crop","ShoreMud":"Water.Bank",
}

def coerce_masks(masks: Mapping[str,np.ndarray]) -> dict[str,np.ndarray]:
    """将同尺寸0~1浮点掩码校验并转换float32，任何未知或NaN都拒绝。"""
    if set(masks)!=set(MASK_NAMES):
        raise ValueError("地貌Mask需九项稳定语义，不允许未知/缺失："+str(sorted(set(masks)^set(MASK_NAMES))))
    out={}
    shape=None
    for name in MASK_NAMES:
        v=np.asarray(masks[name], dtype=np.float32)
        if v.ndim!=2 or v.shape[0]<16 or v.shape[1]<16:
            raise ValueError("Mask必须至少16×16的二维灰度："+name)
        if shape is not None and v.shape!=shape:
            raise ValueError("掩码坐标和尺寸不一致："+name)
        if not np.isfinite(v).all() or np.any(v<0) or np.any(v>1):
            raise ValueError("Mask有NaN、Inf或超过0~1值域："+name)
        shape=v.shape
        out[name]=v
    return out

def project_landscape_weights(masks:Mapping[str,np.ndarray]) -> dict[str,np.ndarray]:
    """10层非负权重，按每像素精确规范到255，空权重兜底Dirt。"""
    m=coerce_masks(masks)
    slope,rock,shore,forest,exc,flow,dep,build=(
        m["Slope"],m["RockExposure"],m["Shore"],m["Forest"],
        m["Exclusion"],m["Flow"],m["Deposition"],m["Buildable"]
    )
    clip=lambda x: np.clip(x,0,1)
    n=slope.shape
    candidates=(
        (1-slope*.6)*(.42+forest*.42)*(1-shore*.65)*(1-exc),
        forest*.60*(1-rock)*(1-exc),
        np.full(n,.18,dtype=np.float32),
        rock*1.7+slope*.75,
        clip(slope*.75+m["Erosion"]*.55)*(1-shore),
        shore*.68*(1-flow*.4),
        dep*.36+flow*.45,
        exc*2.0,
        build*.56*(1-exc),
        shore*.4+flow*.33,
    )
    # 必须和当前技术样本生产算法完全一致；源像素浮点精度由Gaea8位量化引入误差另测。
    stack=np.stack([clip(z) for z in candidates],axis=0)
    total=stack.sum(axis=0)
    invalid=total<1e-7
    if np.any(invalid):
        stack[:,invalid]=0
        stack[2,invalid]=1
        total=stack.sum(axis=0)
    weights=np.uint8(np.floor(stack/total*255))
    residual=255-weights.astype(np.int16).sum(axis=0)
    dirt=weights[2].astype(np.int16)+residual
    if np.any(dirt<0) or np.any(dirt>255):
        raise RuntimeError("归一化后的Dirt兜底值溢出")
    weights[2]=dirt.astype(np.uint8)
    if np.any(weights.astype(np.uint16).sum(axis=0)!=255):
        raise RuntimeError("材质权重总和不等于255，禁止导入UE")
    return {name:weights[i] for i,name in enumerate(LAYERS)}

def sample_pcg_schema_v1(
    masks:Mapping[str,np.ndarray],
    weights:Mapping[str,np.ndarray],
    height_u16:np.ndarray,
    world:Mapping[str,float],
    *,
    grid_step:int=24,
    seed:int=20261011
)->list[dict]:
    """从与Landscape相同的掩码、权重和高度输出PCG调试点，不执行生成或网络权威。"""
    m=coerce_masks(masks)
    h=np.asarray(height_u16)
    if h.dtype!=np.uint16 or h.shape!=m["Slope"].shape:
        raise ValueError("PCG采样高度必须与Mask同网格uint16")
    if set(weights)!=set(LAYERS) or any(np.asarray(weights[k]).shape!=h.shape for k in LAYERS):
        raise ValueError("PCG采样图层权重不是标准10层同网格")
    if np.any(np.sum(np.stack([np.asarray(weights[k],dtype=np.uint16) for k in LAYERS]),axis=0)!=255):
        raise ValueError("PCG层权重不满足归一化")
    if not 8<=grid_step<=1024 or not -2147483647<=seed<=2147483647:
        raise ValueError("PCG采样间隔或32位稳定Seed非法")
    for required in ("worldOriginXcm","worldOriginYcm","worldSizeXcm","worldSizeYcm",
                     "minHeightCm","maxHeightCm"):
        if required not in world or not math.isfinite(float(world[required])):
            raise ValueError("Gaea地形世界变换缺少有限数值："+required)
    rows,cols=h.shape
    xx0,yy0=float(world["worldOriginXcm"]),float(world["worldOriginYcm"])
    sx,sy=float(world["worldSizeXcm"]),float(world["worldSizeYcm"])
    minz,maxz=float(world["minHeightCm"]),float(world["maxHeightCm"])
    if sx<=0 or sy<=0 or maxz<=minz:
        raise ValueError("非法Terrain世界范围/海拔区间")
    points=[]
    for y in range(0,rows,grid_step):
        for x in range(0,cols,grid_step):
            priorities=tuple(int(weights[layer][y,x]) for layer in LAYERS)
            pick=int(np.argmax(priorities))
            layer=LAYERS[pick]
            exc=float(m["Exclusion"][y,x])
            domain=DOMAIN_BY_LAYER[layer]
            # 使用已登记Pcg.*稳定属性名；这些是外部调试记录，UE编辑器节点需另行实际设置。
            entry={
                "Grid": [x,y],
                "WorldPositionCm": [xx0+sx*x/(cols-1),
                                    yy0+sy*(rows-1-y)/(rows-1),
                                    minz+(maxz-minz)*int(h[y,x])/65535],
                "Pcg.Biome.Id": layer,
                "Pcg.Biome.Weight": priorities[pick]/255.0,
                "Pcg.Biome.Priority": 70 if layer=="RoadDirt" else 30,
                "Pcg.Layer.Name":domain,
                "Pcg.Layer.Index":pick,
                "Pcg.Rule.Slope":float(m["Slope"][y,x]),
                "Pcg.Rule.Height":minz+(maxz-minz)*int(h[y,x])/65535,
                "Pcg.Rule.Wetness":min(1.0,float((m["Shore"][y,x]+m["Flow"][y,x])*.5)),
                "Pcg.Exclude.Mask":exc,
                "Pcg.Exclude.Source":"Road.Network" if exc>=.5 else "None",
                "Pcg.Exec.Seed":int(((seed ^ (x*73856093) ^ (y*19349663)) & 0x7fffffff))
            }
            points.append(entry)
    return points
