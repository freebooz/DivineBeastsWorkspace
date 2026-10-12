#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""十层地形晴/苔/湿/小雪/大雪/融雪真实GPU图像有效性门禁。

独立源为UE5.8 SceneCapture2D；不能接受用户拍照、占位黑帧或PBR源纹理拼图。
必须读原始像素，检测六幅有效非重复画面并输出真实的帧差量化。
这只是技术GPU视觉QA，不代替用户主观美术签收或ProfileGPU性能预算。
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
OUTPUT=ROOT/"Game/Saved/Validation/TerrainPBR_GPU_20261012"
META=OUTPUT/"TerrainPBR_GPU_Evidence.json"
REQUIRED=("Dry","Moss","Wet","LightSnow","HeavySnow","AfterSnow")
RES=(1280,720)

def main():
 if not META.is_file():raise FileNotFoundError("没有真实UE5.8 RHI捕获的GPU证据清单")
 obj=json.loads(META.read_text(encoding="utf-8"))
 if obj.get("origin")!="UE5.8 SceneCapture2D" or len(obj.get("frames",[]))!=6:
  raise RuntimeError("图像来源或六种天气状态不正确")
 rows={record["name"]:record for record in obj["frames"]}
 if set(rows)!=set(REQUIRED):raise ValueError("晴天/苔藓/湿润/小雪/重雪/融雪状态不齐")
 captures={}
 metrics=[]
 for state in REQUIRED:
  rec=rows[state]
  path=Path(rec["file"]).resolve()
  if not path.is_relative_to(OUTPUT.resolve()) or path.name!=state+"_D3D12_1280x720.png":
   raise ValueError("GPU图像路径不在本次真实RHI审核目录："+str(path))
  if not path.is_file() or path.stat().st_size<30_000:
   raise FileNotFoundError("GPU图像缺失或异常过小："+str(path))
  with Image.open(path) as image:
   if image.size!=RES or image.format!="PNG":
    raise ValueError("GPU图像格式与尺寸不符："+state)
   rgb=np.asarray(image.convert("RGB"),dtype=np.float32)
  # 排除极端天空/边框，仅检查中央有效地表区域。
  center=rgb[150:590,250:1030,:]
  lum=center[:,:,:3].mean(axis=2)
  mean=float(np.mean(lum));std=float(np.std(lum))
  dark_fraction=float(np.mean(lum<4.0))
  if mean<15 or std<6 or dark_fraction>0.60:
   raise RuntimeError("GPU图像为黑图、灰板或过暗失效："+state+
                      f" mean={mean:.2f} std={std:.2f} black={dark_fraction:.2%}")
  captures[state]=center
  metrics.append({"state":state,"bytes":path.stat().st_size,
                  "sha256":hashlib.sha256(path.read_bytes()).hexdigest(),
                  "meanLuma":round(mean,3),"stdLuma":round(std,3),
                  "blackFraction":round(dark_fraction,4)})
 pairs=(("Dry","Moss"),("Dry","Wet"),("Dry","LightSnow"),
        ("LightSnow","HeavySnow"),("HeavySnow","AfterSnow"))
 diffs=[]
 for a,b in pairs:
  mean_diff=float(np.mean(np.abs(captures[a]-captures[b])))
  diffs.append({"from":a,"to":b,"meanAbsRgbDiff":round(mean_diff,3)})
  if mean_diff<1.2:
   raise RuntimeError("天气/苔藓/湿润状态GPU像素几乎无变化："+a+"→"+b+
                      " diff="+f"{mean_diff:.3f}")
 report=OUTPUT/"TerrainPBR_GPU_PixelQA.json"
 report.write_text(json.dumps({
  "source":"UE5.8 SceneCapture2D",
  "scope":"GPU frame actual-pixels only; shader perf and human approval pending",
  "frames":metrics,"pairs":diffs,
  "result":"GPU_IMAGE_PIXELS_VALID_NO_BLACK_NO_DUPLICATE"
 },ensure_ascii=False,indent=2)+"\n",encoding="utf8")
 print("TERRAIN_PBR_D3D12_GPU_IMAGE_PIXELS_PASS",len(metrics),"weather_pairs",len(diffs))
 for x in diffs:print("TERRAIN_PBR_GPU_DIFFERENCE",x["from"],x["to"],x["meanAbsRgbDiff"])
 print("TERRAIN_PBR_MANUAL_VISUAL_AND_GPU_TIME_STILL_PENDING")

if __name__=="__main__":main()
