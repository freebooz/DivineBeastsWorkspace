#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01Macro：UE5.8 D3D12实际双岩壁截图的像素可读性与亮度对照门禁。

只有上一脚本实际输出1920×1080 PNG后才运行；不从SourceArt伪造GPU结果。
对20米左宏观三向、右亮度校准UV计算平均亮度/标准差/近黑/过曝/梯度强度；
与2026-10-11原实测平衡光截图亮度差50左右作有条件改善比较。
生成JSON证据并保留原图，不能凭像素统计取代用户真人艺术签收/GPU时间。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
FOLDER=ROOT/"Game/Saved/Validation/TerrainGaea/CliffRock01Macro20261011"
BASE=ROOT/"Game/Saved/Validation/TerrainGaea/CliffRock01"
BASE_REPORT=BASE/"CliffRock01_20m_GPU_VisualMetrics.json"
NAME_LIST=(
  "CliffRock01Macro_20m_BalancedLights_ActualGPU.png",
  "CliffRock01Macro_20m_SoftLights_ActualGPU.png"
)
OUTPUT=FOLDER/"CliffRock01Macro_ActualGPU_VisualMetrics.json"


def region_stats(pixels:np.ndarray,rect:tuple[int,int,int,int])->dict:
    x0,y0,x1,y1=rect
    roi=pixels[y0:y1,x0:x1].astype(np.float32)
    gray=roi[:,:,0]*.2126+roi[:,:,1]*.7152+roi[:,:,2]*.0722
    return {
        "meanLuma":round(float(gray.mean()),3),
        "stdLuma":round(float(gray.std()),3),
        "darkFraction":round(float(np.mean(gray<12)),5),
        "clippedFraction":round(float(np.mean(gray>250)),5),
        "edgeGradient":round(float(np.abs(np.diff(gray,axis=1)).mean()+
                                   np.abs(np.diff(gray,axis=0)).mean()),3),
    }


def main()->None:
    if not BASE_REPORT.is_file():
        raise FileNotFoundError("基线UE5.8 D3D12真实GPU审核报告缺失")
    previous=json.loads(BASE_REPORT.read_text(encoding="utf-8"))
    reference=next((x for x in previous["images"] if "BalancedLights" in x["file"]),None)
    if not reference:
        raise RuntimeError("缺少旧UE GPU可比的平衡光照基线")
    rois={"WorldAligned20m":(150,350,780,720),
          "UV20m":(900,350,1500,720)}
    # 新旧均须使用相同固定Camera、20m立面和曝光条件才可比较。
    result=[]
    for name in NAME_LIST:
        path=FOLDER/name
        if not path.is_file():
            raise FileNotFoundError("新的实际UE GPU RenderTarget截图尚未生成："+str(path))
        with Image.open(path) as image:
            if image.format!="PNG" or image.size!=(1920,1080):
                raise RuntimeError("实际SceneCapture2D目标不是1920×1080有效PNG："+name)
            rgb=np.asarray(image.convert("RGB"))
        roidata={key:region_stats(rgb,rect) for key,rect in rois.items()}
        a,b=roidata["WorldAligned20m"],roidata["UV20m"]
        if a["stdLuma"]<15 or a["clippedFraction"]>.20 or a["darkFraction"]>.30:
            raise RuntimeError("三向宏观Cliff实际岩壁画面过曝/过暗或接近纯色："+name)
        result.append({
            "file":name,"sha256":hashlib.sha256(path.read_bytes()).hexdigest(),
            "resolution":[1920,1080],"regions":roidata,
            "luminanceGap":round(abs(a["meanLuma"]-b["meanLuma"]),3),
        })
    oldgap=abs(reference["rois"]["WorldAligned20m"]["lumaMean"]-
               reference["rois"]["UV20m"]["lumaMean"])
    newgap=result[0]["luminanceGap"]
    report={
        "kind":"actual_UE5_8_D3D12_20m_scene_capture",
        "sourceReviewMap":"/Game/Development/Terrain/Maps/L_DBA_CliffRock01MacroReview",
        "baseMapUnaffected":True,
        "originalBalancedLightLumaGap":round(oldgap,3),
        "macroBalancedLightLumaGap":newgap,
        "uvBrightnessBalanceImproved":newgap<oldgap,
        "images":result,
        "doNotClaimRealtimeGPUFrameCost":True,
        "manualUserApproval":False
    }
    if OUTPUT.exists():
        raise FileExistsError("已有宏观GPU证据，拒绝覆盖")
    OUTPUT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("CLIFF_MACRO_GPU_PIXELS_QA_PASS","newBrightnessGap",newgap,
          "oldBrightnessGap",round(oldgap,3),
          "uvBalanceImproved",newgap<oldgap,
          "images",len(result),
          "manualUserApproval=False")


if __name__=="__main__":
    main()
