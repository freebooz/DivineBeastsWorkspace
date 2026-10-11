#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟PBR图集导入预检（无外部依赖，可在UE5.8附带Python及CI调用）。
项目资源层：仅审查用户复制到Incoming的ZIP及Manifest，不能生成/修改.uasset。
支持--inspect/--extract；解包校验原图及独立四通道哈希，阻止目录穿越和误覆盖。
"""
from __future__ import annotations
import argparse, hashlib, json, os, pathlib, struct, zipfile

ROOT=pathlib.Path(__file__).resolve().parents[4]
CONTENT=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/AmbientNature"
INCOMING=CONTENT/"Incoming/DivineBeasts_NiagaraPBR_AtlasAssets_20261011.zip"
CACHE_ROOT=pathlib.Path(os.environ.get("AMBIENT_PBR_CACHE_ROOT",str(ROOT/"Saved/Validation/FallingFoliage/PBRAtlas20261011")))
if not CACHE_ROOT.is_absolute():
    raise RuntimeError("PBR暂存路径必须为绝对路径，禁止在项目内隐式解析工作目录")
STAGE=CACHE_ROOT/"Stage"
REPORT=ROOT/"Saved/Validation/FallingFoliage/PBRAtlas20261011/PreflightReport.json"
CHANNELS=("Albedo","Normal","Roughness","Opacity")
# 锁定由用户实际上传并已本地校验的原始PBR ZIP SHA-256；名称变化不会改变文件内容身份。
APPROVED_ZIP_SHA256="07c5b03e056a0318af8c2368cc3a06c39ff2ef26c181b2912ef2f78f57084015"


def inspect_png_header(source:bytes,member:dict)->None:
    """使用标准库检查PNG IHDR的尺寸/色彩类型，禁止文件后缀或像素模式混淆。"""
    if len(source)<33 or source[:8]!=bytes((137,80,78,71,13,10,26,10)):
        raise RuntimeError("PBR纹理不是合法PNG："+member["file"])
    chunk_size=struct.unpack(">I",source[8:12])[0]
    if source[12:16]!=b"IHDR" or chunk_size!=13:
        raise RuntimeError("PBR纹理缺少正确IHDR："+member["file"])
    width,height,depth,color_type=struct.unpack(">IIBB",source[16:26])
    expected_type=6 if member["mode"]=="RGBA" else 0 if member["mode"]=="L" else -1
    if ((width,height)!=(member["width"],member["height"]) or depth!=8 or
            color_type!=expected_type or (width,height)!=(512,512)):
        raise RuntimeError("PBR图像分辨率/8位RGBA或灰度通道错误："+member["file"])



def validate_archive(path: pathlib.Path) -> dict:
    if not path.is_file():
        raise FileNotFoundError("缺少用户图集ZIP，请将其复制至: "+str(path))
    if hashlib.sha256(path.read_bytes()).hexdigest()!=APPROVED_ZIP_SHA256:
        raise RuntimeError("ZIP与用户本次确认上传版本不一致；禁止覆盖正式旧材质")
    with zipfile.ZipFile(path) as z:
        if z.testzip() is not None:
            raise RuntimeError("原始ZIP校验失败，禁止导入")
        entries={f.filename:f for f in z.infolist()}
        if len(entries)!=len(z.infolist()):raise RuntimeError("ZIP重复文件路径")
        for name,file in entries.items():
            normalized=pathlib.PurePosixPath(name)
            if (normalized.is_absolute() or ".." in normalized.parts or
                    "\\" in name or ":" in name or file.file_size>24*1024*1024):
                raise RuntimeError("ZIP存在非法路径/超大成员："+name)
            if len(normalized.parts)>5:
                raise RuntimeError("目录深度异常："+name)
        manifest=json.loads(z.read("Manifest.json").decode("utf8"))
        if (manifest.get("schemaVersion")!=1 or manifest.get("sourceAtlasCount")!=6 or
                manifest.get("pbrMapCount")!=132 or manifest.get("autoCandidateCount")!=15):
            raise RuntimeError("PBR图集目录版本或总数不匹配")
        species=manifest["styles"]
        if len(species)!=33 or len({s["id"] for s in species})!=33:
            raise RuntimeError("仅接受预期33套纹理身份")
        if len(manifest["atlases"])!=6:
            raise RuntimeError("必须包含6张用户生成图集")
        for atlas in manifest["atlases"]:
            raw=z.read(atlas["file"])
            if hashlib.sha256(raw).hexdigest()!=atlas["sha256"]:
                raise RuntimeError("原图哈希不匹配："+atlas["nameZh"])
        maps_count=0
        for style in species:
            if not style["id"].isascii() or not style["id"].isalnum():
                raise RuntimeError("禁止自定义非法风格身份："+style["id"])
            if len(style["maps"])!=4 or {m["channel"] for m in style["maps"]}!=set(CHANNELS):
                raise RuntimeError("图集未包含4个不同通道："+style["id"])
            if style["importPolicy"] not in ("auto_candidate","review_only","fx_reference_only"):
                raise RuntimeError("非法导入权限："+style["id"])
            if style["importPolicy"]=="auto_candidate" and style["kind"] not in ("petal","leaf"):
                raise RuntimeError("不允许多粒子FX图自动覆盖植被材质："+style["id"])
            if (not (0.0<style["physicalWidthCm"]<=60.0) or
                    not (0.0<style["physicalHeightCm"]<=60.0)):
                raise RuntimeError("缺少合理的视觉尺寸配置（单位厘米）："+style["id"])
            for member in style["maps"]:
                data=z.read(member["file"])
                if hashlib.sha256(data).hexdigest()!=member["sha256"]:
                    raise RuntimeError("纹理文件SHA-256不符："+member["file"])
                inspect_png_header(data,member)
                maps_count+=1
        if maps_count!=132:
            raise RuntimeError("33套四通道贴图数量不足")
    return manifest


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--mode",choices=("inspect","extract"),default="inspect")
    parser.add_argument("--file",type=pathlib.Path,default=INCOMING)
    parser.add_argument("--allow-existing-stage",action="store_true")
    args=parser.parse_args()
    status={"source":str(args.file),"mode":args.mode,"passed":False}
    try:
        manifest=validate_archive(args.file)
        if args.mode=="extract":
            if STAGE.exists() and not args.allow_existing_stage:
                raise FileExistsError("已有暂存目录，拒绝静默覆盖："+str(STAGE))
            if not STAGE.exists():
                STAGE.mkdir(parents=True,exist_ok=False)
                with zipfile.ZipFile(args.file) as z:
                    z.extractall(STAGE)
            # 重试时拒绝利用之前批次留下的另一份Manifest，避免旧规格覆盖新纹理。
            with zipfile.ZipFile(args.file) as z:
                approved_manifest_hash=hashlib.sha256(z.read("Manifest.json")).hexdigest()
            staged_manifest=STAGE/"Manifest.json"
            if not staged_manifest.is_file() or hashlib.sha256(staged_manifest.read_bytes()).hexdigest()!=approved_manifest_hash:
                raise RuntimeError("暂存Manifest与当前用户批准ZIP不一致；请人工审查，不自动清空旧暂存目录")
            for style in manifest["styles"]:
                for member in style["maps"]:
                    p=STAGE/member["file"]
                    if hashlib.sha256(p.read_bytes()).hexdigest()!=member["sha256"]:
                        raise RuntimeError("解包后SHA-256不匹配："+str(p))
        status.update({"passed":True,"atlasCount":6,"styles":33,"maps":132,
                       "autoCandidates":sum(s["importPolicy"]=="auto_candidate" for s in manifest["styles"]),
                       "manualStyles":[s["id"] for s in manifest["styles"] if s["importPolicy"]!="auto_candidate"]})
        print("AMBIENT_PBR_ATLAS_PREFLIGHT_PASS",status["styles"],status["maps"],"auto",status["autoCandidates"])
    except Exception as exc:
        status["error"]=str(exc)
        print("AMBIENT_PBR_ATLAS_PREFLIGHT_ERROR",str(exc))
        raise
    finally:
        REPORT.parent.mkdir(parents=True,exist_ok=True)
        REPORT.write_text(json.dumps(status,ensure_ascii=False,indent=2)+"\n",encoding="utf8")


if __name__=="__main__":
    main()
