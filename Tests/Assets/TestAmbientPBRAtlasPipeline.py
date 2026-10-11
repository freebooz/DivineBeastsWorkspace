#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""《神兽联盟》自然落叶 PBR 分批导入流水线静态门禁（不依赖UE编辑器）。
本测试验证真实平台母材质文件、导入脚本静态安全边界、客户端/服务器插件归属。
若用户原图ZIP未在Windows工程，明确报告waiting_input，不能虚构已导入。
"""
from __future__ import annotations
import ast,json,pathlib,hashlib
ROOT=pathlib.Path(__file__).resolve().parents[2]
TOOLS=ROOT/"Tools/Unreal/VFX/FallingFoliage"
PLATFORM=ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformVFX"
PROJECT=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core"
ZIP=PROJECT/"SourceArt/AmbientNature/Incoming/DivineBeasts_NiagaraPBR_AtlasAssets_20261011.zip"
OUT=ROOT/"Saved/Validation/FallingFoliage/PBRAtlas20261011/StaticPipelineAudit.json"
errors=[]
checks=[]
def check(value,label):
    if value:checks.append(label)
    else:errors.append(label)
for filename in ("PreflightPBRAtlases.py","AuthorAmbientPBRMasterUE58.py",
                 "ImportPBRAtlasAssetsUE58.py","ValidateAmbientPBRMasterUE58.py"):
    path=TOOLS/filename
    check(path.is_file(),"脚本存在:"+filename)
    if path.is_file():
        try:
            tree=ast.parse(path.read_text(encoding="utf8"))
            check(True,"Python语法:"+filename)
        except SyntaxError as ex:errors.append("Python语法错误:"+filename+":"+str(ex))
check((TOOLS/"RunPBRAtlasImport.ps1").is_file(),"导入批次PowerShell脚本存在")
check((PLATFORM/"Content/Environment/AmbientNature/Materials/M_GP_AmbientNature_PBR_Foliage.uasset").is_file(),
      "UE5.8真实PBR母材质已保存")
textures=PROJECT/"Content/VFX/Environment/AmbientNature/Textures"
materials=PROJECT/"Content/VFX/Environment/AmbientNature/Materials"
systems=PROJECT/"Content/VFX/Environment/AmbientNature/Niagara"
check(len(tuple(textures.glob("T_DBA_Ambient_*_Albedo.uasset")))==33,"33类旧Albedo资源身份继续存在")
check(len(tuple(materials.glob("MI_DBA_Ambient_*.uasset")))==33,"33个既有项目材质身份继续存在")
check(len(tuple(systems.glob("NS_DBA_Ambient_*.uasset")))==33,"33个Niagara系统身份继续存在")
pack=PROJECT/"SourceArt/AmbientNature/AmbientNatureStyleCatalog.json"
if pack.is_file():
    meta=json.loads(pack.read_text(encoding="utf8"))
    check(len(meta.get("styles",[]))==39,"已有项目风格总目录39项")
config=ROOT/"Game/Config/Custom/DedicatedServer/DefaultPakFileRules.ini"
if config.is_file():
    text=config.read_text(encoding="utf8")
    check("/GamePlatformVFX/..." in text and "/DBAPresentationPack_Core/..." in text,
          "专用服务器继续剥离纯视觉特效")
report={"pass":not errors,"readyForStage":not errors,"inputZipPresent":ZIP.is_file(),
        "state":"waiting_for_chat_generated_zip_transfer" if not ZIP.is_file() else "source_zip_present",
        "checks":len(checks),"failures":errors,"zipExpectedAt":str(ZIP),
        "notesZh":"母材质已存在而ZIP源贴图未上传则不能宣称新的项目纹理/MI已替换"}
OUT.parent.mkdir(parents=True,exist_ok=True)
OUT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
print("AMBIENT_PBR_PIPELINE",report["pass"],"checks",report["checks"],
      "errors",len(errors),"zip_present",report["inputZipPresent"])
for error in errors:print("AMBIENT_PBR_PIPELINE_ERROR",error)
raise SystemExit(0 if not errors else 1)
