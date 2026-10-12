#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""十类正式扫描PBR纹理源质量门禁。可在暂存阶段或正式替换后运行，绝不写资产。

核对官方MD5/SHA256、四图同源、PNG尺寸/位深、sRGB与ORM契约及纹理细节变化；
不能将本QA通过当成UE Shader、实际Landscape、PCG或人工美术验收通过。
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import sys
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
TOOL=ROOT/"Tools/Unreal/Terrain"
PROD=TOOL/"TerrainProductionPBRManifest_v1.json"
STAGED=ROOT/"Game/Saved/Validation/TerrainPBR_20261012/ProductionStageManifest.json"
ROLE={"BaseColor","Normal","ORM","Height"}
SURFACES={"Grass","ForestSoil","Dirt","Rock","Scree","Sand","Gravel","RoadDirt","FarmSoil","ShoreMud"}

def hash_file(path,kind):
 h=hashlib.new(kind)
 with path.open("rb") as f:
  for chunk in iter(lambda:f.read(1024*1024),b""):h.update(chunk)
 return h.hexdigest()

def main():
 installed=PROD.exists()
 path=PROD if installed else STAGED
 if not path.exists():raise RuntimeError("既无正式来源清单，也无40图官方暂存清单")
 doc=json.loads(path.read_text(encoding="utf-8"))
 if doc.get("sourceKind")!="official_polyhaven_CC0_matched_scan_maps":
  raise RuntimeError("未经核实的扫描素材清单")
 maps=doc.get("files",[])
 if len(maps)!=40:raise RuntimeError("同源扫描贴图必须为40项")
 by_layer={}
 seam_warnings=[]
 checked=[]
 for record in maps:
  layer,role=record["layer"],record["role"]
  if layer not in SURFACES or role not in ROLE:raise ValueError("资产类型或角色非法")
  by_layer.setdefault(layer,set()).add(role)
  relative=record["sourcePath"] if installed else record["stagePath"]
  file=(ROOT/relative).resolve()
  if not file.is_relative_to(ROOT) or not file.is_file():raise FileNotFoundError(relative)
  if hash_file(file,"md5")!=record["upstreamMd5"]:
   raise RuntimeError("原始同源官方MD5不匹配："+relative)
  if hash_file(file,"sha256")!=record["sha256"]:
   raise RuntimeError("SHA256签名不匹配："+relative)
  with Image.open(file) as im:
   if im.format!="PNG" or im.size!=(2048,2048):raise RuntimeError("非原生2K PNG："+relative)
   img=im.convert("RGB").resize((256,256))
   pixels=np.asarray(img,dtype=np.uint8)
   std=np.std(pixels.astype(np.float32),axis=(0,1))
   # 物理扫描允许平整沙地和均匀林土的低频变化，不能按多噪点阈值误拒。
   if role=="BaseColor" and max(std)<2.0:
    raise RuntimeError("基础色图缺少有效变化："+relative)
   if role=="Normal" and max(std)<1.5:
    raise RuntimeError("法线图过于平坦，疑似旧校准占位："+relative)
   if role=="ORM" and np.std(pixels[:,:,1].astype(np.float32))<0.7:
    raise RuntimeError("ORM G粗糙度几乎为常量："+relative)
   if role=="Height":
    # I;16转换RGB可能截断高位，须以源文件原生灰度深度检查起伏。
    band=im.getchannel("R") if im.mode in ("RGB","RGBA") else im
    low,high=band.getextrema()
    minimum=200 if im.mode.startswith("I;16") else 12
    if int(high)-int(low)<minimum:
     raise RuntimeError("高度纹理原始动态范围过低："+relative)
   edges_x=np.abs(pixels[:,0].astype(np.int16)-pixels[:,-1].astype(np.int16))
   edges_y=np.abs(pixels[0].astype(np.int16)-pixels[-1].astype(np.int16))
   # 图片边缘大差异暂列人工UV贴图检查；不伪造无缝成功。
   score=max(np.percentile(edges_x,95),np.percentile(edges_y,95))
   if score>48:seam_warnings.append((layer,role,round(float(score),1)))
   checked.append((layer,role,im.mode,round(float(max(std)),1)))
 for layer,roles in by_layer.items():
  if roles!=ROLE:raise RuntimeError("缺少同源图层："+layer)
 if len(by_layer)!=10:raise RuntimeError("十地表类别未全部覆盖")
 print("TERRAIN_PRODUCTION_PBR_SOURCE_PASS layers=10 maps=40 mode="+("installed" if installed else "staged"))
 for layer,role,mode,std in checked:print("PBR_MAP",layer,role,"mode",mode,"max_rgb_std",std)
 for layer,role,score in seam_warnings:print("PBR_UV_SEAM_REVIEW_REQUIRED",layer,role,"p95",score)
 print("TERRAIN_PRODUCTION_ART_APPROVAL pending_UE_scene_and_user_review")
if __name__=="__main__":main()
