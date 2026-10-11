#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01世界三向坐标扰动的纹理周期重复对照（离线，不是假UE截图）。

直接读取已有真实青灰4K扫描BaseColor，根据AuthorCliffRock01MacroVariationUE
同样频率/位移设置模拟固定垂直平面（20m单位tile×2）的世界对齐二维截面。
左侧旧20m周期重复：每过2000cm得到相同裂隙；右侧宏观位移在世界空间
连续变化，周期可识别度降低。真实法线/ORM在Shader中共享同一扭曲世界坐标。
本脚本不修改用户源PNG/UMaterial或正式Village，只产生标注“离线模拟”
的视觉QA图、定量相关指标，不代替UE5.8 D3D12 SceneCapture和人工签收。
"""
from __future__ import annotations

import ast
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image,ImageDraw,ImageFont

ROOT=Path(__file__).resolve().parents[3]
GEN=ROOT/"Tools/Unreal/Terrain/AuthorCliffRock01MacroVariationUE.py"
SRC=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Terrain/PBR/CliffRock01/Derived/T_DBA_CliffRock01_BaseColor_BlueGray_4K_Source.png"
OUT=ROOT/"Game/Saved/Validation/TerrainGaea/CliffRock01Macro20261011"
WIDTH=1800
HEIGHT=1000
UNIT_CM=2000


def read_params()->tuple[np.ndarray,np.ndarray]:
    tree=ast.parse(GEN.read_text(encoding="utf-8"))
    found={}
    for statement in tree.body:
        if isinstance(statement,ast.Assign):
            for target in statement.targets:
                if isinstance(target,ast.Name) and target.id in ("FREQ","AMP"):
                    found[target.id]=ast.literal_eval(statement.value)
    if set(found)!={"FREQ","AMP"}:
        raise RuntimeError("没有找到与UE材质一致的宏观频率与位移幅度")
    return np.asarray(found["FREQ"],np.float32),np.asarray(found["AMP"],np.float32)


def bilinear(source:np.ndarray,u:np.ndarray,v:np.ndarray)->np.ndarray:
    h,w=source.shape[:2]
    x=np.mod(u*w,w)
    y=np.mod(v*h,h)
    x0=np.floor(x).astype(np.int32)%w
    y0=np.floor(y).astype(np.int32)%h
    x1=(x0+1)%w
    y1=(y0+1)%h
    dx=(x-x0).astype(np.float32)[...,None]
    dy=(y-y0).astype(np.float32)[...,None]
    a=source[y0,x0].astype(np.float32)*(1-dx)+source[y0,x1].astype(np.float32)*dx
    b=source[y1,x0].astype(np.float32)*(1-dx)+source[y1,x1].astype(np.float32)*dx
    return np.uint8(np.rint(np.clip(a*(1-dy)+b*dy,0,255)))


def main()->None:
    if not SRC.is_file():
        raise FileNotFoundError("尚未保存经来源审核的青灰色4K扫描基础色")
    freq,amp=read_params()
    with Image.open(SRC) as img:
        if img.size!=(4096,4096):
            raise RuntimeError("离线检视必须来自真实原始4K岩石纹理")
        source=np.asarray(img.convert("RGB").resize((1024,1024),Image.Resampling.LANCZOS))
    # 水平方向延伸40m（2个物理扫描周期），高度20m。
    world_y=np.linspace(0,4000,WIDTH,endpoint=False,dtype=np.float32)[None,:]
    world_z=np.linspace(0,2000,HEIGHT,endpoint=False,dtype=np.float32)[:,None]
    phase=freq[1]*world_y+freq[2]*world_z
    warp=np.sin(phase*2*math.pi)
    regular=bilinear(source,world_y/UNIT_CM+np.zeros_like(world_z),
                      world_z/UNIT_CM+np.zeros_like(world_y))
    warped=bilinear(source,(world_y+amp[1]*warp)/UNIT_CM,
                     (world_z+amp[2]*warp)/UNIT_CM)
    def similarity(pixels):
        first=pixels[:,:WIDTH//2,:].astype(np.float32)
        second=pixels[:,WIDTH//2:,:].astype(np.float32)
        return float(np.mean(np.abs(first-second))),float(np.corrcoef(
            first[::9,::9,:].ravel(),second[::9,::9,:].ravel())[0,1])
    regular_mae,regular_corr=similarity(regular)
    warped_mae,warped_corr=similarity(warped)
    if regular_corr<.999 or warped_corr>regular_corr-.02 or warped_mae<regular_mae+2:
        raise RuntimeError("低频坐标位移未显著减弱20m纹理周期重复")
    canvas=Image.new("RGB",(WIDTH*2+60,HEIGHT+110),"#12191c")
    canvas.paste(Image.fromarray(regular,"RGB"),(20,88))
    canvas.paste(Image.fromarray(warped,"RGB"),(WIDTH+40,88))
    drawer=ImageDraw.Draw(canvas)
    fontfile=Path("C:/Windows/Fonts/msyh.ttc")
    ft=ImageFont.truetype(str(fontfile),26) if fontfile.is_file() else ImageFont.load_default()
    drawer.text((20,17),"CliffRock01｜原20米世界投射：两段完全重复",font=ft,fill="#edf0ee")
    drawer.text((WIDTH+40,17),"CliffRock01｜低频世界坐标扰动：减弱周期裂隙",font=ft,fill="#edf0ee")
    drawer.text((20,55),"技术离线贴图截面模拟，非虚幻引擎GPU渲染或真实悬崖截图",font=ft,fill="#a1b4c1")
    drawer.line((20+WIDTH//2,88,20+WIDTH//2,88+HEIGHT),fill="#dfdc85",width=2)
    drawer.line((WIDTH+40+WIDTH//2,88,WIDTH+40+WIDTH//2,88+HEIGHT),fill="#dfdc85",width=2)
    OUT.mkdir(parents=True,exist_ok=True)
    imgout=OUT/"CliffRock01_Offline20mRepeatVsMacroWarp.png"
    reportpath=OUT/"CliffRock01_MacroWarpCycleMetrics.json"
    if imgout.exists() or reportpath.exists():
        raise FileExistsError("本轮审核证据已存在，禁止替换先前视觉图")
    canvas.save(imgout,"PNG",compress_level=6)
    report={
        "kind":"offline_2d_world_position_mapping_simulation_not_UE_render",
        "source":"verified_4096_cliff_rock_basecolor",
        "tileWidthCm":UNIT_CM,"terrainWidthCm":4000,"terrainHeightCm":2000,
        "regularRepeatMAE":round(regular_mae,3),
        "macroRepeatMAE":round(warped_mae,3),
        "regularRepeatCorrelation":round(regular_corr,5),
        "macroRepeatCorrelation":round(warped_corr,5),
        "macroFrequency":freq.tolist(),"macroAmplitudeCm":amp.tolist(),
        "realUEMapImageStillRequired":True,"userApprovalPending":True
    }
    reportpath.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("CLIFF_MACRO_OFFLINE_REPEAT_QA_PASS",
          "regularCorrelation",round(regular_corr,5),
          "warpedCorrelation",round(warped_corr,5),
          "regularMAE",round(regular_mae,3),
          "warpedMAE",round(warped_mae,3),
          "image",imgout.relative_to(ROOT))


if __name__=="__main__":
    main()
