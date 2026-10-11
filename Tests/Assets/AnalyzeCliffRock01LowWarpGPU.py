#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01低扰动同GPU会话D3D12四帧审计：同区域清晰度/亮度/基准漂移。

输入只能是真实UE5.8 SceneCapture2D自动生成的完整1920x1080 PNG原图；
输出指标与三款ROI视觉对照。清晰度保持率>=0.65且双基准稳定、
曝光差距可接受才将此审核归为可继续人工评估；否则记录FAIL并保留旧材质。
这里不测GPU毫秒数，不假设场景Cook，也不判定用户已经人工签收。
"""
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image,ImageDraw,ImageFont

ROOT=Path(__file__).resolve().parents[2]
DIR=ROOT/"Game/Saved/Validation/TerrainGaea/CliffRock01LowWarp20261011"
FILES={
    "baseline":"CliffLowWarp_00_Baseline_20m_D3D12.png",
    "lowWarp":"CliffLowWarp_01_LowWarp_20m_D3D12.png",
    "fullWarp":"CliffLowWarp_02_FullWarp_20m_D3D12.png",
    "baselineRepeat":"CliffLowWarp_03_BaselineRepeat_20m_D3D12.png",
}
REPORT=DIR/"CliffRock01LowWarp_SameSessionGPUQuality_V2_CaptureValidity.json"
COMPARISON=DIR/"CliffRock01LowWarp_SameSessionGPUComparison_V2_CaptureValidity.jpg"
ROI=(150,350,780,720)
CONTROL=(900,350,1500,720)

def read_png(name:str):
    file=DIR/FILES[name]
    if not file.is_file() or file.stat().st_size<60000:
        raise FileNotFoundError("真实GPU原始PNG缺失或太小："+str(file))
    with Image.open(file) as src:
        src.load()
        if src.format!="PNG" or src.size!=(1920,1080):
            raise RuntimeError("不能用缩略图/非GPU PNG作为D3D12画面质量证据："+str(file))
        pixels=np.asarray(src.convert("RGB"),dtype=np.uint8)
    return pixels,hashlib.sha256(file.read_bytes()).hexdigest()

def metric(arr:np.ndarray)->dict:
    lum=np.dot(arr.astype(np.float32),np.asarray([.2126,.7152,.0722],dtype=np.float32))
    dx=np.abs(lum[:,1:]-lum[:,:-1])
    dy=np.abs(lum[1:,:]-lum[:-1,:])
    mask=(.25*(np.roll(lum,1,0)+np.roll(lum,-1,0)+
                 np.roll(lum,1,1)+np.roll(lum,-1,1)))
    edge=float((dx.mean()+dy.mean()))
    high=float(np.abs(lum-mask).mean())
    return {
        "lumaMean":round(float(lum.mean()),3),
        "lumaStd":round(float(lum.std()),3),
        "edgeGradient":round(edge,3),
        "highPassResidual":round(high,3),
        "nearWhiteRatio":round(float((lum>249).mean()),5),
        "nearBlackRatio":round(float((lum<7).mean()),5),
    }

def main()->None:
    frames={};sha={}
    for name in FILES:
        frames[name],sha[name]=read_png(name)
    stats={name:metric(img[ROI[1]:ROI[3],ROI[0]:ROI[2],:])
           for name,img in frames.items()}
    control={name:metric(img[CONTROL[1]:CONTROL[3],CONTROL[0]:CONTROL[2],:])
             for name,img in frames.items()}
    base=stats["baseline"]
    drift=stats["baselineRepeat"]
    low=stats["lowWarp"]
    full=stats["fullWarp"]
    eps=1e-5
    stability=1-abs(drift["edgeGradient"]-base["edgeGradient"])/max(base["edgeGradient"],eps)
    brightness_drift=abs(drift["lumaMean"]-base["lumaMean"])
    uv_range=max(x["lumaMean"] for x in control.values())-min(x["lumaMean"] for x in control.values())
    ratios={name:round(stats[name]["edgeGradient"]/max(base["edgeGradient"],eps),4)
            for name in ("lowWarp","fullWarp")}
    brightness={name:round(stats[name]["lumaMean"]-base["lumaMean"],3)
                for name in ("lowWarp","fullWarp")}
    base_stable=(stability>=.90 and brightness_drift<=8.0 and uv_range<=9.0)
    # 必须验证同一块岩面确实随Material改变而变化，禁止“相同模糊图”得到100%清晰度伪通过。
    base_roi=frames["baseline"][ROI[1]:ROI[3],ROI[0]:ROI[2],:]
    repeat_roi=frames["baselineRepeat"][ROI[1]:ROI[3],ROI[0]:ROI[2],:]
    baseline_motion=float(np.abs(base_roi.astype(np.float32)-
                                repeat_roi.astype(np.float32)).mean())
    differ={}
    for name in ("lowWarp","fullWarp"):
        img=frames[name][ROI[1]:ROI[3],ROI[0]:ROI[2],:]
        delta=np.abs(base_roi.astype(np.int16)-img.astype(np.int16))
        differ[name]={
            "roiMeanAbsRGB":round(float(delta.mean()),4),
            "changedPixelsFractionThreshold2":round(float((delta.max(axis=2)>2).mean()),5),
        }
    thresholds={"edgeRetentionMin":.65,"baselineRepeatEdgeStability":.90,
                "baselineRepeatBrightnessMax":8.0,"UVControlBrightnessRangeMax":9.0,
                "nearWhiteLimit":.08,"lumaMeanDiffMax":25.0,
                "baselineMinEdgeGradient":8.0,"variantMinAbsRGB":1.0,
                "variantMinChangedPixelFraction":.02}
    nontrivial=(all(d["roiMeanAbsRGB"]>max(1.0,3*baseline_motion) and
                    d["changedPixelsFractionThreshold2"]>.02 for d in differ.values()))
    capture_valid=base_stable and base["edgeGradient"]>=8 and nontrivial
    low_pass=(capture_valid and ratios["lowWarp"]>=.65 and
              abs(brightness["lowWarp"])<=25.0 and low["nearWhiteRatio"]<.08)
    full_pass=(capture_valid and ratios["fullWarp"]>=.65 and
               abs(brightness["fullWarp"])<=25.0 and full["nearWhiteRatio"]<.08)
    result={
        "schemaVersion":1,
        "kind":"UE5.8_real_D3D12_same_process_four_capture_sharpness_qa",
        "targetWorld":"/Game/Development/Terrain/Maps/L_DBA_CliffRock01MacroReview",
        "baselineAsset":"/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials/MI_DBA_CliffRock01_WorldAligned",
        "lowWarpAsset":"/Game/Development/Terrain/Materials/MI_DBA_CliffRock01_WorldAlignedMacro_LowWarpReview",
        "fullWarpAsset":"/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials/MI_DBA_CliffRock01_WorldAlignedMacro",
        "regionOfInterest":list(ROI),"UVControlRegion":list(CONTROL),
        "sources":{k:{"filename":FILES[k],"sha256":sha[k]} for k in FILES},
        "thresholds":thresholds,
        "statistics":stats,"UVControl":control,
        "baselineEdgeStability":round(stability,4),
        "baselineBrightnessDrift":round(brightness_drift,3),
        "UVControlBrightnessRange":round(uv_range,3),
        "baselineRepeatRoiPixelDrift":round(baseline_motion,4),
        "variantRegionDifferences":differ,
        "captureValidForMaterialComparison":capture_valid,
        "validityReasonZh":("同块20米岩壁不同Material对应的像素/边缘有充分差异且基准清晰" if capture_valid else
                            "无扰动基准纹理过糊或变体与基准区域无真实像素变化；不可将1.0比例记为通过"),
        "supersedesInvalidPriorReport":"CliffRock01LowWarp_SameSessionGPUQuality.json",
        "edgeRetentionRelativeToBaseline":ratios,
        "lumaDifferences":brightness,
        "baselineStable":base_stable,
        "lowWarpEligibleForVisualReview":low_pass,
        "fullWarpEligibleForVisualReview":full_pass,
        "userArtApproval":False,
        "knownLimitationsZh":"只比同一独立UE5.8场景中20米测试墙D3D12像素清晰度/亮度；不测GPU毫秒预算、真实Village/天气、PCG/Server Cook，用户尚未审美签收",
    }
    payload=(json.dumps(result,ensure_ascii=False,indent=2)+"\n").encode("utf-8")
    if REPORT.exists() and REPORT.read_bytes()!=payload:
        raise RuntimeError("本轮GPU审核指标已存在不同内容，拒绝覆盖："+str(REPORT))
    if not REPORT.exists():REPORT.write_bytes(payload)

    # 构图显示真实左侧同一ROI裁剪，不借其他光照或缩放假造效果。
    colsize=(630,370)
    fontpath=Path("C:/Windows/Fonts/msyh.ttc")
    ft=ImageFont.truetype(str(fontpath),23) if fontpath.is_file() else ImageFont.load_default()
    preview=Image.new("RGB",(3*630+40,500),"#152024")
    canvas=ImageDraw.Draw(preview)
    for i,(name,caption) in enumerate((("baseline","原版三向"),
                                      ("lowWarp","新20厘米低扰动"),
                                      ("fullWarp","旧98厘米宏观扰动"))):
        r=frames[name][ROI[1]:ROI[3],ROI[0]:ROI[2],:]
        view=Image.fromarray(r,"RGB")
        x=10+i*630
        preview.paste(view,(x,75))
        canvas.text((x,12),caption+"｜边缘梯度="+str(stats[name]["edgeGradient"]),
                    font=ft,fill="#e5eff0")
        canvas.text((x,457),"清晰度保持率："+str(ratios.get(name,1.0))+"，平均亮度="+str(stats[name]["lumaMean"]),
                    font=ft,fill="#bbcbd0")
    if COMPARISON.exists():
        raise FileExistsError("同场景视觉对照PNG/JPG已存在，禁止覆盖")
    preview.save(COMPARISON,"JPEG",quality=90,optimize=True)
    print("CLIFF_LOWWARP_SAME_GPU_SHARPNESS_V2",json.dumps({
        "baseline":base["edgeGradient"],"lowRatio":ratios["lowWarp"],
        "fullRatio":ratios["fullWarp"],"baseStable":base_stable,
        "roiPixelChange":differ,"captureValid":capture_valid,
        "lowWarpPass":low_pass,"fullWarpPass":full_pass},ensure_ascii=False))
    print("CLIFF_LOWWARP_SAME_GPU_METRICS_SAVED",REPORT)
    print("CLIFF_LOWWARP_SAME_GPU_PREVIEW_SAVED",COMPARISON)
    if not capture_valid:
        raise RuntimeError("同场景截屏无效：岩壁几乎无像素变化或基准贴图模糊；旧统计中的100%为伪通过，不得发布")
    if not low_pass:
        raise RuntimeError("Cliff低振幅宏观纹理仍未达到65%清晰度/亮度门禁，不可批准")
    print("CLIFF_LOWWARP_SHARPNESS_65_PERCENT_GATE_PASS pending_user_art_review=True")

if __name__=="__main__":
    main()
