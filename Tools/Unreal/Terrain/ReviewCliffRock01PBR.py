#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01裸露青灰悬崖岩石4K PBR源数据的独立可视审核与量化检查。

输入仅使用已签Poly Haven官方原始四图和独立青灰BaseColor衍生，
绝不从颜色图生成假法线/粗糙度/位移；原始四通道保持不变。
生成三种可审查图片：
- 四种PBR通道概览（16位Height用仅用于预览的归一化，无损原图不改）；
- 2×2真实颜色重复铺贴（检查大尺度重复与原图四边）；
- 原始米灰岩/青灰派生/法线单方向离线近似光照（**不是UE真正Shader截图**）。
还有数据报告：4K分辨率、颜色偏向、法线变化、ORM通道与边缘接缝。
脚本无UE Asset修改、无正式场景写入；人工作品签收保持待用户确认。
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[3]
SOURCE=(ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/"
        "DBAWorldPack_Village/SourceArt/Terrain/PBR/CliffRock01")
MANIFEST=SOURCE/"CliffRock01_SourceManifest.json"
GRADE=SOURCE/"CliffRock01_ColorGradeManifest.json"
OUT=ROOT/"Game/Saved/Validation/TerrainGaea/CliffRock01"
ROLES=("BaseColor","NormalDX","ORM","Height")
TITLES={
 "BaseColor":"青灰色BaseColor（无光照基础色）",
 "NormalDX":"NormalDX（DirectX真实扫描法线）",
 "ORM":"ORM（R遮蔽/G粗糙度/B金属）",
 "Height":"Height（16位高度/位移）",
}

def asset_files()->dict[str,Path]:
    meta=json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
    grade=json.loads(GRADE.read_text(encoding="utf-8-sig"))
    originals={}
    for rec in meta["maps"]:
        path=(ROOT/rec["localPath"]).resolve()
        if not path.is_relative_to(SOURCE.resolve()) or not path.is_file():
            raise FileNotFoundError("真实扫描PNG缺失："+str(path))
        if hashlib.sha256(path.read_bytes()).hexdigest()!=rec["sha256"]:
            raise RuntimeError("原始同源贴图SHA不一致："+str(path))
        originals[rec["role"]]=path
    derived=(ROOT/grade["gradedSourcePath"]).resolve()
    if not derived.is_relative_to(SOURCE.resolve()) or not derived.is_file():
        raise FileNotFoundError("项目青灰版BaseColor缺失")
    if hashlib.sha256(derived.read_bytes()).hexdigest()!=grade["gradedBaseColorSHA256"]:
        raise RuntimeError("项目青灰色4K基础色SHA不一致")
    return {**originals,"OriginalBaseColor":originals["BaseColor"],"BaseColor":derived}

def font(size:int)->ImageFont.ImageFont:
    path=Path("C:/Windows/Fonts/msyh.ttc")
    return ImageFont.truetype(str(path),size) if path.is_file() else ImageFont.load_default()

def canvas(w:int,h:int,title:str,info:str)->Image.Image:
    out=Image.new("RGB",(w,h),"#141919")
    d=ImageDraw.Draw(out)
    d.text((28,19),title,font=font(33),fill="#f2f3ee")
    d.text((29,70),info,font=font(21),fill="#a5b4bd")
    return out

def thumb(path:Path,res:int=870)->Image.Image:
    with Image.open(path) as src:
        src.load()
        if src.size!=(4096,4096):
            raise RuntimeError("必须是原始4K源材质而非放大的小图："+str(path))
        if path.name.endswith("_Height_4K_Original.png"):
            raw=np.asarray(src)
            if raw.dtype!=np.uint16:
                raise RuntimeError("原始高度PNG不是真正16位："+str(path))
            small=Image.fromarray(raw,"I;16").convert("I").resize((res,res),Image.Resampling.BILINEAR)
            values=np.asarray(small).astype(np.float32)
            low,high=np.percentile(values[::8,::8],[1,99])
            if high-low<250:
                raise RuntimeError("真实高度图动态范围太小，需复核扫描")
            display=np.uint8(np.rint(np.clip((values-low)/(high-low)*255,0,255)))
            return Image.fromarray(display,"L").convert("RGB")
        return src.convert("RGB").resize((res,res),Image.Resampling.LANCZOS)

def title_cell(img:Image.Image,position:tuple[int,int],piece:Image.Image,
               label:str,footer:str)->None:
    d=ImageDraw.Draw(img)
    x,y=position
    img.paste(piece,(x,y+37))
    d.rectangle((x,y+36,x+piece.width,y+37+piece.height),outline="#6e878f",width=2)
    d.text((x,y),label,font=font(24),fill="#e4f0ed")
    d.text((x,y+piece.height+49),footer,font=font(20),fill="#a8b4b8")

def edge_stats(array:np.ndarray)->dict:
    a=array.astype(np.float32)
    seam=np.concatenate((a[:,0,:]-a[:,-1,:],a[0,:,:]-a[-1,:,:]),axis=0)
    local=np.concatenate((a[:,1,:]-a[:,0,:],a[1,:,:]-a[0,:,:]),axis=0)
    edge=float(np.sqrt(np.mean(seam*seam)))
    local_rms=float(np.sqrt(np.mean(local*local)))
    return {
        "seamRmsRgb":round(edge,3),
        "adjacentLocalRmsRgb":round(local_rms,3),
        "seamRatioVsLocal":round(edge/max(local_rms,.01),3),
    }

def main()->None:
    paths=asset_files()
    OUT.mkdir(parents=True,exist_ok=True)
    previews={role:thumb(paths[role]) for role in ROLES}

    four=canvas(1820,1940,"神兽联盟 CliffRock01 · 4K同源PBR原图审核",
        "青灰项目版BaseColor + 官方NormalDX/ARM/Height；Height仅显示用归一化")
    for i,role in enumerate(ROLES):
        x=29+(i%2)*902
        y=121+(i//2)*897
        title_cell(four,(x,y),previews[role],TITLES[role],
                   "4096×4096｜PBR源图，不是UE截图")
    four.save(OUT/"01_CliffRock01_PBR4Maps.png",compress_level=6)
    print("CLIFFROCK_VISUAL_CONTACT_SAVED",OUT/"01_CliffRock01_PBR4Maps.png")

    tiled=canvas(1700,1680,"CliffRock01 · 2×2青灰裸岩平铺审核",
        "四次重复同一源材质，不缝合不修饰；重点看边线与大尺度重复")
    patch=thumb(paths["BaseColor"],res=780)
    for oy in (0,780):
        for ox in (0,780):
            tiled.paste(patch,(70+ox,105+oy))
    tiled.save(OUT/"02_CliffRock01_BaseColor2x2.png",compress_level=6)

    original=thumb(paths["OriginalBaseColor"],res=690)
    cool=thumb(paths["BaseColor"],res=690)
    normals=np.asarray(thumb(paths["NormalDX"],res=690),dtype=np.float32)/127.5-1
    normals=normals/np.maximum(np.linalg.norm(normals,axis=2,keepdims=True),1e-4)
    sun=np.asarray([.4,-.28,.87],np.float32)
    sun=sun/np.linalg.norm(sun)
    intensity=np.clip((normals*sun).sum(axis=2),0,1)
    # 离线Lambert参考：不包含真正UE后处理、PBR天空/反射，禁止冒充真实GPU渲染。
    lit=np.asarray(cool,dtype=np.float32)*(.4+.6*intensity[...,None])
    litimg=Image.fromarray(np.uint8(np.rint(np.clip(lit,0,255))),"RGB")
    comparison=canvas(2150,845,"CliffRock01 · 色彩变体和法线细节审核",
        "左：官方米灰岩；中：青灰色变体；右：简化法线方向光模拟（非UE真实渲染）")
    for i,(image,label) in enumerate((
        (original,"官方原始米灰岩"),(cool,"独立青灰色基础色"),(litimg,"仅示意法线方向光"))):
        title_cell(comparison,(18+i*710,103),image,label,"非最终悬崖场景")
    comparison.save(OUT/"03_CliffRock01_OriginalVsBlueGray.png",compress_level=6)

    dense={}
    for role in ROLES:
        with Image.open(paths[role]) as src:
            if role=="Height":
                height=np.asarray(src)
                depth_bits=16 if height.dtype==np.uint16 else 8
                if depth_bits!=16:
                    raise RuntimeError("Height原图不是保留原始16位")
                arr=np.asarray(previews[role],dtype=np.uint8)
            else:
                depth_bits=8
                arr=np.asarray(src.convert("RGB").resize((1024,1024),
                                Image.Resampling.BILINEAR),dtype=np.uint8)
        dense[role]={**edge_stats(arr),"sourceBitDepth":depth_bits,
                     "sourceDimensions":[4096,4096]}
    n=np.asarray(previews["NormalDX"],dtype=np.uint8)
    orm=np.asarray(previews["ORM"],dtype=np.uint8)
    cold=np.asarray(previews["BaseColor"],dtype=np.float32).mean((0,1))
    unique=int(np.unique(n[::10,::10,:].reshape(-1,3),axis=0).shape[0])
    rgh=float(orm[:,:,1].std())/255
    if unique<500 or rgh<.01:
        raise RuntimeError("岩壁扫描Normal/粗糙度缺少真实物理变化")
    if not cold[2]>cold[1]>cold[0]:
        raise RuntimeError("项目派生基础色并非稳定青灰色")
    metrics={
        "asset":"CliffRock01","sourceId":"marble_cliff_05",
        "sourceResolution":[4096,4096],
        "userApproved":False,"physicalScaleMeters":20,
        "normalDistinctColorsSmallSample":unique,
        "ORMRoughnessStd":round(rgh,4),
        "meanGradedBlueGrayRGB":np.round(cold,1).tolist(),
        "physicalPBRCharts":dense,
        "notRealUEGameScreenshot":True,
        "riskZh":"这是一张20米宽的无人造物裸岩扫描；审查重影、宏观重复、露天高亮色偏；UE真实立面三向投射与GPU效果还需独立验证",
        "sourceMapsUnchangedExceptDerivedBaseColor":True,
    }
    path=OUT/"CliffRock01_SourceVisualMetrics.json"
    data=(json.dumps(metrics,ensure_ascii=False,indent=2)+"\n").encode("utf-8")
    if path.exists() and path.read_bytes()!=data:
        raise RuntimeError("审核证据已存在不同版本，拒绝覆盖")
    if not path.exists():
        path.write_bytes(data)
    print("CLIFFROCK_VISUAL_PBR_SOURCE_QA_PASS", "normal_unique",unique,
          "roughness_std",round(rgh,4),
          "mean_color",np.round(cold,1).tolist(),
          "basecolor_seam_ratio",dense["BaseColor"]["seamRatioVsLocal"],
          "USER_APPROVAL_PENDING")

if __name__=="__main__":
    main()
