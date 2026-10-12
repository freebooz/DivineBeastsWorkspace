#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""通用痕迹可重复源贴图：五类×Albedo(带Alpha)、Normal、ORM，各1024²。
这是可运行的风格化开发纹理，不自称摄影级素材；真正UE Texture2D与Shader另行验收。
独立运行，禁止覆盖任意非相同字节的既有源文件；输出SHA256清单。
"""
from __future__ import annotations

import hashlib
import io
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/SourceArt/Marks"
MANIFEST = Path(__file__).with_name("SurfaceMarkSourceManifest.json")
SIZE = 1024
KINDS = ("Bullet", "Scorch", "Slash", "Fluid", "Crack")
PALETTE = {
    "Bullet": (53, 52, 52),
    "Scorch": (42, 36, 33),
    "Slash": (99, 100, 99),
    "Fluid": (30, 86, 104),  # 平台中立液体颜色；神兽联盟默认能量或墨迹
    "Crack": (40, 46, 49),
}

def smooth_noise(rng, cells):
    """稳定的宏/微随机场，无外部在线纹理依赖。"""
    values = rng.integers(0, 256, size=(cells, cells), dtype=np.uint8)
    image = Image.fromarray(values).resize((SIZE, SIZE), Image.Resampling.BICUBIC)
    return np.asarray(image, dtype=np.float32) / 255.0

def make_masks(kind, index):
    rng = np.random.default_rng(11039 + index * 97)
    yy, xx = np.mgrid[-1:1:complex(SIZE), -1:1:complex(SIZE)].astype(np.float32)
    coarse = smooth_noise(rng, 32) - 0.5
    detail = smooth_noise(rng, 128) - 0.5
    noise = coarse * 0.12 + detail * 0.035
    radial = np.sqrt((xx * (1.0 + noise)) ** 2 + (yy * (1.0 - noise)) ** 2)
    height = np.zeros_like(radial)
    alpha = np.zeros_like(radial)

    if kind == "Bullet":
        # 黑洞+剥落环，高度仅用于法线，表面几何不改变。
        pit = np.clip((0.24 - radial) / 0.055, 0, 1)
        ring = np.clip(1 - np.abs(radial - 0.31 - coarse * .045) / .105, 0, 1)
        scratches = np.clip(1 - radial / .68 + noise * 3, 0, 1) * .16
        alpha = np.clip(pit * .94 + ring * .67 + scratches, 0, 1)
        height = -pit * .60 + ring * .36
    elif kind == "Scorch":
        shape = radial + coarse * .26 + detail * .08
        alpha = np.clip((.84 - shape) / .29, 0, 1)
        alpha *= np.clip(.76 + coarse * .95 + detail * .38, .26, 1.0)
        height = -alpha * .12
    elif kind == "Slash":
        # 斜向切口、主痕与两侧磨损区，确保有可见方向。
        signed = (yy + 0.66 * xx - 0.11 * np.sin(xx * 7)) / 1.25
        along = np.clip(1 - np.abs(xx + yy * .08) / .8, 0, 1) ** .7
        trench = np.clip(1 - np.abs(signed) / (.023 + detail * .01), 0, 1)
        abrasion = np.clip(1 - np.abs(signed) / .135, 0, 1) * .31
        alpha = np.clip((trench * .9 + abrasion * np.clip(.45 + coarse * 3, 0, 1)) * along, 0, 1)
        height = -trench * .32 + np.clip(1 - np.abs(signed + .04) / .035, 0, 1) * along * .11
    elif kind == "Fluid":
        # 液滴散射，项目中可用能量墨迹替代红色血液。
        alpha = np.clip(np.exp(-((xx + .12) ** 2 / .19 + (yy - .07) ** 2 / .10)) * 1.2 - .1, 0, 1)
        for _ in range(44):
            cx, cy = rng.uniform(-.88,.88, 2)
            radius = rng.uniform(.011, .084)
            radius *= 1 if rng.random() < .83 else 1.6
            spot = np.exp(-((xx - cx) ** 2 + (yy - cy) ** 2) / (radius ** 2))
            alpha = np.maximum(alpha, np.clip((spot - .17) * rng.uniform(.62, 1.25), 0, 1))
        alpha *= np.clip(.90 + coarse * .24, .5, 1.0)
        height = alpha * .045
    elif kind == "Crack":
        # 中心辐射+三级分叉，非硬编码一条十字线。
        layer = Image.new("L", (SIZE, SIZE), 0)
        draw = ImageDraw.Draw(layer)
        centre = (SIZE // 2, SIZE // 2)
        for root_angle in np.linspace(0, 2*np.pi, 9, endpoint=False):
            root_angle += rng.uniform(-.18,.18)
            px, py = centre
            angle = root_angle
            for segment in range(14):
                angle += rng.uniform(-.29,.29)
                length = rng.uniform(14, 33)
                x2 = int(px + np.cos(angle) * length)
                y2 = int(py + np.sin(angle) * length)
                draw.line((px, py, x2, y2), fill=255, width=max(2, 8 - segment//3))
                if segment % 3 == 0:
                    branch_angle = angle + rng.choice([-1,1]) * rng.uniform(.35,.96)
                    bx,by = x2,y2
                    for depth in range(4):
                        nx = int(bx + np.cos(branch_angle) * rng.uniform(10,32))
                        ny = int(by + np.sin(branch_angle) * rng.uniform(10,32))
                        draw.line((bx,by,nx,ny),fill=180,width=max(1,4-depth))
                        bx,by=nx,ny
                px,py = x2,y2
        fractures = np.asarray(layer.filter(ImageFilter.GaussianBlur(1.4)), dtype=np.float32) / 255.0
        alpha = np.clip(fractures * (.86 + coarse * .2), 0, 1)
        height = -fractures * .35
    else:
        raise ValueError(kind)
    return np.ascontiguousarray(alpha), np.ascontiguousarray(height), coarse, detail

def encode(kind, masks):
    alpha, height, coarse, detail = masks
    base = np.asarray(PALETTE[kind], dtype=np.float32)
    illumination = np.clip(1 + coarse * .18 + detail * .08, .67, 1.17)
    rgb = np.clip(base[None,None,:] * illumination[:,:,None], 0, 255).astype(np.uint8)
    albedo = np.dstack((rgb, np.asarray(np.clip(alpha,0,1)*255,dtype=np.uint8)))
    gy,gx = np.gradient(height)
    nx = np.clip(-gx * 15, -1, 1)
    ny = np.clip(-gy * 15, -1, 1)
    nz = np.ones_like(nx)
    length = np.maximum(np.sqrt(nx*nx + ny*ny + nz*nz), .001)
    normal = np.stack(((nx/length*.5+.5)*255,(ny/length*.5+.5)*255,(nz/length*.5+.5)*255),axis=-1).clip(0,255).astype(np.uint8)
    roughness = {"Bullet":.85,"Scorch":.95,"Slash":.77,"Fluid":.38,"Crack":.86}[kind]
    ao = np.clip(1-alpha*.30,0,1)*255
    rough = np.clip(roughness + detail * .095,0,1)*255
    orm = np.stack((ao,rough,np.zeros_like(ao)),axis=-1).clip(0,255).astype(np.uint8)
    return {"Albedo": Image.fromarray(albedo), "Normal": Image.fromarray(normal),
            "ORM": Image.fromarray(orm)}

def run():
    SOURCE.mkdir(parents=True,exist_ok=True)
    records=[]
    for index,kind in enumerate(KINDS):
        for role,img in encode(kind,make_masks(kind,index)).items():
            name=f"T_GP_Mark_{kind}_{role}_Source.png"
            buffer=io.BytesIO()
            img.save(buffer,format="PNG",optimize=False,compress_level=6)
            data=buffer.getvalue()
            path=SOURCE/name
            if path.exists():
                if path.read_bytes()!=data:
                    raise RuntimeError("不覆盖不同字节的现有源图："+str(path))
                state="UNCHANGED"
            else:
                with path.open("xb") as out: out.write(data)
                state="CREATED"
            record={"kind":kind,"role":role,"path":path.relative_to(ROOT).as_posix(),
                    "sha256":hashlib.sha256(data).hexdigest(),"width":SIZE,"height":SIZE,"state":state}
            records.append(record)
            print("SURFACE_MARK_SOURCE",kind,role,state,record["sha256"])
    manifest={"schemaVersion":1,"purpose":"StylizedPreviewTextures","formalAAAApproved":False,
              "assetAuthoringRequired":True,"engine":"UE5.8","records":[{k:v for k,v in r.items() if k!="state"} for r in records]}
    data=(json.dumps(manifest,ensure_ascii=False,indent=2)+"\n").encode("utf8")
    if MANIFEST.exists():
        if MANIFEST.read_bytes()!=data:
            raise RuntimeError("现有源图清单不同，拒绝覆盖："+str(MANIFEST))
    else:
        with MANIFEST.open("xb") as f:f.write(data)
    print("SURFACE_MARK_SOURCE_MANIFEST",len(records),str(MANIFEST))

if __name__=="__main__":
    run()
