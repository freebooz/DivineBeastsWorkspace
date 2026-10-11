#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""同光照、同20米岩壁、不同已保存独立UWorld的D3D12截图质量审计。

对基准和低扰动两个进程的各两张GPU截图：
1. 各自重复画面相似，右侧不变UV对照区稳定；
2. 左侧20m岩面材质确实产生像素变化，基准不是模糊占位；
3. 低扰动清晰度至少保持原始WorldAligned的65%，平均亮度不过曝；
4. 审核仅表示进入用户美术复核的资格，不代表UE性能/Cook/真人已签。
"""
from __future__ import annotations
import hashlib
import json
import sys
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/"Game/Saved/Validation/TerrainGaea/CliffRock01LowWarpIsolated20261011"
NAMES={
    "base0":"CliffLowWarp_baseline_D3D12_0.png",
    "base1":"CliffLowWarp_baseline_D3D12_1.png",
    "low0":"CliffLowWarp_lowWarp_D3D12_0.png",
    "low1":"CliffLowWarp_lowWarp_D3D12_1.png"
}
ROI=(150,350,780,720)
CONTROL=(900,350,1500,720)
REPORT=OUT/"CliffLowWarp_IsolatedSavedMaps_GPUQuality.json"
REVIEW=OUT/"CliffLowWarp_IsolatedSavedMaps_PBRReview.jpg"

def load(name:str)->tuple[np.ndarray,str]:
    path=OUT/NAMES[name]
    if not path.is_file() or path.stat().st_size<60000:
        raise FileNotFoundError("真实UE D3D12源图缺失："+str(path))
    with Image.open(path) as image:
        image.load()
        if image.format!="PNG" or image.size!=(1920,1080):
            raise RuntimeError("必须使用1920x1080真GPU完整PNG："+str(path))
        pixels=np.asarray(image.convert("RGB"),dtype=np.uint8)
    return pixels,hashlib.sha256(path.read_bytes()).hexdigest()

def patch(arr:np.ndarray,roi:tuple[int,int,int,int])->np.ndarray:
    return arr[roi[1]:roi[3],roi[0]:roi[2],:]

def difference(a:np.ndarray,b:np.ndarray)->dict:
    d=np.abs(a.astype(np.int16)-b.astype(np.int16))
    return {
        "meanAbsoluteRGB":round(float(d.mean()),4),
        "fractionPixelsDifferingByMoreThan2":round(float((d.max(axis=2)>2).mean()),5),
    }

def metric(crop:np.ndarray)->dict:
    pixels=crop.astype(np.float32)
    lum=.2126*pixels[:,:,0]+.7152*pixels[:,:,1]+.0722*pixels[:,:,2]
    dx=np.abs(lum[:,1:]-lum[:,:-1]).mean()
    dy=np.abs(lum[1:,:]-lum[:-1,:]).mean()
    high=np.abs(lum-.25*(np.roll(lum,1,0)+np.roll(lum,-1,0)+
                          np.roll(lum,1,1)+np.roll(lum,-1,1))).mean()
    return {
        "lumaMean":round(float(lum.mean()),3),
        "lumaStd":round(float(lum.std()),3),
        "edgeGradient":round(float(dx+dy),3),
        "highPass":round(float(high),3),
        "nearWhiteRatio":round(float((lum>248).mean()),5),
    }

def main():
    frames={};sources={}
    for role in NAMES:
        frames[role],sources[role]=load(role)
    left={role:patch(arr,ROI) for role,arr in frames.items()}
    right={role:patch(arr,CONTROL) for role,arr in frames.items()}
    stats={role:metric(array) for role,array in left.items()}
    base_repeat=difference(left["base0"],left["base1"])
    low_repeat=difference(left["low0"],left["low1"])
    changed=difference(left["base0"],left["low0"])
    uv_drift=difference(right["base0"],right["low0"])
    base_edge=stats["base0"]["edgeGradient"]
    retention=round(stats["low0"]["edgeGradient"]/max(base_edge,1e-6),4)
    brightness_delta=round(stats["low0"]["lumaMean"]-stats["base0"]["lumaMean"],3)
    consistent=(
        base_repeat["meanAbsoluteRGB"]<=3.0 and
        low_repeat["meanAbsoluteRGB"]<=3.0 and
        uv_drift["meanAbsoluteRGB"]<=8.0
    )
    authentic=(
        base_edge>=8.0 and
        changed["meanAbsoluteRGB"]>max(1.0,3*base_repeat["meanAbsoluteRGB"]) and
        changed["fractionPixelsDifferingByMoreThan2"]>.02
    )
    sharp=retention>=.65
    brightness=abs(brightness_delta)<=25.0 and stats["low0"]["nearWhiteRatio"]<=.08
    approved_for_art_review=consistent and authentic and sharp and brightness
    report={
        "schemaVersion":1,"type":"UE5_8_D3D12_two_saved_worlds_sharpness_comparison",
        "baseWorld":"/Game/Development/Terrain/Maps/L_DBA_CliffRock01LowWarpBaselineReview",
        "lowWarpWorld":"/Game/Development/Terrain/Maps/L_DBA_CliffRock01LowWarpVariantReview",
        "sourceHashes":sources,"leftROI":list(ROI),"UVControlROI":list(CONTROL),
        "stats":stats,"baselineRepeatPixelDiff":base_repeat,
        "lowWarpRepeatPixelDiff":low_repeat,
        "materialDifferenceWithinSameActorROI":changed,
        "unchangedUVControlPixelDiffAcrossProcesses":uv_drift,
        "textureDetailRetentionRatio":retention,"brightnessDifference":brightness_delta,
        "qualityGates":{
            "sameWorldRepeatStable":consistent,
            "materialDifferenceActuallyCapturedAndBaseCrisp":authentic,
            "sharpnessRetentionAtLeast65Percent":sharp,
            "noExcessBrightness":brightness,
            "canProceedToManualArtReview":approved_for_art_review
        },
        "gatesZh":"基准边缘梯度至少8；低扰动相对基准>=65%；两个不同UE世界像素必须存在真实变化；相同右侧UV板跨进程稳定。",
        "userManuallyApproved":False,
        "limitsZh":"只代表独立QA地图GPU像素；不代表Village真实悬崖、天气覆盖、GPU帧时预算或Client/Server Cook。"
    }
    if REPORT.exists():
        raise FileExistsError("前次GPU像素审核报告已存在，不覆盖")
    OUT.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")

    sheet=Image.new("RGB",(1280,490),"#172026")
    draw=ImageDraw.Draw(sheet)
    fontpath=Path("C:/Windows/Fonts/msyh.ttc")
    font=ImageFont.truetype(str(fontpath),21) if fontpath.exists() else ImageFont.load_default()
    for idx,(role,title) in enumerate((("base0","原无扰动WorldAligned"),("low0","新20cm低扰动WorldAligned"))):
        x=idx*640
        sheet.paste(Image.fromarray(left[role],"RGB"),(x+5,65))
        draw.text((x+12,18),title+" | 梯度="+str(stats[role]["edgeGradient"]),
                  font=font,fill="#e6f0f4")
    draw.text((15,452),"清晰度保持率="+str(retention)+", 重复校验="+str(consistent)+", 贴图差异可信="+str(authentic),
              font=font,fill="#b5c8ce")
    if REVIEW.exists():
        raise FileExistsError("独立QA人工预览已存在，禁止覆盖")
    sheet.save(REVIEW,"JPEG",quality=89,optimize=True)
    print("CLIFF_LOW_ISOLATED_GPU_QUALITY",json.dumps({
        "baselineGradient":base_edge,"lowGradient":stats["low0"]["edgeGradient"],
        "retention":retention,"changedRGB":changed["meanAbsoluteRGB"],
        "stable":consistent,"valid":authentic,"pass":approved_for_art_review},ensure_ascii=False))
    print("CLIFF_LOW_ISOLATED_GPU_REPORT_SAVED",REPORT)
    print("CLIFF_LOW_ISOLATED_GPU_IMAGE_SAVED",REVIEW)
    if not approved_for_art_review:
        raise RuntimeError("CliffRock低扰动仍未通过独立GPU清晰度/场景重复稳定门禁；不可批准")
    print("CLIFF_LOW_ISOLATED_GPU_SHARPNESS_GATE_PASS userApprovalPending=True")

if __name__=="__main__":
    main()
