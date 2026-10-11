#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟多区域环境粒子源纹理生成器（项目层；不触碰已有桃花/枫叶等正式素材）。

输入：项目层AmbientNatureStyleCatalog.json（风格主目录）。
输出：透明背景的颜色与法线PNG、SHA-256清单和人工审核缩略图。
职责：只处理项目美术资源；不创建Niagara播放器/引擎二进制。
素材为**原创确定性绘制**，不是对当前会话图像原件的直接转存；若后续原图经安全
文件传输进入项目，应采用单独的源图替换审核流程，不得静默冒名替换。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

WORKSPACE=Path(__file__).resolve().parents[4]
ROOT=WORKSPACE/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/AmbientNature"
CATALOG=ROOT/"AmbientNatureStyleCatalog.json"
OUTPUT=ROOT/"Textures"
MANIFEST=ROOT/"GeneratedTextureManifest.json"
FAMILIES={"petal","leaf","floater","debris"}
SEED=20261011


def hex_rgb(value: str) -> np.ndarray:
    color=value.lstrip("#")
    if len(color)!=6:
        raise ValueError("颜色标识必须为6位十六进制："+str(value))
    return np.array([int(color[i:i+2],16) for i in (0,2,4)],dtype=np.float32)


def smooth_closed(points, detail=14):
    p=np.array(points,dtype=np.float32)
    verts=[]
    for i in range(len(p)):
        a,b,c,d=(p[(i-1)%len(p)],p[i],p[(i+1)%len(p)],p[(i+2)%len(p)])
        for k in range(detail):
            t=k/detail
            q=.5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)
            verts.append(tuple(np.clip(q,.035,.965)))
    return verts


def petal_points(name:str):
    if "Lotus" in name or "Magnolia" in name:
        return [(.50,.925),(.38,.82),(.28,.66),(.24,.47),(.27,.29),(.37,.13),
                (.49,.08),(.63,.13),(.73,.30),(.78,.47),(.73,.66),(.61,.83)]
    if "RedSpider" in name:
        return [(.49,.94),(.445,.69),(.36,.47),(.31,.24),(.39,.12),(.5,.20),
                (.58,.09),(.68,.20),(.65,.42),(.58,.63),(.535,.80)]
    if "Osmanthus" in name:
        return [(.49,.44),(.33,.24),(.19,.30),(.24,.48),(.10,.63),(.28,.73),
                (.47,.57),(.67,.80),(.81,.68),(.66,.48),(.80,.25),(.59,.23)]
    if "Rose" in name:
        return [(.5,.93),(.34,.81),(.19,.66),(.17,.46),(.24,.25),(.35,.19),
                (.43,.29),(.52,.16),(.63,.18),(.71,.27),(.80,.43),(.83,.60),
                (.71,.76),(.62,.85)]
    return [(.5,.91),(.37,.84),(.27,.73),(.20,.60),(.18,.44),(.23,.28),
            (.32,.16),(.43,.14),(.49,.205),(.535,.145),(.65,.155),(.76,.27),
            (.82,.43),(.79,.59),(.71,.72),(.62,.84)]


def leaf_points(name:str):
    if "WillowLeaf" in name or "PineNeedle" in name or "PalmLeaf" in name:
        width=.10 if "Pine" not in name else .028
        return [(.49,.94),(.50-width,.69),(.51-width*.85,.42),(.5-width*.3,.14),
                (.52,.07),(.5+width*.55,.28),(.5+width,.58),(.52,.88)]
    if "Birch" in name or "Poplar" in name:
        return [(.50,.92),(.37,.78),(.23,.62),(.22,.44),(.27,.24),
                (.42,.16),(.5,.11),(.60,.18),(.73,.29),(.80,.45),
                (.76,.65),(.63,.80)]
    if "Oak" in name:
        return [(.50,.94),(.37,.82),(.26,.74),(.28,.61),(.18,.54),(.23,.40),
                (.31,.39),(.31,.29),(.42,.22),(.50,.09),(.58,.22),(.69,.30),
                (.70,.41),(.79,.43),(.83,.56),(.72,.62),(.73,.74),(.61,.82)]
    if "Plane" in name:
        return [(.50,.95),(.46,.75),(.30,.84),(.33,.67),(.15,.62),(.25,.50),
                (.08,.36),(.32,.40),(.25,.24),(.42,.34),(.50,.10),(.61,.32),
                (.75,.23),(.67,.41),(.90,.36),(.76,.52),(.85,.64),(.67,.67),
                (.68,.84),(.54,.76)]
    return [(.50,.94),(.43,.80),(.26,.73),(.28,.60),(.14,.52),(.26,.43),
            (.20,.26),(.42,.33),(.50,.08),(.60,.33),(.79,.25),(.72,.43),
            (.86,.52),(.72,.62),(.75,.76),(.54,.80)]


def mask_for(style, size:int):
    name=style["styleId"]
    kind=style["category"]
    mask=Image.new("L",(size,size),0)
    painter=ImageDraw.Draw(mask)
    if kind=="petal":
        pts=smooth_closed(petal_points(name))
        painter.polygon([(int(x*size),int(y*size)) for x,y in pts],fill=255)
    elif kind=="leaf":
        pts=smooth_closed(leaf_points(name),detail=7 if name in ("PlaneLeaf","OakLeaf") else 14)
        painter.polygon([(int(x*size),int(y*size)) for x,y in pts],fill=255)
        if name=="PineNeedle":
            painter.line([(int(.37*size),int(.91*size)),(int(.44*size),int(.19*size))],
                         fill=215,width=max(3,size//42))
    elif name in ("DandelionSeed","WillowCatkin"):
        # 絮状柔点与细羽绒丝，非硬边缘叶片
        base_y=.80
        painter.line([(int(.52*size),int(.90*size)),(int(.50*size),int(.45*size))],
                     fill=195,width=max(2,size//170))
        for k in range(22):
            theta=k*2*math.pi/22
            x=.5+.22*math.cos(theta)
            y=.42+.22*math.sin(theta)
            painter.line([(int(.5*size),int(.44*size)),(int(x*size),int(y*size))],
                         fill=115,width=max(1,size//210))
            r=max(2,size//100)
            painter.ellipse(((x*size-r,y*size-r),(x*size+r,y*size+r)),fill=238)
    elif name=="Feather":
        pts=smooth_closed([(.47,.94),(.34,.80),(.24,.55),(.33,.29),(.54,.10),
                           (.68,.15),(.73,.36),(.65,.65),(.52,.84)])
        painter.polygon([(int(x*size),int(y*size)) for x,y in pts],fill=255)
    elif name in ("PaperFragment","Confetti"):
        pts=([(.20,.34),(.66,.21),(.81,.67),(.33,.81)] if name=="PaperFragment"
             else [(.26,.35),(.70,.31),(.74,.69),(.28,.71)])
        painter.polygon([(int(x*size),int(y*size)) for x,y in pts],fill=255)
    elif name in ("Ember","Firefly","Spore"):
        painter.ellipse((int(.35*size),int(.35*size),int(.65*size),int(.65*size)),fill=255)
    else:
        painter.ellipse((int(.24*size),int(.29*size),int(.74*size),int(.73*size)),fill=255)
    sigma=(1.0 if kind in ("petal","leaf","debris") else size*.015)
    return mask.filter(ImageFilter.GaussianBlur(sigma))


def paint_veins(style,size:int):
    vein=Image.new("L",(size,size),0)
    draw=ImageDraw.Draw(vein)
    name=style["styleId"]
    if style["category"]=="petal" and "Osmanthus" not in name:
        draw.arc((int(.31*size),int(.14*size),int(.69*size),int(.96*size)),
                 164,366,fill=85,width=max(1,size//260))
        for side in (-1,1):
            for k in range(10):
                t=(k+1)/12
                y=.89-.70*t
                x=.51 +.02*math.sin(math.pi*t)
                draw.line([(x*size,y*size),
                           ((x+side*.12*math.sin(math.pi*t))*size,(y-.08)*size)],
                          fill=52,width=max(1,size//410))
    elif style["category"]=="leaf":
        draw.line([(.50*size,.89*size),(.50*size,.12*size)],
                  fill=135,width=max(1,size//150))
        for k in range(1,11):
            y=(.88-k*.060)*size
            t=k/11
            reach=(.11+.15*math.sin(math.pi*t))*size
            for sign in (-1,1):
                draw.line([(.5*size,y),(.5*size+sign*reach,y-.077*size)],
                          fill=74,width=max(1,size//330))
    elif name=="Feather":
        draw.line([(.50*size,.91*size),(.54*size,.11*size)],fill=165,
                  width=max(1,size//190))
        for k in range(14):
            y=(.78-k*.045)*size
            for side in (-1,1):
                draw.line([(.52*size,y),((.52+side*.21)*size,y-.05*size)],
                          fill=85,width=max(1,size//400))
    return np.asarray(vein.filter(ImageFilter.GaussianBlur(1.1)),dtype=np.float32)/255.


def render(style,size:int):
    name=style["styleId"]
    cat=style["category"]
    alpha=np.asarray(mask_for(style,size),dtype=np.float32)/255.
    y,x=np.mgrid[0:size,0:size].astype(np.float32)
    x/=size-1
    y/=size-1
    color=hex_rgb(style["baseTint"])
    radial=np.clip(1.-np.sqrt(((x-.51)/.62)**2+((y-.5)/.60)**2),0,1)
    veins=paint_veins(style,size)
    up=np.clip((.75-y)/.8,0,1)
    if cat=="petal":
        light=.85+.20*radial+.08*up+.058*veins
        base=np.clip(color[None,None,:]*light[...,None]+(1-up[...,None])*14,0,255)
    elif cat=="leaf":
        light=.69+.28*radial+.11*up+.09*veins
        seasonal=1.+.12*np.sin(6*x+2*y)
        base=np.clip(color[None,None,:]*light[...,None]*seasonal[...,None]+veins[...,None]*11,0,255)
    elif name in ("Ember","Firefly"):
        alpha=alpha*(.28+.72*radial)
        base=np.clip(color[None,None,:]*(1.0+.13*radial[...,None]),0,255)
    else:
        light=.83+.21*radial+.07*veins
        base=np.clip(color[None,None,:]*light[...,None],0,255)
    rgba=np.empty((size,size,4),dtype=np.uint8)
    rgba[:,:,:3]=np.asarray(base,dtype=np.uint8)
    rgba[:,:,3]=np.uint8(np.clip(alpha*255,0,255))
    height=alpha*(.075+.10*radial+.045*veins+.02*np.cos(10*x+7*y))
    gy,gx=np.gradient(height)
    sensitivity=120. if cat=="petal" else 155.
    nx=np.clip(-gx*sensitivity,-.53,.53)
    ny=np.clip(-gy*sensitivity,-.53,.53)
    mag=np.sqrt(nx*nx+ny*ny+1.)
    normal=np.empty_like(rgba)
    normal[:,:,0]=np.uint8(np.clip((nx/mag+.999)*127.5,0,255))
    normal[:,:,1]=np.uint8(np.clip((ny/mag+.999)*127.5,0,255))
    normal[:,:,2]=np.uint8(np.clip((1/mag+.999)*127.5,0,255))
    normal[:,:,3]=rgba[:,:,3]
    return Image.fromarray(rgba,"RGBA"),Image.fromarray(normal,"RGBA")


def output_paths(style):
    stem="T_DBA_Ambient_"+style["styleId"]
    return [OUTPUT/(stem+"_Albedo_Source.png"),OUTPUT/(stem+"_Normal_Source.png")]


def generate(size:int,apply:bool):
    cat=json.loads(CATALOG.read_text(encoding="utf-8"))
    styles=[x for x in cat["styles"] if x["assetStatus"]=="planned" and x["category"] in FAMILIES]
    if not styles:
        raise RuntimeError("待创建资产列表为空")
    OUTPUT.mkdir(parents=True,exist_ok=True)
    specs=[]
    preview=Image.new("RGB",(1200,math.ceil(len(styles)/6)*225),(35,39,46))
    for i,style in enumerate(styles):
        albedo,normal=render(style,size)
        paths=output_paths(style)
        for path,image,kind in ((paths[0],albedo,"Albedo"),(paths[1],normal,"Normal")):
            if apply:
                if path.exists():
                    raise FileExistsError("拒绝覆盖用户/之前批次的源纹理："+str(path))
                image.save(path,optimize=True)
                digest=hashlib.sha256(path.read_bytes()).hexdigest()
            else:
                digest=""
            specs.append({
                "name":path.name,"species":style["styleId"],"type":kind,
                "sourceSRGB":kind=="Albedo",
                "recommendedCompression":"Default" if kind=="Albedo" else "Normalmap",
                "sha256":digest,
                "resolution":[size,size],
                "origin":"确定性原创程序绘制；不冒充对话生成的原始图集"
            })
        thumb=albedo.copy()
        thumb.thumbnail((170,170),Image.Resampling.LANCZOS)
        tile=Image.new("RGBA",(185,185),(48,52,58,255))
        tile.alpha_composite(thumb,((185-thumb.width)//2,(185-thumb.height)//2))
        preview.paste(tile.convert("RGB"),((i%6)*200+8,(i//6)*225+8))
        print("AMBIENT_SOURCE_GENERATED" if apply else "AMBIENT_SOURCE_PREVIEW",
              style["styleId"],size,flush=True)
    if apply:
        MANIFEST.write_text(json.dumps({
            "schemaVersion":1,"generator":"GenerateAmbientNatureTextures.py",
            "textures":specs,"count":len(specs),"styles":len(styles)
        },ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    thumbout=ROOT/("GeneratedContactSheet.png" if apply else "ProposedContactSheet.png")
    preview.save(thumbout,optimize=True)
    print("AMBIENT_TEXTURE_STAGE_DONE",len(styles),len(specs),"applied",apply,flush=True)


def verify():
    man=json.loads(MANIFEST.read_text(encoding="utf-8"))
    failures=[]
    for spec in man["textures"]:
        path=OUTPUT/spec["name"]
        if not path.exists():
            failures.append(spec["name"]+":Missing");continue
        if hashlib.sha256(path.read_bytes()).hexdigest()!=spec["sha256"]:
            failures.append(spec["name"]+":SHA256")
        with Image.open(path) as image:
            if image.size!=tuple(spec["resolution"]) or image.mode!="RGBA":
                failures.append(spec["name"]+":Format")
    if failures:
        raise RuntimeError("源纹理验证失败："+",".join(failures[:8]))
    print("AMBIENT_TEXTURE_VERIFIED",len(man["textures"]),flush=True)


if __name__=="__main__":
    arg=argparse.ArgumentParser()
    arg.add_argument("--mode",choices=("preview","apply","verify"),default="preview")
    arg.add_argument("--size",type=int,default=1024,choices=(512,1024,2048))
    params=arg.parse_args()
    if params.mode=="verify":
        verify()
    else:
        generate(params.size,params.mode=="apply")
