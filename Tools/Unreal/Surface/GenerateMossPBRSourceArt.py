#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""通用苔藓PBR源图：生成可平铺PNG和哈希清单，不生成伪Unreal资产。"""
from pathlib import Path
import argparse
import hashlib
import json
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/SourceArt/Moss"
MANIFEST = Path(__file__).with_name("MossPBRSourceManifest.json")
FILES = {k: f"T_GP_Moss_{k.capitalize()}_Source.png"
         for k in ("albedo", "normal", "orm", "height", "coverage")}

def checksum(path):
    """内容摘要用于发现修改，不替代视觉审查。"""
    return hashlib.sha256(path.read_bytes()).hexdigest()

def encode(a):
    """线性数值转换为八位源图。"""
    return np.rint(np.clip(a,0,1)*255).astype(np.uint8)

def noise(n, cells, rng):
    """周期格点噪声，接缝连续。"""
    g = rng.random((cells,cells),dtype=np.float32)
    u = np.arange(n,dtype=np.float32)*cells/n
    i=np.floor(u).astype(np.int32)
    t=u-i; t=t*t*(3-2*t)
    a,b,x,y=i%cells,(i+1)%cells,t[None,:],t[:,None]
    return ((g[a[:,None],a[None,:]]*(1-x)+g[a[:,None],b[None,:]]*x)*(1-y)
           +(g[b[:,None],a[None,:]]*(1-x)+g[b[:,None],b[None,:]]*x)*y)

def fractal(n, freqs, weights, rng):
    """融合宏观苔藓团簇与细观叶丛。"""
    out=np.zeros((n,n),dtype=np.float32)
    for f,w in zip(freqs,weights):
        out += w*(noise(n,f,rng)-.5)
    return np.clip(.5+1.65*out,0,1)

def write(role, pixels):
    """平铺首尾闭合。"""
    pixels[-1]=pixels[0]
    pixels[:,-1]=pixels[:,0]
    path=OUT/FILES[role]
    if path.exists():
        with Image.open(path) as old:
            if not np.array_equal(np.asarray(old),pixels):
                raise FileExistsError("拒绝覆盖已有不同源图："+str(path))
    else:
        Image.fromarray(pixels).save(path,optimize=True)

def create():
    """纹理均属非金属植物表面，尺寸及随机种子固定便于复现。"""
    OUT.mkdir(parents=True,exist_ok=True)
    rng=np.random.default_rng(20261012)
    n=2048
    patch=fractal(n,(3,7,15,28),(.64,.43,.24,.14),rng)
    detail=fractal(n,(30,75,160),(.42,.28,.16),rng)
    fiber=fractal(n,(90,220,390),(.30,.20,.10),rng)
    density=np.clip((patch-.26)/.48,0,1)
    height=np.clip(.13+.61*density+.23*(detail-.5)+.11*(fiber-.5),0,1)

    # 基础色为低饱和森林绿；叶丛细节避免纯色贴片。
    color=np.stack((.17+.20*detail-.05*(1-density),
                    .26+.33*detail+.08*(fiber-.5),
                    .11+.16*detail),axis=-1)
    write("albedo",encode(color))
    # 切线空间法线强度偏低，以避免高频发白反射。
    dx=(np.roll(height,-1,1)-np.roll(height,1,1))*1.35
    dy=(np.roll(height,-1,0)-np.roll(height,1,0))*1.35
    inv=1/np.sqrt(dx*dx+dy*dy+1)
    write("normal",encode(np.stack((.5-.5*dx*inv,.5-.5*dy*inv,.5+.5*inv),axis=-1)))

    # ORM通道：R遮蔽、G粗糙度、B零金属度。
    ao=np.clip(.78+.20*height,0,1)
    rough=np.clip(.79+.11*(1-detail),.70,.94)
    write("orm",encode(np.stack((ao,rough,np.zeros_like(ao)),axis=-1)))
    write("height",encode(height))
    macro=fractal(1024,(3,6,12,25),(.73,.34,.20,.10),rng)
    write("coverage",encode(np.clip((macro-.24)/.63,0,1)))

    doc={"schemaVersion":1,"seed":20261012,"owner":"GamePlatformSurface",
         "maps":[{"role":k,"path":(OUT/v).relative_to(ROOT).as_posix(),
                  "sha256":checksum(OUT/v),
                  "resolution":[1024,1024] if k=="coverage" else [n,n],
                  "colorSpace":"sRGB" if k=="albedo" else "linear"}
                 for k,v in FILES.items()]}
    text=json.dumps(doc,ensure_ascii=False,indent=2)+"\n"
    if MANIFEST.exists() and MANIFEST.read_text(encoding="utf8")!=text:
        raise FileExistsError("已有不同的纹理清单，不覆盖")
    if not MANIFEST.exists(): MANIFEST.write_text(text,encoding="utf8")
    verify()

def verify():
    """复核来源、像素空间、ORM金属度及无缝性，结果不等于UE渲染通过。"""
    doc=json.loads(MANIFEST.read_text(encoding="utf8"))
    if doc["schemaVersion"]!=1 or len(doc["maps"])!=5:
        raise ValueError("苔藓源素材清单版本或数量不符")
    for entry in doc["maps"]:
        path=ROOT/entry["path"]
        if path.name not in FILES.values() or checksum(path)!=entry["sha256"]:
            raise ValueError("苔藓贴图摘要错误："+str(path))
        with Image.open(path) as im: a=np.asarray(im)
        if list(im.size)!=entry["resolution"] or not np.array_equal(a[0],a[-1]) or not np.array_equal(a[:,0],a[:,-1]):
            raise ValueError("苔藓贴图尺寸或边缘不符合无缝合同")
        if entry["role"]=="orm" and np.any(a[:,:,2]!=0):
            raise ValueError("苔藓非金属度必须为0")
        print("MOSS_SOURCE_VALID",entry["role"],entry["sha256"][:12])
    print("MOSS_SOURCE_QA_PASS maps=5")

if __name__=="__main__":
    parser=argparse.ArgumentParser(description="生成或只读检查通用苔藓PBR源图")
    group=parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--create",action="store_true")
    group.add_argument("--verify",action="store_true")
    args=parser.parse_args()
    verify() if args.verify else create()
