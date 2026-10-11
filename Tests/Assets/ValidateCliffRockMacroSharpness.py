#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01 20米真实GPU岩石纹理的明度与细节保留对照门禁。

比较相同相机/1920×1080/20米立面/平衡灯光下的已验证原
WorldAligned三向材质与本次Macro变体；统一ROI与同一锐度算法，
避免通过两份报告不同指标采样范围制造虚假的性能/画面改善。
明度差小于30只是基础门禁；真正保留4K裂隙需WorldAligned与UV的
相邻梯度至少达到旧图的65%，否则报失败并保留证据要求重新GPU审核。
该工具只能复查真实UE5.8 SceneCapture PNG，不重新合成GPU画面；
报告先保存再抛异常，使未通过可复核；不能声称真人美术签收。
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
OLD=ROOT/"Game/Saved/Validation/TerrainGaea/CliffRock01/CliffRock01_20m_BalancedLights_ActualGPU.png"
NEW=ROOT/"Game/Saved/Validation/TerrainGaea/CliffRock01Macro20261011/CliffRock01Macro_20m_BalancedLights_ActualGPU.png"
REPORT=ROOT/"Game/Saved/Validation/TerrainGaea/CliffRock01Macro20261011/CliffRock01Macro_GPUSharpnessComparison.json"
REGIONS={
    "WorldAligned20m":(150,350,780,720),
    "UV20m":(900,350,1500,720)
}

def extract(file:Path)->dict:
    if not file.is_file():raise FileNotFoundError("UE GPU实际PNG不存在："+str(file))
    with Image.open(file) as im:
        if im.format!="PNG" or im.size!=(1920,1080):
            raise RuntimeError("旧新GPU图必须都是1920×1080真PNG，不接纳海报："+str(file))
        rgb=np.asarray(im.convert("RGB"),dtype=np.float32)
    result={}
    for name,(x1,y1,x2,y2) in REGIONS.items():
        image=rgb[y1:y2,x1:x2]
        luma=image[...,0]*.2126+image[...,1]*.7152+image[...,2]*.0722
        edge=np.abs(np.diff(luma,axis=0)).mean()+np.abs(np.diff(luma,axis=1)).mean()
        residual=np.abs(luma-(np.roll(luma,1,axis=0)+np.roll(luma,-1,axis=0)+
                              np.roll(luma,1,axis=1)+np.roll(luma,-1,axis=1))*.25)
        result[name]={
            "lumaMean":round(float(luma.mean()),3),
            "edgeGradient":round(float(edge),3),
            "highPassResidualMean":round(float(residual.mean()),3),
            "nearBlackFraction":round(float(np.mean(luma<12)),5),
            "nearWhiteFraction":round(float(np.mean(luma>250)),5)
        }
    return result

def main()->None:
    original=extract(OLD)
    updated=extract(NEW)
    rates={}
    for region in REGIONS:
        baseline=original[region]["edgeGradient"]
        now=updated[region]["edgeGradient"]
        rates[region]=round(now/max(baseline,.001),4)
    luminance_gap=abs(updated["WorldAligned20m"]["lumaMean"]-
                      updated["UV20m"]["lumaMean"])
    crispness_pass=min(rates.values())>=.65
    report={
      "kind":"same_roi_actual_UE5_8_D3D12_source_gpu_pixel_comparison",
      "baselineFile":str(OLD.relative_to(ROOT)),
      "macroFile":str(NEW.relative_to(ROOT)),
      "baselineSHA256":hashlib.sha256(OLD.read_bytes()).hexdigest(),
      "macroSHA256":hashlib.sha256(NEW.read_bytes()).hexdigest(),
      "regions":REGIONS,
      "baseline":original,
      "macro":updated,
      "sharpnessRetentionRatio":rates,
      "sharpnessGateMinRatio":.65,
      "brightnessGap":round(luminance_gap,3),
      "brightnessPass":luminance_gap<30,
      "crispnessPass":crispness_pass,
      "manualUserApproval":False,
      "recommendedNextIfFailZh":"先确认真实Editor的4K Texture Mip驻留与Shader预热，再决定调整Warp振幅；不要把亮度改善冒充3A细节提升",
    }
    if REPORT.exists():
        existing=json.loads(REPORT.read_text(encoding="utf-8"))
        if existing!=report:raise RuntimeError("保存的GPU清晰度对比报告已被修改，拒绝覆盖")
    else:
        REPORT.parent.mkdir(parents=True,exist_ok=True)
        REPORT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("CLIFF_MACRO_GPU_SHARPNESS_METRICS",
          "edgeRatios",rates,"brightnessGap",round(luminance_gap,3),
          "crispnessPass",crispness_pass)
    if not crispness_pass:
        raise RuntimeError("新宏观岩石GPU截图相比原版缺少细节：尚不可做3A正式发布")
    if luminance_gap>=30:
        raise RuntimeError("同灯光20m两块岩壁平均明度差仍大于30")


if __name__=="__main__":
    main()
