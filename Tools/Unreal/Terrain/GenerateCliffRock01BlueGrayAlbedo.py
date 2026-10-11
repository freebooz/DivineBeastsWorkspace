#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01（青灰绝壁）基础色可复现色彩校正；只改颜色，不动真实扫描物理通道。

项目层DBAWorldPack_Village美术资源。原始Marble Cliff 05扫描是米黄色，
按sRGB→线性空间→亮度保留/低饱和青灰系数→sRGB方法生成4K项目BaseColor。
不借概念图生成假Normal，不改nor_dx/arm/16位Displacement真实同源文件。
--preview使用官方1K候选预览生成仅供选型的青灰对照；--apply/--verify对
官方4K原始PNG与SHA256检查后生成派生BaseColor及独立manifest，禁止覆盖不同文件。
本色彩变体仍属于待人工签收候选，正式地图/发布配置不改。
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[3]
SOURCE=(ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/"
        "SourceArt/Terrain/PBR/CliffRock01")
ORIGINAL=SOURCE/"Original/T_DBA_CliffRock01_BaseColor_4K_Original.png"
DERIVED=SOURCE/"Derived/T_DBA_CliffRock01_BaseColor_BlueGray_4K_Source.png"
MANIFEST=SOURCE/"CliffRock01_ColorGradeManifest.json"
SOURCE_MANIFEST=SOURCE/"CliffRock01_SourceManifest.json"
PREVIEW=ROOT/"Game/Saved/Validation/TerrainGaea/CliffRockCandidateSources20261011/"
PREVIEW_FILE=PREVIEW/"marble_cliff_05_official_diff_1k.jpg"
PREVIEW_OUT=PREVIEW/"MarbleCliff05_BlueGray_GradeApprovalPreview.jpg"
COEFFICIENTS=np.asarray([.75,.87,1.01],dtype=np.float32)
CHROMA_RETENTION=np.float32(.08)

def sha(raw:bytes)->str:return hashlib.sha256(raw).hexdigest()

def check_manifest()->str:
    if not SOURCE_MANIFEST.is_file():
        raise FileNotFoundError("官方同源4K扫描Manifest未完成")
    source=json.loads(SOURCE_MANIFEST.read_text(encoding="utf-8-sig"))
    if source.get("sourceId")!="marble_cliff_05" or source.get("sourceResolution")!="4k":
        raise RuntimeError("CliffRock真实扫描来源身份/尺寸错误")
    rows=[x for x in source["maps"] if x["role"]=="BaseColor"]
    if len(rows)!=1:raise RuntimeError("原始BaseColor不存在或重复")
    original=ROOT/rows[0]["localPath"]
    if original.resolve()!=ORIGINAL.resolve() or not original.is_file():
        raise FileNotFoundError("原始扫描位置不符合项目层SourceArt")
    actual=sha(original.read_bytes())
    if actual!=rows[0]["sha256"]:
        raise RuntimeError("原始4K扫描发生变化，禁止导出项目色彩变体")
    return actual

def grade(im:Image.Image)->Image.Image:
    """遵循sRGB传递函数；保留原法线/高度决定的裂缝结构与原始亮度层次。"""
    image=im.convert("RGB")
    if image.width!=image.height or image.width not in (1024,4096):
        raise ValueError("仅支持当前真实1K源预览与4K原始扫描")
    output=Image.new("RGB",image.size)
    # 逐段处理以避免4K源图在UE编辑器同时工作时出现额外大内存峰值。
    for y in range(0,image.height,256):
        block=np.asarray(image.crop((0,y,image.width,min(y+256,image.height))),
                         dtype=np.float32)/255.0
        linear=np.where(block<=.04045,block/12.92,((block+.055)/1.055)**2.4)
        luma=(linear[...,0]*.2126+linear[...,1]*.7152+linear[...,2]*.0722)
        target=luma[...,None]*COEFFICIENTS + CHROMA_RETENTION*(linear-luma[...,None])
        target=np.clip(target,0,1)
        srgb=np.where(target<=.0031308,target*12.92,1.055*target**(1/2.4)-.055)
        output.paste(Image.fromarray(np.uint8(np.rint(np.clip(srgb,0,1)*255)),"RGB"),
                     (0,y))
    return output

def show_preview()->None:
    if not PREVIEW_FILE.is_file():raise FileNotFoundError("官方1k预览尚未按MD5验证")
    if PREVIEW_OUT.exists():
        print("CLIFFROCK_BLUEGRAY_PREVIEW_ALREADY_EXISTS",PREVIEW_OUT)
        return
    source=Image.open(PREVIEW_FILE)
    cool=grade(source)
    canvas=Image.new("RGB",(2080,1180),"#111817")
    origin=source.convert("RGB")
    canvas.paste(origin,(25,100))
    canvas.paste(cool,(1054,100))
    d=ImageDraw.Draw(canvas)
    fontpath=Path("C:/Windows/Fonts/msyh.ttc")
    font=ImageFont.truetype(str(fontpath),28) if fontpath.is_file() else ImageFont.load_default()
    d.text((25,28),"Marble Cliff 05（原始米灰岩） → CliffRock01（青灰色变体）",font=font,fill="#e5edf0")
    d.text((25,1138),"两侧只是官方1K预览的色彩校正对比；4K正式源图与材质需独立检验。",font=font,fill="#a6b8be")
    canvas.save(PREVIEW_OUT,"JPEG",quality=91,optimize=True)
    print("CLIFFROCK_BLUEGRAY_GRADE_PREVIEW",PREVIEW_OUT)

def create(verify_only:bool)->None:
    origin_sha=check_manifest()
    with Image.open(ORIGINAL) as im:
        if im.size!=(4096,4096) or im.mode not in ("RGB","RGBA"):
            raise RuntimeError("必须原始4096×4096照片扫描色图")
        expected=grade(im)
    sample=np.asarray(expected.resize((512,512),Image.Resampling.BILINEAR),dtype=np.float32)
    original=Image.open(ORIGINAL)
    old_mean=np.asarray(original.convert("RGB").resize((512,512)),dtype=np.float32).mean((0,1))
    new_mean=sample.mean((0,1))
    if not (new_mean[2] >= new_mean[1]+3 and new_mean[1]>=new_mean[0]+3):
        raise RuntimeError("候选BlueGray基础色不满足自然青灰配色")
    if not (new_mean.min()>55 and new_mean.max()<216):
        raise RuntimeError("岩石青灰色亮度超出可审查物理范围")
    stream=io.BytesIO()
    expected.save(stream,"PNG",compress_level=6,optimize=False)
    payload=stream.getvalue()
    source_spec=json.loads(SOURCE_MANIFEST.read_text(encoding="utf-8-sig"))
    grade_doc={
        "schemaVersion":1,
        "generatedBy":"Tools/Unreal/Terrain/GenerateCliffRock01BlueGrayAlbedo.py",
        "sourceId":"marble_cliff_05",
        "originalBaseColorSHA256":origin_sha,
        "gradedBaseColorSHA256":sha(payload),
        "colorSpace":"sRGB→linearRGB→luma/chroma→sRGB",
        "luminanceWeights":[.2126,.7152,.0722],
        "luminancePerChannelCoefficients":COEFFICIENTS.tolist(),
        "originalChromaticVariationRetention":float(CHROMA_RETENTION),
        "sourceResolution":[4096,4096],
        "sourcePackage":"/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Textures/T_DBA_CliffRock01_BaseColor_BlueGray",
        "gradedSourcePath":DERIVED.relative_to(ROOT).as_posix(),
        "meanOriginalRGB":np.round(old_mean,2).tolist(),
        "meanGradedRGB":np.round(new_mean,2).tolist(),
        "normalOrmHeightSourceUnchanged":True,
        "productionAssetApproved":False,
        "descriptionZh":"保留裂隙/石理形状和同源物理贴图，只修正暖米黄为低饱和自然青灰，用户尚未审核批准",
    }
    docraw=(json.dumps(grade_doc,ensure_ascii=False,indent=2)+"\n").encode("utf-8")
    if verify_only and (not DERIVED.exists() or not MANIFEST.exists()):
        raise FileNotFoundError("只读模式不能创建待审核4K颜色变体")
    for file,raw in ((DERIVED,payload),(MANIFEST,docraw)):
        if file.exists() and file.read_bytes()!=raw:
            raise RuntimeError("已有项目CliffRock颜色变体被修改，拒绝覆盖："+str(file))
    if not verify_only:
        for file,raw in ((DERIVED,payload),(MANIFEST,docraw)):
            if not file.exists():
                file.parent.mkdir(parents=True,exist_ok=True)
                file.write_bytes(raw)
                print("CLIFFROCK_BLUEGRAY_RESOURCE_CREATED",file.relative_to(ROOT).as_posix())
    print("CLIFFROCK_BLUEGRAY_4K_ALBEDO_QA_PASS source_md5_verified_by_acquire=True",
          "origMean=",np.round(old_mean,1).tolist(),"gradedMean=",np.round(new_mean,1).tolist(),
          "sha256=",sha(payload),"mode=verify" if verify_only else "mode=apply")

def main()->None:
    parser=argparse.ArgumentParser(description="青灰色裸岩扫描BaseColor颜色派生与签名")
    parser.add_argument("--preview",action="store_true")
    parser.add_argument("--verify",action="store_true")
    args=parser.parse_args()
    if args.preview:
        show_preview()
    else:
        create(args.verify)
if __name__=="__main__":main()
