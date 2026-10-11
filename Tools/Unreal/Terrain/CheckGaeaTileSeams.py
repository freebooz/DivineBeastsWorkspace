#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Gaea 2.3多瓦片R16高度与语义Mask无缝检测（只读，正式源与校准严格分离）。

对每一对横向相邻Tile比对X最末/最初列，纵向相邻比对Y最末/最初行；
要求显式tileYDirection=image_down，即TileY递增对应源图向下。源图源路径、
SHA256、尺寸和9份标准语义Mask必须全部可核验，缺失时失败关闭。
本工具不填充接缝、不压平高程、不修改原图或现有WorldPartition地图。
--self-test只检查程序算法正确，不表示用户的真实Gaea瓦片通过。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Mapping

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
MASKS = ("Slope","Erosion","Deposition","RockExposure","Flow","Shore",
         "Forest","Buildable","Exclusion")

def compare_border(a: np.ndarray,b: np.ndarray,axis: str,tolerance:int)->int:
    """两个同宽高的瓦片，横向比较右/左，纵向比较下/上，返回最大绝对差。"""
    if a.shape != b.shape or a.ndim != 2 or min(a.shape)<16:
        raise ValueError("高程或Mask瓦片尺寸不匹配")
    if axis=="horizontal":
        x,y=a[:,-1],b[:,0]
    elif axis=="vertical":
        x,y=a[-1,:],b[0,:]
    else:
        raise ValueError("不支持未批准的Tile邻接方向")
    diff=int(np.abs(x.astype(np.int32)-y.astype(np.int32)).max())
    if diff>tolerance:
        raise RuntimeError(f"Gaea Tile{axis}接缝不一致：最大误差={diff}，阈值={tolerance}")
    return diff

def content_digest(path:Path)->str:
    hash_value=hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda:stream.read(4*1024*1024),b""):
            hash_value.update(block)
    return hash_value.hexdigest()

def load_tile_image(spec:Mapping, dims:tuple[int,int], depth:int)->np.ndarray:
    path=(ROOT/spec["sourcePath"]).resolve()
    if not path.is_relative_to(ROOT) or not path.is_file():
        raise FileNotFoundError("Gaea Tile图片缺失或越过项目根："+str(path))
    if content_digest(path)!=spec["sha256"]:
        raise ValueError("Gaea Tile原图SHA256不匹配："+str(path))
    if path.suffix.lower()==".r16":
        if depth!=16 or path.stat().st_size!=dims[0]*dims[1]*2:
            raise ValueError("R16瓦片尺寸与little endian字节数不符")
        return np.memmap(path,dtype="<u2",mode="r",shape=(dims[1],dims[0]))
    with Image.open(path) as im:
        if im.format!="PNG" or im.size!=dims:
            raise ValueError("Tile PNG尺寸或格式不一致")
        arr=np.asarray(im)
        if arr.ndim!=2 or (depth==16 and arr.dtype!=np.uint16) or (
                depth==8 and arr.dtype!=np.uint8):
            raise ValueError("高程需16位/Mask需8位单通道PNG")
        return arr

def audit(manifest:dict,height_tolerance:int,mask_tolerance:int)->dict:
    if (manifest.get("schemaVersion")!=1 or
        manifest.get("sourceKind")!="gaea_2_3_export" or
        not str(manifest.get("gaeaVersion","")).startswith("2.3") or
        manifest.get("tileYDirection")!="image_down"):
        raise ValueError("必须是真实Gaea 2.3/明确影像Y+向下的Tile清单")
    dims=tuple(manifest.get("tileSamples",()))
    if len(dims)!=2 or not all(isinstance(v,int) and 16<=v<=8193 for v in dims):
        raise ValueError("瓦片尺寸与Landscape网格合同不符")
    tiles=manifest.get("tiles",[])
    if not 2<=len(tiles)<=256:
        raise ValueError("至少2张、最多256张真实Gaea瓦片")
    data={}
    for record in tiles:
        key=(record.get("tileX"),record.get("tileY"))
        if (key in data or not all(type(v)==int and abs(v)<=1024 for v in key)
                or set(record.get("masks",{}))!=set(MASKS)):
            raise ValueError("重复、不合法的Tile坐标或语义Mask缺失")
        height=load_tile_image(record["height"],dims,16)
        masks={name:load_tile_image(record["masks"][name],dims,8) for name in MASKS}
        data[key]={"Height":height,**masks}
    tested=0
    max_delta=0
    for (x,y),tile in data.items():
        for offset,axis,tolerance_height,tolerance_mask in (
            ((1,0),"horizontal",height_tolerance,mask_tolerance),
            ((0,1),"vertical",height_tolerance,mask_tolerance)
        ):
            neighbor=data.get((x+offset[0],y+offset[1]))
            if neighbor is None:continue
            for name in ("Height",*MASKS):
                diff=compare_border(tile[name],neighbor[name],axis,
                                    tolerance_height if name=="Height" else tolerance_mask)
                max_delta=max(max_delta,diff)
            tested+=1
    if tested==0:
        raise RuntimeError("所有Gaea瓦片互不相邻，无法执行接缝验收")
    return {"tileCount":len(tiles),"adjacentPairs":tested,"checkedChannels":10,
            "maxEdgeDifference":max_delta,"sourceKind":"real_gaea_2_3"}

def self_test() -> None:
    a=np.zeros((32,32),np.uint16)
    b=np.zeros((32,32),np.uint16)
    a[:,-1]=52
    b[:,0]=52
    assert compare_border(a,b,"horizontal",0)==0
    a[-1,:]=17
    b[0,:]=17
    assert compare_border(a,b,"vertical",0)==0
    b[0,0]=88
    failed=False
    try:compare_border(a,b,"horizontal",0)
    except RuntimeError:failed=True
    assert failed,"故意制造的Gaea Tile接缝错误未被拦截"
    print("TERRAIN_GAEA_TILE_SEAM_ALGORITHM_SELFTEST_PASS no_genuine_gaea_files_loaded")

def main()->None:
    parser=argparse.ArgumentParser(description="真实Gaea 2.3多Tile高度及Mask接缝只读校验")
    parser.add_argument("--manifest",type=Path)
    parser.add_argument("--height-tolerance",type=int,default=0)
    parser.add_argument("--mask-tolerance",type=int,default=0)
    parser.add_argument("--self-test",action="store_true")
    args=parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not args.manifest:
        raise ValueError("真实Gaea验收必须明确输入已签源Tile manifest；不能使用校准源代替")
    if not 0<=args.height_tolerance<=1024 or not 0<=args.mask_tolerance<=32:
        raise ValueError("Tile接缝允许误差超出经批准范围")
    path=args.manifest.resolve()
    if not path.is_relative_to(ROOT) or not path.is_file():
        raise FileNotFoundError("Gaea Tile Manifest不在工作空间内")
    doc=json.loads(path.read_text(encoding="utf-8"))
    report=audit(doc,args.height_tolerance,args.mask_tolerance)
    print("TERRAIN_GAEA_REAL_TILE_SEAM_AUDIT_PASS",json.dumps(report,ensure_ascii=False))
if __name__=="__main__":
    main()
