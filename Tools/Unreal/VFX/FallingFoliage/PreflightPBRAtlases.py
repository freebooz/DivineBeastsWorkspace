#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟PBR图集导入预检（无外部依赖，可在UE5.8附带Python及CI调用）。
项目资源层：仅审查用户复制到Incoming的ZIP及Manifest，不能生成/修改.uasset。
支持--inspect/--extract；解包校验原图及独立四通道哈希，阻止目录穿越和误覆盖。
"""
from __future__ import annotations
import argparse, hashlib, json, pathlib, shutil, sys, zipfile

ROOT=pathlib.Path(__file__).resolve().parents[4]
CONTENT=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/AmbientNature"
INCOMING=CONTENT/"Incoming/DivineBeasts_NiagaraPBR_AtlasAssets_20261011.zip"
STAGE=ROOT/"Saved/Validation/FallingFoliage/PBRAtlas20261011/Stage"
REPORT=ROOT/"Saved/Validation/FallingFoliage/PBRAtlas20261011/PreflightReport.json"
CHANNELS=("Albedo","Normal","Roughness","Opacity")


def validate_archive(path: pathlib.Path) -> dict:
    if not path.is_file():
        raise FileNotFoundError("缺少用户图集ZIP，请将其复制至: "+str(path))
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
            for member in style["maps"]:
                data=z.read(member["file"])
                if hashlib.sha256(data).hexdigest()!=member["sha256"]:
                    raise RuntimeError("纹理文件SHA-256不符："+member["file"])
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
