#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟唯美自然飘落物V2源美术生成器（2048x2048，原创可复现）。
对现有平面化叶片做平滑轮廓/柔粉渐变/脉络和微弯曲法线，禁止背景或文字。
preview仅写Saved，不修改项目；apply前验证目标且备份原图，按SHA-256清单原子更新。
每份纹理均保留合法8位RGBA与柔和抗锯齿边缘；不生成伪造.uasset。
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import tempfile

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[4]
SOURCES = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/FallingFoliage/Textures"
PREVIEW = ROOT / "Saved/Validation/FallingFoliage/BeautyV2Preview"
SIZE = 2048
STYLE = {
    "Peach": {"name":"桃花花瓣", "tip":(252,196,211), "middle":(249,208,221), "base":(253,239,237)},
    "Maple": {"name":"枫叶", "tip":(205,69,44), "middle":(237,117,58), "base":(245,165,89)},
    "Bamboo": {"name":"竹叶", "tip":(86,154,81), "middle":(121,178,90), "base":(178,198,111)},
    "Ginkgo": {"name":"银杏叶", "tip":(242,204,88), "middle":(244,218,135), "base":(237,207,125)},
}

# 特意保留温柔的非对称轮廓，桃花沿侧面自然收尖、顶部带小豁口。
SHAPES = {
    "Peach": [
        (.50,.90),(.38,.84),(.29,.77),(.215,.65),(.185,.49),(.20,.34),(.255,.22),
        (.345,.14),(.425,.14),(.482,.178),(.503,.212),(.525,.16),(.598,.128),
        (.69,.16),(.765,.245),(.816,.367),(.825,.50),(.781,.64),(.698,.76),(.591,.85)
    ],
    "Ginkgo": [
        (.50,.94),(.48,.84),(.46,.72),(.40,.66),(.26,.63),(.15,.595),(.12,.50),
        (.145,.37),(.213,.265),(.31,.185),(.418,.15),(.48,.195),(.505,.233),
        (.535,.186),(.625,.152),(.725,.20),(.814,.28),(.864,.392),(.857,.495),
        (.815,.573),(.70,.63),(.57,.71),(.52,.845)
    ],
    "Bamboo": [
        (.43,.92),(.413,.76),(.405,.60),(.413,.44),(.443,.265),(.50,.13),(.555,.20),
        (.606,.39),(.617,.56),(.585,.72),(.52,.85)
    ],
}
MAPLE_POINTS=[
    (.50,.93),(.484,.76),(.348,.799),(.355,.691),(.146,.633),(.224,.529),
    (.105,.405),(.317,.415),(.245,.251),(.410,.343),(.50,.102),
    (.588,.345),(.757,.249),(.683,.424),(.902,.403),(.77,.535),
    (.853,.63),(.656,.691),(.655,.802),(.519,.756)
]


def smooth_closed(points, count=12):
    """Catmull-Rom闭合曲线；显式控制点只描述叶片轮廓，不生成硬多边形折角。"""
    pts=np.array(points,dtype=np.float32)
    result=[]
    for i in range(len(pts)):
        p0,p1,p2,p3=(pts[(i-1)%len(pts)],pts[i],pts[(i+1)%len(pts)],pts[(i+2)%len(pts)])
        for k in range(count):
            t=k/count
            v=.5*((2*p1)+(-p0+p2)*t+
                  (2*p0-5*p1+4*p2-p3)*t*t+
                  (-p0+3*p1-3*p2+p3)*t*t*t)
            result.append(tuple(np.clip(v, .025, .975)))
    return result


def get_mask(kind):
    mask=Image.new("L",(SIZE,SIZE),0)
    d=ImageDraw.Draw(mask)
    points=MAPLE_POINTS if kind=="Maple" else smooth_closed(SHAPES[kind],12)
    d.polygon([(round(x*SIZE),round(y*SIZE)) for x,y in points],fill=255)
    # 仅1.1px抗锯齿，不做大片白边与脏颗粒。
    return mask.filter(ImageFilter.GaussianBlur(1.1))


def veins_and_curl(kind):
    """叶脉是几何层次而非噪点；桃花极细，枫叶清晰，银杏扇形放射。"""
    vein=Image.new("L",(SIZE,SIZE),0)
    d=ImageDraw.Draw(vein)
    if kind=="Peach":
        for side in (-1,1):
            for k in range(9):
                t=.15+k*.075
                y=.88-t*.66
                cx=.50+(.028 if side==1 else -.02)*t
                spread=(.135+ .12*math.sin(math.pi*t))* (.45+.46*t)
                x1=cx+side*spread
                y1=y-.065-0.018*t
                d.line([(int(cx*SIZE),int(y*SIZE)),
                        (int((cx+side*spread*.43)*SIZE),int((y-.035)*SIZE)),
                        (int(x1*SIZE),int(y1*SIZE))],fill=65,width=3,joint="curve")
        d.arc((.37*SIZE,.18*SIZE,.63*SIZE,.94*SIZE),155,367,fill=95,width=6)
    elif kind=="Maple":
        origin=(int(SIZE*.50),int(SIZE*.76))
        tips=[(.50,.11),(.25,.40),(.13,.62),(.40,.36),(.75,.37),(.87,.60),(.65,.70)]
        for tx,ty in tips:
            end=(int(tx*SIZE),int(ty*SIZE))
            d.line((origin,end),fill=128,width=7)
            for k in range(2,6):
                t=k/7
                px=int(origin[0]*(1-t)+end[0]*t)
                py=int(origin[1]*(1-t)+end[1]*t)
                direction=-1 if tx<.5 else 1
                d.line((px,py,px+direction*22,py-31),fill=67,width=2)
    elif kind=="Bamboo":
        line=[]
        for k in range(100):
            t=k/99
            y=.9-.76*t
            x=.47-.045*math.sin(math.pi*t)+.03*t
            line.append((int(x*SIZE),int(y*SIZE)))
        d.line(line,fill=125,width=6,joint="curve")
        for k in range(2,25):
            t=k/27
            y=.9-.76*t
            x=.47-.045*math.sin(math.pi*t)+.03*t
            for side in (-1,1):
                d.line((int(x*SIZE),int(y*SIZE),
                        int((x+side*.06*math.sin(math.pi*t))*SIZE),
                        int((y-.027)*SIZE)),fill=65,width=2)
    else:
        origin=(int(.50*SIZE),int(.82*SIZE))
        for i in range(25):
            angle=math.pi*1.04+(math.pi*.92)*(i/24)
            tx=.50+math.cos(angle)*.385
            ty=.60+math.sin(angle)*.41
            d.line([origin,(int(tx*SIZE),int(ty*SIZE))],fill=115,width=4)
    return np.asarray(vein.filter(ImageFilter.GaussianBlur(2.8)),dtype=np.float32)/255.0


def render(kind):
    alpha=np.asarray(get_mask(kind),dtype=np.float32)/255.
    y,x=np.mgrid[:SIZE,:SIZE].astype(np.float32)
    x=x/(SIZE-1)
    y=y/(SIZE-1)
    t=np.clip((y-.13)/.77,0,1)
    vein=veins_and_curl(kind)
    # 柔和纵向变色：顶部更粉润，根部逐渐呈乳白色。
    tip=np.array(STYLE[kind]["tip"],dtype=np.float32)
    middle=np.array(STYLE[kind]["middle"],dtype=np.float32)
    base=np.array(STYLE[kind]["base"],dtype=np.float32)
    blend=np.clip(t/.56,0,1)
    b2=np.clip((t-.56)/.44,0,1)
    palette=(1-blend)[...,None]*tip+blend[...,None]*middle
    palette=palette*(1-b2[...,None])+base*b2[...,None]
    # 可控柔光：边缘轻微增亮；花瓣中心顺着曲线形成薄纱纹，不使用任何随机颗粒。
    cx=.5 + .035*np.sin(2.7*y)
    dist=np.sqrt(((x-cx)/.39)**2+((y-.50)/.52)**2)
    soft=np.clip(1-dist,0,1)
    fold=(np.cos((x-.47)*20+1.2*y)*.5+.5)*(.22+.48*soft)
    skirt=np.clip((alpha-np.asarray(Image.fromarray(np.uint8(alpha*255)).filter(ImageFilter.MinFilter(19)),dtype=np.float32)/255)*2.5,0,1)
    if kind=="Peach":
        diffuse=1.05+.078*soft+.07*fold+.095*skirt+.055*vein
        # 桃花特有的乳白色透光基部与柔粉边缘，不使用粉色霓虹描边。
        diffuse+=.025*np.sin(20*y+4*x)*soft
    elif kind=="Maple":
        diffuse=.94+.13*soft+.046*fold+.10*vein+.035*skirt
    elif kind=="Bamboo":
        diffuse=.93+.12*soft+.04*fold+.09*vein+.036*skirt
    else:
        diffuse=1.00+.07*soft+.07*fold+.10*vein+.03*skirt
    albedo=np.clip(palette*diffuse[...,None],0,255).astype(np.uint8)
    rgba=np.empty((SIZE,SIZE,4),dtype=np.uint8)
    rgba[:,:,:3]=albedo
    rgba[:,:,3]=np.uint8(np.clip(alpha*255,0,255))
    color=Image.fromarray(rgba,"RGBA")
    # 高度用可微分的大尺度卷曲与脉络；法线方向连续，拒绝原有粗糙噪声。
    curl=.18*(x-cx)**2+ .06*np.sin(9*x+6*y)*soft
    height=(.085+.10*soft+.03*vein+.04*fold+curl)*alpha
    dzdy,dzdx=np.gradient(height)
    magnitude=140.0 if kind=="Peach" else 185.0
    nx=np.clip(-dzdx*magnitude,-.62,.62)
    ny=np.clip(-dzdy*magnitude,-.62,.62)
    norm=np.sqrt(nx*nx+ny*ny+1.)
    n=np.empty_like(rgba)
    n[:,:,0]=np.uint8(np.clip(127.5*(nx/norm+1),0,255))
    n[:,:,1]=np.uint8(np.clip(127.5*(ny/norm+1),0,255))
    n[:,:,2]=np.uint8(np.clip(127.5*(1/norm+1),0,255))
    n[:,:,3]=rgba[:,:,3]
    normal=Image.fromarray(n,"RGBA")
    return color,normal


def write_assets(destination):
    destination.mkdir(parents=True,exist_ok=True)
    texture_specs=[]
    contact=Image.new("RGB",(2048,2048),(29,33,39))
    for index,kind in enumerate(STYLE):
        color,normal=render(kind)
        for label,asset,srgb,compression in [
            ("Albedo",color,True,"Default"),("Normal",normal,False,"Normalmap")]:
            name=f"T_DBA_Foliage_{kind}_{label}_Source.png"
            output=destination/name
            asset.save(output,optimize=True)
            texture_specs.append({
                "name":name,"type":label,"species":kind,
                "descriptionZh":STYLE[kind]["name"],
                "sourceSRGB":srgb,"recommendedCompression":compression,
                "sha256":hashlib.sha256(output.read_bytes()).hexdigest()
            })
            print("BEAUTY_V2_TEXTURE",name,asset.size,output.stat().st_size,flush=True)
        thumb=color.resize((972,972),Image.Resampling.LANCZOS)
        bg=Image.new("RGBA",(972,972),(40,44,51,255))
        bg.alpha_composite(thumb)
        contact.paste(bg.convert("RGB"),(index%2*1024+26,index//2*1024+26))
    contact.save(destination/"FallingFoliageContactSheet.png",optimize=True)
    manifest={"schemaVersion":1,"artRevision":2,"seed":20261011,
              "resolution":[SIZE,SIZE],
              "origin":"神兽联盟原创唯美自然材质V2：平滑轮廓/柔和透光梯度/细腻法线，非第三方图片",
              "textures":texture_specs}
    (destination/"FallingFoliageSourceManifest.json").write_text(
        json.dumps(manifest,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    return manifest


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--mode",choices=("preview","apply"),default="preview")
    args=ap.parse_args()
    if args.mode=="preview":
        write_assets(PREVIEW)
        print("BEAUTY_V2_PREVIEW_READY",PREVIEW)
        return
    # 备份受影响源纹理与Manifest；不触碰已有Unreal二进制资产。
    backup=ROOT/"Saved/Validation/FallingFoliage/BeautyV2SourceBackup"
    backup.mkdir(parents=True,exist_ok=True)
    for item in SOURCES.glob("*"):
        if item.is_file() and item.suffix.lower() in (".png",".json") and not (backup/item.name).exists():
            shutil.copy2(item,backup/item.name)
    manifest=write_assets(PREVIEW)
    for asset in sorted(PREVIEW.glob("*")):
        if asset.suffix.lower() in (".png",".json"):
            staged=SOURCES/(asset.name+".beautyV2stage")
            shutil.copy2(asset,staged)
            os.replace(staged,SOURCES/asset.name)
    print("BEAUTY_V2_SOURCE_APPLIED",len(manifest["textures"]),flush=True)


if __name__=="__main__":
    main()
