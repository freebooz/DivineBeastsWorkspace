#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成《神兽联盟》多层地形的可复现技术PBR纹理及Gaea兼容校准高程/掩码。

职责/权限：Tools/Unreal/Terrain工程技术制作工具，产物归第三层DBAWorldPack_Village的SourceArt；
不创建.uasset/.umap，不假装真实Gaea 2.3导出，也不覆盖任一不相同的人工源贴图。
平台算法未来可复用，项目美术的10层名称仅作为第三层配置，不能加入GamePlatform源码硬编码。
Python依赖NumPy/Pillow；固定随机种子/世界空间/输入掩码，以Manifest SHA256做确定性复核。
源颜色sRGB、法线和ORM线性；非扫描级别的技术纹理仅用于建立UE材质与PCG管线。
--verify只读；默认生成；异常中止拒绝覆盖现有不同内容（所有输出先做写入预检）。
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
BASE = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Terrain"
MANIFEST = Path(__file__).with_name("TerrainSourceManifest_v1.json")
SEED = 20261011
TEXTURE_SIZE = 1024
CALIBRATION_SIZE = 1009

# 项目十种地表层：这里是项目内容配置，不能泄露到平台共享运行接口。
SURFACES = (
    ("Grass",       (76, 119, 63), .83, .24, "青绿草地"),
    ("ForestSoil",  (83, 75, 59), .89, .17, "森林腐殖土"),
    ("Dirt",        (126, 107, 84), .86, .16, "普通泥土"),
    ("Rock",        (105, 113, 114), .79, .18, "山体岩石"),
    ("Scree",       (129, 126, 120), .85, .23, "山地碎石"),
    ("Sand",        (184, 173, 144), .79, .13, "湖岸沙地"),
    ("Gravel",      (138, 136, 128), .84, .18, "河滩砾石"),
    ("RoadDirt",    (115, 106, 89), .88, .13, "压实土路"),
    ("FarmSoil",    (91, 77, 61), .91, .15, "耕作农田"),
    ("ShoreMud",    (99, 103, 91), .67, .14, "湖岸湿泥"),
)

# 本组语义只是Gaea兼容的校准Mask，不声称Gaea真实算法/导出版本。
MASKS = (
    ("Slope", "坡度归一遮罩"),
    ("Erosion", "侵蚀量示意遮罩"),
    ("Deposition", "沉积区示意遮罩"),
    ("RockExposure", "裸露岩石遮罩"),
    ("Flow", "水流汇聚带遮罩"),
    ("Shore", "湖岸界面带遮罩"),
    ("Forest", "林地适宜遮罩"),
    ("Buildable", "可建设/农田区域遮罩"),
    ("Exclusion", "人工道路/玩法排除遮罩"),
)

def periodic_noise(n: int, rng: np.random.Generator,
                   cells=(4, 11, 29, 71, 149)) -> np.ndarray:
    """双轴周期5次插值随机场；所有铺贴纹理必须可无缝重复。"""
    u = np.arange(n, dtype=np.float32)
    output = np.zeros((n, n), dtype=np.float32)
    weights = (0.43, .27, .17, .085, .045)
    for count, weight in zip(cells, weights):
        v = rng.random((count, count), dtype=np.float32)
        f = u * count / n
        lower = np.floor(f).astype(np.int32)
        t = f - lower
        t = t * t * t * (t * (t * 6 - 15) + 10)
        left = lower % count
        right = (lower + 1) % count
        a = v[left[:,None], left[None,:]]
        b = v[left[:,None], right[None,:]]
        c = v[right[:,None], left[None,:]]
        d = v[right[:,None], right[None,:]]
        output += weight * ((a * (1-t[None,:]) + b * t[None,:]) * (1-t[:,None]) +
                            (c * (1-t[None,:]) + d * t[None,:]) * t[:,None])
    return output

def clamp01(values: np.ndarray) -> np.ndarray:
    return np.clip(values, 0, 1)

def to_byte(values: np.ndarray) -> np.ndarray:
    return np.uint8(np.rint(clamp01(values) * 255))

def texture_images(layer: str, color: tuple[int,int,int],
                   roughness: float, micro: float, seed: int) -> tuple[Image.Image,Image.Image,Image.Image]:
    """基础颜色以完整中低频色块主导，微表面法线幅度较低、无人工锐化；所有贴图同尺寸。"""
    rng = np.random.default_rng(seed)
    broad = periodic_noise(TEXTURE_SIZE, rng, (3, 7, 13, 31, 67))
    fine = periodic_noise(TEXTURE_SIZE, rng, (9, 21, 47, 101, 197))
    variations = ((broad - .5) * .18 + (fine - .5) * .095).astype(np.float32)
    n = TEXTURE_SIZE
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    # 沙地低起伏风纹，农田低频田垄；频率为整数以保证水平无缝。
    if layer == "Sand":
        variations += np.sin(2*np.pi*(xx/n*17 + yy/n*5))*.035
    if layer == "FarmSoil":
        variations += np.sin(2*np.pi*(yy/n*22))*.035
    base = np.zeros((n, n, 3), dtype=np.float32)
    for channel, entry in enumerate(color):
        base[...,channel] = entry + 255 * variations * (.67 if channel == 0 else .60)
    base_img = Image.fromarray(np.uint8(np.rint(np.clip(base, 0, 255))), "RGB")
    height = ((broad-.5)*.60 + (fine-.5)*micro).astype(np.float32)
    dx = (np.roll(height,-1,axis=1)-np.roll(height,1,axis=1))* .23
    dy = (np.roll(height,-1,axis=0)-np.roll(height,1,axis=0))* .23
    norm = np.sqrt(dx*dx+dy*dy+1)
    normals = np.stack(((dx/norm*.5+.5)*255,
                        (dy/norm*.5+.5)*255,
                        (1/norm*.5+.5)*255), axis=-1)
    normal_img = Image.fromarray(np.uint8(np.rint(np.clip(normals, 0, 255))), "RGB")
    ao = clamp01(.92 + (broad-.5)*.12)
    rough = clamp01(roughness + (fine-.5)*.14)
    orm = np.stack((to_byte(ao),to_byte(rough),np.zeros((n,n), dtype=np.uint8)),axis=-1)
    return base_img, normal_img, Image.fromarray(orm, "RGB")

def calibration_masks() -> tuple[np.ndarray,dict[str,np.ndarray],dict[str,np.ndarray]]:
    """生成1009点16位校准地形+9语义mask+10个整数Weight图；非Gaea真实数据。"""
    n = CALIBRATION_SIZE
    y, x = np.mgrid[0:n, 0:n].astype(np.float32)
    u = (x/(n-1))*2-1
    v = (y/(n-1))*2-1
    # 岛屿轮廓、环山与湖心低地：平缓群系区适合测试岩/草/岸不同地貌阈值。
    radial = np.sqrt((u*.94)**2 + (v*1.04)**2)
    wave = .07*np.sin(7*u+3*v) + .05*np.cos(5*v-2*u)
    mountain = np.exp(-((u+.56)**2/.10+(v-.27)**2/.27))*.46
    bluff = np.exp(-((u-.62)**2/.14+(v+.49)**2/.21))*.52
    lake = np.exp(-((u-.03)**2/.085+(v-.12)**2/.068))*.43
    terrain = (.50 + .17*np.maximum(0,1.2-radial) + mountain + bluff - lake +
               wave * np.clip(radial,.1,1))
    terrain_norm = clamp01((terrain-.08)/1.08)
    height16 = np.uint16(np.rint(terrain_norm*65535))
    sy,sx=np.gradient(terrain_norm)
    slope=clamp01(np.sqrt(sx*sx+sy*sy)*78)
    lap = np.gradient(sx,axis=1) + np.gradient(sy,axis=0)
    erosion=clamp01(slope*.5 + np.abs(lap)*180)
    deposition=clamp01((1-slope)*(.9-terrain_norm)*1.6)
    rock=clamp01((slope-.24)*1.8 + (terrain_norm-.65)*2)
    # 平缓坡面与海拔共同决定岸带，中心湖的流径专门作为语义示例。
    shore=clamp01(1-np.abs(terrain_norm-.41)/.10)*clamp01(1-slope*.42)
    riverx = .08 + .10*np.sin(v*7)
    flow=clamp01(np.exp(-((u-riverx)/.075)**2)*(.40+(.75-terrain_norm)*.6))
    forest=clamp01((1-slope)*(.9-.24*np.abs(terrain_norm-.59))* (1-rock)*1.17)
    buildable=clamp01((1-slope*1.4)*np.exp(-((u+.39)**2/.29+(v+.28)**2/.31)))
    road=np.exp(-((v-(.04+.12*np.sin(u*4)))/.047)**2)
    exclusion=clamp01(road)
    masks={
        "Slope":slope,"Erosion":erosion,"Deposition":deposition,
        "RockExposure":rock,"Flow":flow,"Shore":shore,
        "Forest":forest,"Buildable":buildable,"Exclusion":exclusion,
    }
    # 十个候选原始正权重，PCG与地形共用同一地貌语义。
    candidates=[
        (1-slope*.6)*(.42+forest*.42)*(1-shore*.65)*(1-exclusion),
        forest*.60*(1-rock)*(1-exclusion),
        np.full((n,n), .18, dtype=np.float32),
        rock*1.7+slope*.75,
        clamp01(slope*.75+erosion*.55)*(1-shore),
        shore*.68*(1-flow*.4),
        deposition*.36+flow*.45,
        exclusion*2.0,
        buildable*.56*(1-exclusion),
        shore*.4+flow*.33,
    ]
    stack=np.stack([clamp01(a) for a in candidates],axis=0)
    total=np.sum(stack,axis=0)
    zero=total<1e-7
    stack[:,zero]=0
    stack[2,zero]=1
    total=np.sum(stack,axis=0)
    weights=np.uint8(np.floor((stack/total)*255))
    # floor总和<=255，余量归稳定的Dirt层，保证每个像素严格合计255。
    difference=255-weights.astype(np.int16).sum(axis=0)
    dirt=weights[2].astype(np.int16)+difference
    if np.any(dirt<0) or np.any(dirt>255):
        raise RuntimeError("权重归一会导致Dirt溢出，禁止发布校准数据")
    weights[2]=dirt.astype(np.uint8)
    if not np.all(weights.astype(np.uint16).sum(axis=0)==255):
        raise RuntimeError("校准地表所有权重没有按像素合计255")
    return height16, masks, {kind[0]:weights[i] for i,kind in enumerate(SURFACES)}

def png_bytes(image:Image.Image)->bytes:
    data=io.BytesIO()
    image.save(data,"PNG",compress_level=6,optimize=False)
    return data.getvalue()

def main():
    parser=argparse.ArgumentParser(description="神兽联盟十地表纹理与Gaea兼容测试掩码")
    parser.add_argument("--verify",action="store_true",help="只校验确定性源，不修改")
    args=parser.parse_args()
    # 正式PBR另行声明且已替换同名原图后，本历史技术生成器不再拥有PBR覆盖权限。
    prod=Path(__file__).with_name("TerrainProductionPBRManifest_v1.json")
    if prod.exists():
        if not args.verify:
            raise RuntimeError("正式CC0扫描PBR已经发布：禁止用技术校准工具覆盖正式纹理")
        history=json.loads(MANIFEST.read_text(encoding="utf-8"))
        official=json.loads(prod.read_text(encoding="utf-8"))
        if history.get("sourceKind")!="procedural_calibration_not_gaea" or len(official.get("files",[]))!=40:
            raise RuntimeError("技术地貌或正式扫描合同不完整")
        for item in history["files"]:
            if item["category"]=="PBR_TECHNICAL":
                continue  # 技术PBR是历史签名，迁移后不再与同名正式源图比较。
            path=ROOT/item["sourcePath"]
            if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=item["sha256"]:
                raise RuntimeError("地貌校准图签名发生变化："+str(path))
        for item in official["files"]:
            path=ROOT/item["sourcePath"]
            if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=item["sha256"]:
                raise RuntimeError("正式扫描源图签名发生变化："+str(path))
        print("TERRAIN_SOURCE_QA_PASS historical_geodata=20 production_pbr=40 mode=verify")
        return

    files=[]
    metadata=[]
    for i,(layer,rgb,rough,micro,description) in enumerate(SURFACES):
        bc,no,orm=texture_images(layer,rgb,rough,micro,SEED+101*i)
        for role,image,srgb,compression in (
            ("BaseColor",bc,True,"Default"),
            ("Normal",no,False,"Normalmap"),
            ("ORM",orm,False,"Masks")
        ):
            path=BASE/"PBR"/layer/("T_DBA_Terrain_"+layer+"_"+role+"_Source.png")
            asset="/DBAWorldPack_Village/Terrain/Textures/"+layer+"/T_DBA_Terrain_"+layer+"_"+role
            binary=png_bytes(image)
            files.append((path,binary))
            metadata.append({"sourcePath":path.relative_to(ROOT).as_posix(),"assetPath":asset,
                             "role":role,"layer":layer,"descriptionZh":description,
                             "size":[TEXTURE_SIZE,TEXTURE_SIZE],"mode":image.mode,
                             "sRGB":srgb,"compression":compression,
                             "sha256":hashlib.sha256(binary).hexdigest(),"category":"PBR_TECHNICAL"})
    height,masks,weights=calibration_masks()
    height_path=BASE/"Calibration"/"Height_1009_R16_Source.png"
    height_png=png_bytes(Image.fromarray(height,"I;16"))
    files.append((height_path,height_png))
    metadata.append({"sourcePath":height_path.relative_to(ROOT).as_posix(),
                     "category":"HEIGHT_R16_CALIBRATION","size":[CALIBRATION_SIZE]*2,"mode":"I;16",
                     "sha256":hashlib.sha256(height_png).hexdigest(),"noteZh":"程序地形校准样本，不是Gaea出口"})
    for name,arr in masks.items():
        path=BASE/"Calibration"/"Masks"/(name+"_Source.png")
        binary=png_bytes(Image.fromarray(to_byte(arr),"L"))
        files.append((path,binary))
        metadata.append({"sourcePath":path.relative_to(ROOT).as_posix(),"category":"GAEA_COMPATIBLE_SAMPLE_MASK",
                         "maskName":name,"size":[CALIBRATION_SIZE]*2,"mode":"L",
                         "sha256":hashlib.sha256(binary).hexdigest(),"noteZh":"语义掩码校准样本，不是Gaea"})
    for name,arr in weights.items():
        path=BASE/"Calibration"/"Weights"/("LS_"+name+"_Source.png")
        binary=png_bytes(Image.fromarray(arr,"L"))
        files.append((path,binary))
        metadata.append({"sourcePath":path.relative_to(ROOT).as_posix(),"category":"LANDSCAPE_WEIGHT",
                         "layer":name,"size":[CALIBRATION_SIZE]*2,"mode":"L",
                         "sha256":hashlib.sha256(binary).hexdigest()})
    doc={
        "schemaVersion":1,"generatedBy":"Tools/Unreal/Terrain/GenerateTerrainCalibrationPBR.py",
        "generatedSeed":SEED,"sourceKind":"procedural_calibration_not_gaea",
        "styleZh":"魔兽世界清晰地貌区与神兽联盟温润3A材质的技术校准样本；非最终3A扫描素材",
        "terrainSamples":[CALIBRATION_SIZE,CALIBRATION_SIZE],"pbrTextureSize":[TEXTURE_SIZE,TEXTURE_SIZE],
        "baseLayers":[x[0] for x in SURFACES],"maskNames":[x[0] for x in MASKS],
        "weightSum":255,"heightEncoding":"PNG 16bit grayscale I;16",
        "worldConvention":{"axisX":"positive_right","axisY":"positive_up","imageRows":"top_down",
                           "flipX":False,"flipY":True,"worldSizeCm":[100800.0,100800.0],
                           "minHeightCm":-10000.0,"maxHeightCm":20000.0},
        "projectUEMount":"/DBAWorldPack_Village","files":metadata,
    }
    manifest_bytes=(json.dumps(doc,ensure_ascii=False,indent=2)+"\n").encode("utf-8")
    planned=[*files,(MANIFEST,manifest_bytes)]
    # 完整预检后才逐一写新文件；已存在非相同内容立即拒绝。
    for path,raw in planned:
        if args.verify and not path.is_file():
            raise FileNotFoundError("校验需要真实文件："+str(path))
        if path.exists() and path.read_bytes()!=raw:
            raise RuntimeError("源纹理已经由人工修改，拒绝覆盖："+str(path))
    if not args.verify:
        for path,raw in planned:
            if not path.exists():
                path.parent.mkdir(parents=True,exist_ok=True)
                path.write_bytes(raw)
                print("TERRAIN_SOURCE_CREATED",path.resolve().relative_to(ROOT).as_posix(),len(raw))
    print("TERRAIN_SOURCE_QA_PASS","pbr_textures=30","masks="+str(len(masks)),
          "weight_layers="+str(len(weights)),"height=1",
          "files="+str(len(metadata)),"mode="+("verify" if args.verify else "generate"))


if __name__=="__main__":
    main()
