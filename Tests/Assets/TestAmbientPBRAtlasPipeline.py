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
ZIP_ALTERNATE=PROJECT/"SourceArt/AmbientNature/Incoming/DivineBeasts_NiagaraPBR_AtlasAssets_20261011(1).zip"
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
# 白名单、版本锁定、备份和逐批验收是覆盖当前资源的必要安全约束。
preflight=(TOOLS/"PreflightPBRAtlases.py").read_text(encoding="utf8")
importer=(TOOLS/"ImportPBRAtlasAssetsUE58.py").read_text(encoding="utf8")
launcher=(TOOLS/"RunPBRAtlasImport.ps1").read_text(encoding="utf-8-sig")
check("APPROVED_ZIP_SHA256" in preflight,"预检锁定用户此次上传ZIP的SHA-256")
check("inspect_png_header" in preflight,"预检验证独立PNG尺寸及通道类型")
check("originalAssetBackups" in importer and "backupFileSha256" in importer,
      "批量覆旧前必须生成可校验备份清单")
check("imported_object_paths" in importer,"UE导入任务必须报告真实目标资产")
check('-Apply必须同时指定-Batch' in launcher,"禁止无批次执行一次性全量覆盖")
check("UEBatchImportReport_Batch" in launcher,"每批保留独立的UE导入与失败日志")
check("StagePBRFromDownloads.ps1" in launcher,
      "批次入口支持Windows下载目录的可信原图自动接收")
check("AMBIENT_PBR_CACHE_ROOT" in launcher and "D:\\DivineBeastsWorkspace_PBR_WorkCache_20261011" in launcher,
      "低容量时项目源码和PBR临时备份目录分离")
check("Get-CimInstance Win32_Process" in launcher,
      "正式材质覆盖前检查是否仍有其他UE编辑器实例打开项目")
check("backup_hash!=current_hash" in importer,
      "发现旧备份与当前资产不一致时拒绝继续覆盖")

check((PLATFORM/"Content/Environment/AmbientNature/Materials/M_GP_AmbientNature_PBR_Foliage.uasset").is_file(),
      "UE5.8真实PBR母材质已保存")
textures=PROJECT/"Content/VFX/Environment/AmbientNature/Textures"
materials=PROJECT/"Content/VFX/Environment/AmbientNature/Materials"
systems=PROJECT/"Content/VFX/Environment/AmbientNature/Niagara"
check(len(tuple(textures.glob("T_DBA_Ambient_*_Albedo.uasset")))==33,"33类旧Albedo资源身份继续存在")
check(len(tuple(materials.glob("MI_DBA_Ambient_*.uasset")))==33,"33个既有项目材质身份继续存在")
check(len(tuple(systems.glob("NS_DBA_Ambient_*.uasset")))==33,"33个Niagara系统身份继续存在")
scale=PROJECT/"SourceArt/AmbientNature/AmbientPBRPhysicalScaleManifest.json"
check(scale.is_file(),"保存33种花瓣/落叶物理尺寸参考，不将材质错误当作粒子大小")
if scale.is_file():
    physical=json.loads(scale.read_text(encoding="utf8"))
    species=physical.get("species",[])
    check(physical.get("physicalUnit")=="cm" and len(species)==33 and len({s["id"] for s in species})==33,
          "物种物理尺寸元数据33项均唯一且单位cm")
    check(sum(s["importPolicy"]=="auto_candidate" for s in species)==15,
          "PBR物理尺寸清单的15种自动候选权限必须与原图一致")
    check(physical.get("niagaraSizeApplied") is False,
          "没有实际Niagara尺寸绑定证据时不谎称已经完成粒子缩放")
pack=PROJECT/"SourceArt/AmbientNature/AmbientNatureStyleCatalog.json"
if pack.is_file():
    meta=json.loads(pack.read_text(encoding="utf8"))
    check(len(meta.get("styles",[]))==39,"已有项目风格总目录39项")
config=ROOT/"Game/Config/Custom/DedicatedServer/DefaultPakFileRules.ini"
if config.is_file():
    text=config.read_text(encoding="utf8")
    check("/GamePlatformVFX/..." in text and "/DBAPresentationPack_Core/..." in text,
          "专用服务器继续剥离纯视觉特效")
zip_present=ZIP.is_file() or ZIP_ALTERNATE.is_file()
report={"pass":not errors,"readyForStage":not errors,"inputZipPresent":zip_present,
        "state":"waiting_for_chat_generated_zip_transfer" if not zip_present else "source_zip_present",
        "checks":len(checks),"failures":errors,"zipExpectedAt":str(ZIP),
        "notesZh":"母材质已存在而ZIP源贴图未上传则不能宣称新的项目纹理/MI已替换"}
OUT.parent.mkdir(parents=True,exist_ok=True)
OUT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
print("AMBIENT_PBR_PIPELINE",report["pass"],"checks",report["checks"],
      "errors",len(errors),"zip_present",report["inputZipPresent"])
for error in errors:print("AMBIENT_PBR_PIPELINE_ERROR",error)
raise SystemExit(0 if not errors else 1)
