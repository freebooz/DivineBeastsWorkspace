#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""《神兽联盟》Rock写实候选：实际4K同源PBR图的视觉质量/纹理平铺审核图。

本工具只读下载的原始BaseColor、NormalDX、ORM、Height，不改变或替换真实扫描源。
通过Pillow生成三张带明确“候选待人工审核”的视觉证据：
1. 4通道贴图1K预览；2. BaseColor 2×2原图平铺，检验接缝/重复；
3. 老校准源与新扫描候选直接对比，附非UE光照估计（不能声称真实引擎渲染）。
并生成SHA/尺寸/法线丰富度/ORM粗糙度变化/接缝差异等JSON QA指标。
真实UE材质Shader/UMaterial/MI回读需要单独的UnrealEditor完成，不可由此冒充。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image,ImageDraw,ImageFont

ROOT=Path(__file__).resolve().parents[3]
SOURCE=(ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/"
        "DBAWorldPack_Village/SourceArt/Terrain/PBR/RockRealism01")
OLD=(ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/"
     "DBAWorldPack_Village/SourceArt/Terrain/PBR/Rock/"
     "T_DBA_Terrain_Rock_BaseColor_Source.png")
MANIFEST=SOURCE/"RockRealism01_SourceManifest.json"
OUT=ROOT/"Game/Saved/Validation/TerrainGaea/RockRealism01"
FONT=Path("C:/Windows/Fonts/msyh.ttc")
MAPS=("BaseColor","NormalDX","ORM","Height")

def load()->dict[str,Image.Image]:
    doc=json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
    if doc.get("sourceId")!="rock_face_04" or doc.get("sourceResolution")!="4k":
        raise ValueError("PBR原图来源Manifest或分辨率不正确")
    result={}
    for item in doc["maps"]:
        role=item["role"]
        if role not in MAPS or role in result:
            raise ValueError("四张同源PBR角色重复或非法："+role)
        path=(ROOT/item["localPath"]).resolve()
        if not path.is_relative_to(SOURCE.resolve()) or not path.is_file():
            raise FileNotFoundError("原图文件不存在："+str(path))
        if hashlib.sha256(path.read_bytes()).hexdigest()!=item["sha256"]:
            raise ValueError("真实扫描源SHA256与清单不一致："+str(path))
        with Image.open(path) as im:
            im.load()
            if im.format!="PNG" or im.size!=(4096,4096):
                raise RuntimeError("真实扫描源必须是4096×4096 PNG，未接受缩放或假4K："+str(path))
            result[role]=im.copy()
    if set(result)!=set(MAPS):
        raise ValueError("Rock扫描候选必须包含同源4图")
    return result

def font(size:int):
    return ImageFont.truetype(str(FONT),size) if FONT.exists() else ImageFont.load_default()

def text(draw,xy,message,size=25,fill="#f1f4ec"):
    draw.text(xy,message,font=font(size),fill=fill)

def card_canvas(width:int,height:int,title:str,subtitle:str)->Image.Image:
    output=Image.new("RGB",(width,height),"#131917")
    pen=ImageDraw.Draw(output)
    text(pen,(28,22),title,38)
    text(pen,(30,80),subtitle,21,"#9fb4a8")
    return output

def paste_label(canvas:Image.Image,img:Image.Image,pos:tuple[int,int],box:int,title:str,
                bottom:str)->None:
    draw=ImageDraw.Draw(canvas)
    x,y=pos
    canvas.paste(img.convert("RGB").resize((box,box),Image.Resampling.LANCZOS),(x,y+35))
    draw.rectangle((x,y+34,x+box,y+35+box),outline="#677d70",width=2)
    text(draw,(x,y),title,23)
    text(draw,(x,y+box+46),bottom,20,"#adbbb3")

def seam_stats(rgb:np.ndarray)->dict:
    a=rgb.astype(np.float32)
    dx=a[:,0,:]-a[:,-1,:]
    dy=a[0,:,:]-a[-1,:,:]
    # 用相邻内像素差做真实接缝的参考尺度，而不是绝对要求0（真实扫描纹理有细节）
    internal=np.concatenate(((a[:,1,:]-a[:,0,:]).reshape(-1),
                             (a[1,:,:]-a[0,:,:]).reshape(-1)))
    joined=np.concatenate((dx.reshape(-1),dy.reshape(-1)))
    seam_rmse=float(np.sqrt(np.mean(joined*joined)))
    inner_rmse=float(np.sqrt(np.mean(internal*internal)))
    return {
        "seamRmseRGB":round(seam_rmse,3),
        "referenceAdjacentRmse":round(inner_rmse,3),
        "seamVsAdjacentRatio":round(seam_rmse/max(inner_rmse,.001),3),
        "seamMaxRGB":round(float(np.max(np.abs(joined))),1),
    }

def main()->None:
    items=load()
    OUT.mkdir(parents=True,exist_ok=True)
    names={
        "BaseColor":"BaseColor（无光照基础色）",
        "NormalDX":"NormalDX（DirectX切线法线）",
        "ORM":"ORM（R遮蔽 / G粗糙度 / B金属度）",
        "Height":"Height（高度／位移源图）",
    }
    # 真实Height为16位灰度，Pillow直接convert("RGB")会将65535裁剪到255，
    # 造成错误的纯白预览；先按1%-99%百分位做仅用于审核的线性显示映射。
    def preview(role,image):
        if role=="Height":
            samples=np.asarray(image,dtype=np.float32)
            low,high=np.percentile(samples[::8,::8],[1,99])
            if high-low<256:
                raise RuntimeError("扫描高度图实际动态范围过窄，拒绝展示假白图")
            mapped=np.uint8(np.rint(np.clip((samples-low)/(high-low)*255,0,255)))
            return Image.fromarray(mapped,"L").convert("RGB").resize(
                (900,900),Image.Resampling.LANCZOS)
        return image.convert("RGB").resize((900,900),Image.Resampling.LANCZOS)

    previews={key:preview(key,value) for key,value in items.items()}
    # 素材真实通道四格：这些是源图，不是AI概念图或引擎PBR截图。
    sheet=card_canvas(1910,2070,
        "《神兽联盟》Rock 01 · 真正4K岩壁PBR源图",
        "Poly Haven Rock Face 04 · CC0扫描素材 · 四通道同一实物 · 仅候选 / 待人工审核")
    for i,role in enumerate(MAPS):
        x=34+(i%2)*948
        y=145+(i//2)*956
        paste_label(sheet,previews[role],(x,y),900,names[role],
                    f"原始4096×4096 · {role}")
    sheet.save(OUT/"01_RockRealism01_PBR4Maps_HeightCorrected.png",compress_level=6)

    # 用原始真实BaseColor缩至768后2×2重复，展示同纹理真实重复性。
    piece=items["BaseColor"].convert("RGB").resize((768,768),Image.Resampling.LANCZOS)
    tiled=Image.new("RGB",(1536,1536))
    for y in (0,768):
        for x in (0,768):
            tiled.paste(piece,(x,y))
    sheet=card_canvas(1600,1680,"Rock 01 · 2×2 原纹理无缝检查",
        "四次重复同一真实基础色源图；未做人工缝合、未混合，检查明显重复/接缝")
    sheet.paste(tiled,(32,120))
    sheet.save(OUT/"02_RockRealism01_BaseColor2x2.png",compress_level=6)

    # 老技术校准源 vs 同源扫描；附真实Normal的简化离线漫反射预览。
    with Image.open(OLD) as prior:
        old=prior.convert("RGB").resize((720,720),Image.Resampling.LANCZOS)
    base=items["BaseColor"].convert("RGB").resize((720,720),Image.Resampling.LANCZOS)
    normal=np.asarray(items["NormalDX"].convert("RGB").resize((720,720),
                      Image.Resampling.LANCZOS),dtype=np.float32)/127.5-1
    normal=(normal/np.maximum(np.linalg.norm(normal,axis=2,keepdims=True),.005))
    sun=np.array([.39,-.31,.866],np.float32)
    sun=sun/np.linalg.norm(sun)
    diffuse=np.clip(np.sum(normal*sun,axis=2),0,1)
    shaded=np.asarray(base,dtype=np.float32)*(.37+.63*diffuse[...,None])
    shaded=Image.fromarray(np.uint8(np.rint(np.clip(shaded,0,255))),"RGB")
    comparison=card_canvas(2240,885,"Rock 01 · 旧技术样本 vs 实物扫描候选 vs 模拟方向光",
        "右侧仅是离线法线漫反射近似，不是UE5.8实时渲染；所有项目资产仍待美术人工审核")
    for i,(image,title) in enumerate(((old,"旧Rock校准色（平滑噪声）"),
                                      (base,"新Rock扫描基础色（真实裂纹）"),
                                      (shaded,"新扫描法线：近似光照示意"))):
        x=25+i*745
        paste_label(comparison,image,(x,126),720,title,"显示比例已缩小，用于比较物质形态")
    comparison.save(OUT/"03_RockRealism01_BeforeAfter.png",compress_level=6)

    compact={}
    for role,image in items.items():
        im=image
        if role=="Height":
            reduced=np.asarray(preview("Height",im),dtype=np.uint8)
        else:
            reduced=np.asarray(im.convert("RGB").resize((1024,1024),Image.Resampling.BILINEAR))
        metrics=seam_stats(reduced)
        metrics["originalMode"]=im.mode
        metrics["originalResolution"]=list(im.size)
        compact[role]=metrics
    orm=np.asarray(items["ORM"].convert("RGB").resize((1024,1024)),dtype=np.uint8)
    normal=np.asarray(items["NormalDX"].convert("RGB").resize((1024,1024)),dtype=np.uint8)
    metrics={
        "sourceAsset":"rock_face_04",
        "license":"CC0 1.0",
        "scanIsNotAutomaticallyApproved":True,
        "realEngineScreenshot":False,
        "maps":compact,
        "physics":{
            "aoMean":float(orm[:,:,0].mean())/255,
            "roughnessMean":float(orm[:,:,1].mean())/255,
            "roughnessStd":float(orm[:,:,1].std())/255,
            "metallicMax":int(orm[:,:,2].max()),
            "normalXYStd":float(normal[:,:,:2].std())/255,
            "normalDistinctColors":int(len(np.unique(normal[::16,::16,:].reshape(-1,3),axis=0))),
        },
        "knownRisksZh":[
            "真实岩壁扫描可有光照残留、叶片等区域特征，需要在项目桃林/绝壁场景中检查是否一致",
            "若2×2平铺出现宏观重复，应通过材质随机旋转、多尺度宏观遮罩或更换扫描源减弱，而不是直接采纳",
            "最终3A验收需实际UE材质球、真实Landscape坡面、光照和画质三档，当前是源纹理/离线参考审核",
        ]
    }
    if metrics["physics"]["normalDistinctColors"]<400 or metrics["physics"]["roughnessStd"]<.015:
        raise RuntimeError("真实扫描贴图物理通道变化不足，不允许进入材质审核")
    (OUT/"RockRealism01_SourceReviewMetrics_HeightCorrected.json").write_text(
        json.dumps(metrics,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("ROCK_REALISM_VISUAL_SOURCE_REVIEW_CREATED",OUT)
    print("ROCK_REALISM_4K_SOURCE_QA_PASS", "normalDistinct",
          metrics["physics"]["normalDistinctColors"],
          "roughnessStd",round(metrics["physics"]["roughnessStd"],4),
          "normalXYStd",round(metrics["physics"]["normalXYStd"],4),
          "seamRatio",metrics["maps"]["BaseColor"]["seamVsAdjacentRatio"])
    print("ROCK_REALISM_USER_ART_APPROVAL_PENDING")


if __name__=="__main__":
    main()
