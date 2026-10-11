#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟自然飘落物四类确定性源纹理创作器。
仅生成真正的PNG源纹理/预览，不伪造UE .uasset。--verify严格校验透明通道、尺寸及SHA-256。
使用固定种子生成桃花、枫叶、竹叶、银杏叶彩色/法线贴图，均配中文源资产说明。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[4]
DEST = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/FallingFoliage/Textures"
SIZE = 1024
EXPORT_SIZE = 2048
SEED = 20261011
KINDS = (
    ("Peach", "桃花花瓣", (245, 146, 173), 0.5),
    ("Maple", "秋季枫叶", (215, 78, 34), 1.0),
    ("Bamboo", "青翠竹叶", (76, 155, 65), 0.65),
    ("Ginkgo", "金色银杏叶", (227, 187, 66), 0.9),
)


def shape_points(kind: str) -> list[tuple[float, float]]:
    """归一化轮廓顶点；轴线从叶柄向叶梢，保持贴图边界有足够Alpha安全距离。"""
    if kind == "Peach":
        # 一片轻微心形豁口的椭圆桃花花瓣，上沿自然不对称。
        return [
            (0.50, 0.87), (0.36, 0.80), (0.23, 0.67), (0.18, 0.52),
            (0.16, 0.35), (0.23, 0.20), (0.34, 0.13), (0.45, 0.14),
            (0.50, 0.19), (0.54, 0.13), (0.67, 0.14), (0.79, 0.25),
            (0.84, 0.43), (0.79, 0.61), (0.67, 0.78),
        ]
    if kind == "Maple":
        return [
            (0.49, 0.94), (0.47, 0.74), (0.31, 0.78), (0.34, 0.65),
            (0.14, 0.62), (0.22, 0.52), (0.09, 0.39), (0.29, 0.41),
            (0.23, 0.26), (0.39, 0.34), (0.50, 0.09), (0.61, 0.34),
            (0.76, 0.26), (0.71, 0.41), (0.91, 0.38), (0.78, 0.52),
            (0.86, 0.61), (0.66, 0.65), (0.69, 0.78), (0.53, 0.74),
            (0.51, 0.94),
        ]
    if kind == "Bamboo":
        steps = np.linspace(0, 1, 85)
        left, right = [], []
        for t in steps:
            y = 0.92 - 0.82 * t
            cx = 0.52 + 0.13 * math.sin(math.pi * t) - 0.06 * t
            half = 0.125 * (math.sin(math.pi * t) ** 1.05)
            left.append((cx-half, y))
            right.append((cx+half, y))
        return left + right[::-1]
    # 银杏扇叶：从狭窄叶柄沿弧线展开，顶部呈缓和凹口。
    rim = []
    for angle in np.linspace(math.pi * 1.06, math.pi * 1.94, 120):
        ratio = (angle - math.pi * 1.06) / (math.pi * 0.88)
        radius = 0.38 * (1 + 0.018 * math.sin(18 * angle))
        x = 0.5 + math.cos(angle)*radius
        y = 0.63 + math.sin(angle)*radius * 1.1
        y += 0.032 * math.exp(-((ratio-0.5)/0.075)**2)
        rim.append((x, y))
    return [(0.49, 0.94), (0.48, 0.68)] + rim + [(0.52, 0.68), (0.51, 0.94)]


def veins_mask(kind: str) -> Image.Image:
    layer = Image.new("L", (SIZE, SIZE))
    d = ImageDraw.Draw(layer)
    if kind == "Peach":
        d.arc((340, 170, 680, 1080), 170, 355, fill=150, width=6)
        for i in range(11):
            y = 380 + i * 38
            spread = 90 * math.sin((i+1) / 12 * math.pi)
            d.line([(500, y + 110), (500 - spread, y - 20)], fill=75, width=2)
            d.line([(505, y + 90), (510 + spread, y - 5)], fill=75, width=2)
    elif kind == "Maple":
        base = (SIZE // 2, int(SIZE * .74))
        for x, y in [(0.50, .12), (.20, .40), (.12, .60), (.31, .71),
                     (.80, .39), (.87, .58), (.69, .73)]:
            d.line([base, (int(x*SIZE), int(y*SIZE))], fill=170, width=5)
            for j in range(1, 5):
                t = j/5
                px = int(base[0]*(1-t)+x*SIZE*t)
                py = int(base[1]*(1-t)+y*SIZE*t)
                d.line([(px,py),(px + (14 if x < .5 else -14), py-22)],fill=65,width=2)
    elif kind == "Bamboo":
        spine = [(int((0.52 + .13*math.sin(math.pi*t) - .06*t)*SIZE),
                  int((.92-.82*t)*SIZE)) for t in np.linspace(0,1,65)]
        d.line(spine,fill=180,width=5,joint="curve")
        for j in range(1,21):
            t=j/23
            cx=(.52+.13*math.sin(math.pi*t)-.06*t)*SIZE
            cy=(.92-.82*t)*SIZE
            half=.115*math.sin(math.pi*t)*SIZE
            d.line([(cx,cy),(cx-half,cy-11)],fill=70,width=2)
            d.line([(cx,cy),(cx+half,cy-14)],fill=70,width=2)
    else:
        for angle in np.linspace(math.pi*1.075, math.pi*1.925, 27):
            end=(int((.5+.38*math.cos(angle))*SIZE),
                 int((.63+.418*math.sin(angle))*SIZE))
            d.line([(SIZE//2,int(.80*SIZE)),end],fill=100,width=3)
    return layer.filter(ImageFilter.GaussianBlur(1.3))


def render(kind: str, base: tuple[int,int,int], height_amplitude: float) -> tuple[Image.Image,Image.Image]:
    rng = np.random.default_rng(SEED + sum(ord(c) for c in kind))
    mask = Image.new("L", (SIZE, SIZE))
    d = ImageDraw.Draw(mask)
    d.polygon([(round(x*(SIZE-1)),round(y*(SIZE-1))) for x,y in shape_points(kind)], fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(1.25))
    alpha = np.asarray(mask,dtype=np.float32)/255.
    yy,xx=np.mgrid[0:SIZE,0:SIZE].astype(np.float32)
    x=xx/(SIZE-1)
    y=yy/(SIZE-1)
    # 细节：叶肉纹理、叶柄方向渐变、边缘半透、脉络起伏。
    grain = rng.normal(0,1,(SIZE,SIZE)).astype(np.float32)
    radial = np.clip(1.0-np.sqrt(((x-.50)/.56)**2+((y-.51)/.64)**2),0,1)
    midrib = np.exp(-((x-.50-.05*np.sin(y*3.9))/.055)**2)
    veins = np.asarray(veins_mask(kind),dtype=np.float32)/255
    if kind == "Peach":
        shade = .75 + .19*radial + .10*(1-y) + .065*np.sin(17*y+11*x) + .025*grain
    elif kind == "Maple":
        shade = .68 + .25*radial + .12*(x+y) + .10*np.sin(13*x-8*y) + .055*grain
    elif kind == "Bamboo":
        shade = .63 + .22*radial + .14*x + .08*np.sin(18*y) + .035*grain
    else:
        shade = .72 + .24*radial + .14*(1-y) + .05*np.sin(25*x+3*y) + .035*grain
    shade=np.clip(shade + .055*veins, .3, 1.2)
    tint=np.array(base,dtype=np.float32)
    rgb=np.empty((SIZE,SIZE,4),dtype=np.uint8)
    for channel in range(3):
        # 花瓣/树叶高光是材质光照属性而非简单强光斑。
        rgb[:,:,channel]=np.clip((tint[channel]*shade) + 16*veins*height_amplitude + 5*midrib,0,255).astype(np.uint8)
    rgb[:,:,3]=(alpha*255).astype(np.uint8)
    color=Image.fromarray(rgb,"RGBA").resize((EXPORT_SIZE,EXPORT_SIZE),Image.Resampling.LANCZOS)

    h = alpha * height_amplitude * (.28*radial + .12*veins + .08*midrib)
    dy,dx=np.gradient(h)
    # 对移动网格采用弱法线扰动：N=(Nx,Ny,1)，保持光照方向稳定。
    nx=np.clip(-dx*110,-.45,.45)
    ny=np.clip(-dy*110,-.45,.45)
    nz=np.ones_like(nx)
    length=np.sqrt(nx*nx+ny*ny+nz*nz)
    nrgb=np.zeros_like(rgb)
    nrgb[:,:,0]=np.uint8(np.clip((nx/length*.5+.5)*255,0,255))
    nrgb[:,:,1]=np.uint8(np.clip((ny/length*.5+.5)*255,0,255))
    nrgb[:,:,2]=np.uint8(np.clip((nz/length*.5+.5)*255,0,255))
    nrgb[:,:,3]=np.uint8(alpha*255)
    normal=Image.fromarray(nrgb,"RGBA").resize((EXPORT_SIZE,EXPORT_SIZE),Image.Resampling.LANCZOS)
    return color,normal


def verify_manifest() -> None:
    manifest_path=DEST/"FallingFoliageSourceManifest.json"
    data=json.loads(manifest_path.read_text(encoding="utf-8"))
    if data["schemaVersion"] != 1 or len(data["textures"]) != 8:
        raise RuntimeError("素材清单版本或数量异常")
    for entry in data["textures"]:
        f=DEST/entry["name"]
        if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=entry["sha256"]:
            raise RuntimeError(f"素材缺失或内容已变化：{f}")
        with Image.open(f) as image:
            if image.size!=(EXPORT_SIZE,EXPORT_SIZE) or image.mode!="RGBA":
                raise RuntimeError(f"尺寸或模式不符：{f}")
            alpha=image.getchannel("A")
            if alpha.getextrema()!=(0,255):
                raise RuntimeError(f"Alpha通道无有效轮廓：{f}")
    print("FALLING_FOLIAGE_SOURCE_VERIFIED",len(data["textures"]),EXPORT_SIZE)


def main() -> None:
    parser=argparse.ArgumentParser()
    parser.add_argument("--apply",action="store_true",help="生成8张真实RGBA PNG，拒绝覆盖任何现有源图")
    parser.add_argument("--verify",action="store_true",help="检查尺寸/通道/hash")
    args=parser.parse_args()
    if args.verify:
        verify_manifest()
        return
    if not args.apply:
        print("INSPECT_ONLY:",DEST,"四种叶片各一张Albedo+Normal纹理，2048x2048")
        return
    DEST.mkdir(parents=True,exist_ok=True)
    targets=[f"T_DBA_Foliage_{kind}_{suffix}_Source.png"
             for kind,_,_,_ in KINDS for suffix in ("Albedo","Normal")]
    targets+=["FallingFoliageSourceManifest.json","FallingFoliageContactSheet.png"]
    existing=[t for t in targets if (DEST/t).exists()]
    if existing:
        raise FileExistsError("已有美术素材，拒绝覆盖："+",".join(existing))
    data={"schemaVersion":1,"seed":SEED,"resolution":[EXPORT_SIZE,EXPORT_SIZE],
          "origin":"本项目确定性程序原创素材","textures":[]}
    contact=Image.new("RGB",(EXPORT_SIZE,EXPORT_SIZE),(38,42,46))
    for idx,(kind,chinese,color,amplitude) in enumerate(KINDS):
        albedo,normal=render(kind,color,amplitude)
        for suffix,img,srgb,compression in (
            ("Albedo",albedo,True,"Default"),("Normal",normal,False,"Normalmap")):
            name=f"T_DBA_Foliage_{kind}_{suffix}_Source.png"
            img.save(DEST/name,optimize=True)
            data["textures"].append({"name":name,"type":suffix,"species":kind,
                                     "descriptionZh":chinese,"sourceSRGB":srgb,
                                     "recommendedCompression":compression,
                                     "sha256":hashlib.sha256((DEST/name).read_bytes()).hexdigest()})
            print("GENERATED_TEXTURE",name,img.size)
        # 纹理接触图只用于人工快速审核，不导入正式游戏。
        checker=Image.new("RGBA",(EXPORT_SIZE//2,EXPORT_SIZE//2),(60,65,69,255))
        checker.alpha_composite(albedo.resize(checker.size,Image.Resampling.LANCZOS))
        contact.paste(checker.convert("RGB"),((idx%2)*(EXPORT_SIZE//2),(idx//2)*(EXPORT_SIZE//2)))
    contact.save(DEST/"FallingFoliageContactSheet.png")
    (DEST/"FallingFoliageSourceManifest.json").write_text(
        json.dumps(data,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    verify_manifest()
    print("FALLING_FOLIAGE_TEXTURE_STAGE_COMPLETED")


if __name__=="__main__":
    main()
