#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE雪苔三态真实GPU截图离线质量回归；拒绝全黑、低对比、同帧复用。

前置：在有D3D12图形RHI的UE5.8 Editor执行CaptureMossSnowGPUReview.py。
此处只核验真实像素差异及灰度统计；不会渲染、修改/美化截图。
人工视觉（雪色/坡度/苔藓自然度）和ProfileGPU仍须额外签收。
"""
from pathlib import Path
import argparse
import json

import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
DEFAULT=ROOT/"Game/Saved/Validation/MossSnowVisual_20261012"
STATES=("Clear","LightSnow","HeavySnow")

def main():
    parser=argparse.ArgumentParser(description="验证虚幻原生GPU三态雪苔截图")
    parser.add_argument("--directory",type=Path,default=DEFAULT)
    opts=parser.parse_args()
    images={}
    report={"platform":"Unreal Engine 5.8 GPU screenshot",
            "qualityMeaningZh":"只证明真实图片非黑帧且天气切换有像素差异，不能替代人工验收"}
    for state in STATES:
        path=opts.directory/("MossSnow_"+state+"_GPU_1280x720.png")
        if not path.is_file() or path.stat().st_size<30000:
            raise RuntimeError("该次实际GPU截图缺失/过小："+str(path))
        with Image.open(path) as im:
            if im.size!=(1280,720):
                raise RuntimeError("截图尺寸不符合1280x720："+str(path))
            image=np.asarray(im.convert("RGB"),dtype=np.float32)
        region=image[75:645,100:1180]
        mean=float(region.mean())
        std=float(region.std())
        if mean<12 or mean>247 or std<8:
            raise RuntimeError(f"{state}黑屏、全白或缺乏纹理：mean={mean:.2f},std={std:.2f}")
        images[state]=region
        report[state]={"bytes":path.stat().st_size,"mean":mean,"std":std}
        print("MOSS_SNOW_FRAME_PIXELS_OK",state,"mean",round(mean,2),"std",round(std,2))
    diff=float(np.mean(np.abs(images["Clear"]-images["HeavySnow"])))
    if diff<3:
        raise RuntimeError("Clear与HeavySnow截图像素差异不足，MPC雪覆盖可能未真实生效："+str(diff))
    report["clearHeavyMeanAbsoluteDifference"]=diff
    output=opts.directory/"MossSnowVisualPixelQA.json"
    if output.exists():
        raise FileExistsError("已存在视觉证据报告，不覆盖另一轮："+str(output))
    output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
    print("MOSS_SNOW_GPU_PIXEL_QA_PASS weather_pixel_difference",round(diff,2))

if __name__=="__main__":
    main()
