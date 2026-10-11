#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""自然环境RGBA纹理V2.1：宽阔柔光、透光渐变与方向性纹理，不引入高频颗粒/边缘锐化。
--preview只写Saved审核目录并生成联系表，绝不覆盖项目正式源纹理。
--apply仅在完整预览成功后备份全部原图、统一替换33类颜色及法线源PNG并更新SHA-256清单；
UE .uasset需单独运行Unreal真实导入/材质验证脚本完成，不能用此脚本伪造。
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import os
import shutil
from pathlib import Path

import numpy as np
from PIL import Image,ImageDraw,ImageFilter,ImageFont
from GenerateAmbientNatureTextures import mask_for,paint_veins,hex_rgb

WORKSPACE=Path(__file__).resolve().parents[4]
ROOT=WORKSPACE/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/AmbientNature"
SOURCE=ROOT/"Textures"
CATALOG=json.loads((ROOT/"AmbientNatureStyleCatalog.json").read_text(encoding="utf-8"))["styles"]
MANIFEST=ROOT/"GeneratedTextureManifest.json"
PREVIEW=WORKSPACE/"Saved/Validation/FallingFoliage/ExpandedLibrary/BeautyV21Preview"
BACKUP=WORKSPACE/"Saved/Validation/FallingFoliage/ExpandedLibrary/BeautyV21SourceBackup"
SIZE=1024
STYLE_CODES={
 "PearPetal":(.23,.13,.14), "CherryPetal":(.28,.17,.08), "PlumPetal":(.20,.19,.11),
 "MagnoliaPetal":(.16,.14,.09),"LotusPetal":(.19,.17,.08),"GenericPetalWhite":(.12,.15,.06)
}

def paint(style):
    kind=style["category"]
    name=style["styleId"]
    alpha=np.asarray(mask_for(style,SIZE),dtype=np.float32)/255.
    yy,xx=np.mgrid[:SIZE,:SIZE].astype(np.float32)
    x=xx/(SIZE-1)
    y=yy/(SIZE-1)
    veins=paint_veins(style,SIZE)
    seed=int.from_bytes(hashlib.sha256(name.encode("utf-8")).digest()[:2],"big")
    phase=(seed%100)*.037
    center=.51+.015*np.sin(3.4*y+phase)
    radial=np.clip(1.-np.sqrt(((x-center)/.44)**2+((y-.50)/.53)**2),0,1)
    transverse=(x-center)
    # 宽带褶皱（空间频率低），而非噪点。柔和漫反射随背面透光变化。
    broad_fold=np.sin(10.0*transverse+5*y+phase)*.5+.5
    curl=.3*transverse*transverse+.15*radial+ .06*np.sin(8*x+3*y+phase)
    near_edge=np.asarray(Image.fromarray(np.uint8(alpha*255)).filter(ImageFilter.MinFilter(15)),dtype=np.float32)/255.
    rim=np.clip(alpha-near_edge,0,1)
    base=hex_rgb(style["baseTint"])
    if kind=="petal":
        whiteflower=name in ("PearPetal","GenericPetalWhite","MagnoliaPetal")
        if whiteflower:
            # 象牙白以偏暖的茎部、略带粉紫的浅色边缘显现体积；白场仍存在明暗差。
            col=np.zeros((SIZE,SIZE,3),dtype=np.float32)
            gradient=np.clip((y-.15)/.78,0,1)
            col[:,:,0]=base[0] - 18*gradient + 3*radial - 4*transverse
            col[:,:,1]=base[1] - 22*gradient + 6*radial + 2*transverse
            col[:,:,2]=base[2] - 24*gradient + 5*radial + 2*transverse
            scalar=(.94+.05*radial+.045*broad_fold+.025*veins- .045*rim)
        else:
            gradient=np.clip((y-.14)/.79,0,1)
            light_root=1.0-gradient
            col=base[None,None,:]*(.92+.12*radial[...,None]+.11*gradient[...,None])
            col+=np.array([12,15,15],dtype=np.float32)[None,None,:]*gradient[...,None]
            scalar=(.94+.07*broad_fold+.04*veins-.03*rim)
        normalcurl=.10*curl+.13*radial+.04*veins
    elif kind=="leaf":
        autumn=name in ("DryLeaf","PlaneLeaf","OakLeaf","BirchLeaf","YellowLeaf","RedLeaf","PoplarLeaf")
        # 低频色斑表现叶片自然光照和轻微季节渐变，但不出现花纹碎噪点。
        flecks=np.sin(9*x+4*y+phase)*np.sin(6*y-2*x+phase)
        seasonal=.89+.10*radial +(.085 if autumn else .035)*flecks+.05*(.5-y)
        shade=(.02+.07*veins)
        col=base[None,None,:]*seasonal[...,None] + shade[...,None]*24
        if autumn:
            col[:,:,0]+=10*np.clip(y-.4,0,1)
            col[:,:,1]-=5*np.clip(y-.3,0,1)
        scalar=.95+.06*broad_fold+.05*veins-.05*rim
        normalcurl=.14*curl+.22*radial+.09*veins
    elif kind=="debris":
        col=base[None,None,:]*(.95+.08*radial[...,None]+.06*broad_fold[...,None])
        scalar=1.0
        normalcurl=.12*curl+.08*radial
    else:
        # 漂浮粒子保留形状，中央渐变边缘半透明。
        col=base[None,None,:]*(.93+.11*radial[...,None])
        scalar=.96+.06*radial
        normalcurl=.10*radial
        if name in ("WillowCatkin","DandelionSeed"):
            alpha=np.clip(alpha*2.45,0,1)  # 柳絮/蒲公英原图几乎没有Alpha>=128的可见像素。
    if isinstance(scalar,float):
        rgb=col*scalar
    else:
        rgb=col*scalar[...,None]
    color=np.empty((SIZE,SIZE,4),dtype=np.uint8)
    color[:,:,:3]=np.uint8(np.clip(rgb,0,255))
    color[:,:,3]=np.uint8(np.clip(alpha*255,0,255))
    # 柔和法线：没有碎噪点，避免旧塑料/砂砾感。
    h=normalcurl*alpha
    gy,gx=np.gradient(h)
    strength=160 if kind=="leaf" else 135
    nx=np.clip(-gx*strength,-.58,.58)
    ny=np.clip(-gy*strength,-.58,.58)
    norm=np.sqrt(1+nx*nx+ny*ny)
    n=np.empty_like(color)
    n[:,:,0]=np.uint8(np.clip((nx/norm+1)*127.5,0,255))
    n[:,:,1]=np.uint8(np.clip((ny/norm+1)*127.5,0,255))
    n[:,:,2]=np.uint8(np.clip((1/norm+1)*127.5,0,255))
    n[:,:,3]=color[:,:,3]
    return Image.fromarray(color,"RGBA"),Image.fromarray(n,"RGBA")


def run(mode):
    candidates=[s for s in CATALOG if s["assetStatus"]=="planned"]
    if len(candidates)!=33:
        raise RuntimeError("当前生成合同仅承接33种未更换原图的项目视觉风格")
    PREVIEW.mkdir(parents=True,exist_ok=True)
    stats=[]
    for style in candidates:
        color,normal=paint(style)
        for typ,img in (("Albedo",color),("Normal",normal)):
            name="T_DBA_Ambient_"+style["styleId"]+"_"+typ+"_Source.png"
            path=PREVIEW/name
            img.save(path,optimize=True)
            stats.append({"name":name,"species":style["styleId"],"type":typ,
                 "sourceSRGB":typ=="Albedo","recommendedCompression":
                     "Default" if typ=="Albedo" else "Normalmap",
                 "sha256":hashlib.sha256(path.read_bytes()).hexdigest(),
                 "resolution":[SIZE,SIZE],
                 "origin":"V2.1宽带渐变/连续法线/无高频噪点程序源图"})
        print("AMBIENT_V21_PREVIEW",style["styleId"],flush=True)
    count=len(candidates)
    contact=Image.new("RGB",(6*280,math.ceil(count/6)*240),(30,34,39))
    draw=ImageDraw.Draw(contact)
    font_file="C:/Windows/Fonts/msyh.ttc"
    font=ImageFont.truetype(font_file,16) if Path(font_file).exists() else ImageFont.load_default()
    for i,style in enumerate(candidates):
        rgba=Image.open(PREVIEW/("T_DBA_Ambient_"+style["styleId"]+"_Albedo_Source.png")).convert("RGBA")
        im=rgba.resize((188,188),Image.Resampling.LANCZOS)
        x=(i%6)*280
        y=(i//6)*240
        for j,bg in enumerate(((35,40,45),(221,223,220))):
            square=Image.new("RGBA",(92,92),bg+(255,))
            quarter=rgba.resize((88,88),Image.Resampling.LANCZOS)
            square.alpha_composite(quarter,(2,2))
            contact.paste(square.convert("RGB"),(x+8+j*96,y+14))
        draw.text((x+7,y+120),style["displayNameZh"],font=font,fill=(225,228,231))
        draw.text((x+7,y+155),style["styleId"],font=font,fill=(183,191,198))
    contact.save(PREVIEW/"BeautyV21ArtReview.png",optimize=True)
    if mode=="preview":
        print("AMBIENT_V21_PREVIEW_COMPLETE",count,len(stats))
        return
    if mode!="apply":
        raise ValueError("mode只能preview/apply")
    BACKUP.mkdir(parents=True,exist_ok=True)
    for prior in SOURCE.glob("T_DBA_Ambient_*_Source.png"):
        backup=BACKUP/prior.name
        if not backup.exists():shutil.copy2(prior,backup)
    backup_manifest=BACKUP/"GeneratedTextureManifest.json"
    if not backup_manifest.exists():shutil.copy2(MANIFEST,backup_manifest)
    for spec in stats:
        name=spec["name"]
        target=SOURCE/name
        temp=SOURCE/(name+".stageV21")
        shutil.copy2(PREVIEW/name,temp)
        os.replace(temp,target)
    MANIFEST.write_text(json.dumps({"schemaVersion":1,"generator":"GenerateAmbientNatureArtV21.py",
         "textures":stats,"count":len(stats),"styles":count,
         "artRevision":"2.1"},ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("AMBIENT_V21_SOURCE_APPLIED",count,len(stats),"backup",BACKUP,flush=True)


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--mode",choices=("preview","apply"),default="preview")
    run(parser.parse_args().mode)
