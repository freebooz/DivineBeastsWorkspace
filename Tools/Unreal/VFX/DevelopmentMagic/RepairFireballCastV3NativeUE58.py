# -*- coding: utf-8 -*-
"""《神兽联盟》火球 V3 P1 的 UE5.8 原生 HLSL 修复，仅修改审核包内一个 NiagaraScript。

UE5.8 CustomHLSL 分词器不将百分号作为分隔符，Index与百分号之间必须留空格。
生成器默认隐藏InputMap且标记非必需，导致调用方参数流丢失；按既有冰箭合同恢复暴露/必需输入。
任何无法唯一定位节点、源文本不符、包只读或保存失败都立即停止。
独立复审不修改 GAS、发射/命中、正式火球、场景或第三方资产。
"""
import hashlib
import json
import re
import shutil
import traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
SCRIPT_PATH="/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NM_DBA_FireballCast_V3Review"
SYSTEM_PATH="/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review"
SOURCE=ROOT/"Tools/Unreal/VFX/DevelopmentMagic/AuthorFireballCastV3Review.ps1"
BINARY=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NM_DBA_FireballCast_V3Review.uasset"
EVIDENCE=ROOT/"Saved/Validation/FireballV3P1/NativeShaderFix"
REPORT=EVIDENCE/"ShaderRepairResult.json"
result={"asset":SCRIPT_PATH,"system":SYSTEM_PATH,"status":"not_started","humanAccepted":False,"productionBound":False,"serverTimingsModified":False}

try:
    project=unreal.Paths.get_project_file_path().replace("\\","/")
    if not project.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("拒绝对其它工程执行")
    if not unreal.SystemLibrary.get_engine_version().startswith("5.8"):
        raise RuntimeError("只支持项目原生UE5.8")
    source=SOURCE.read_text(encoding="utf-8-sig")
    match=re.search(r"\$hlsl=@'\s*\n(.*?)\n'@",source,re.S)
    if not match:raise RuntimeError("冻结的P1作者源没有独立HLSL块")
    wanted=match.group(1).rstrip("\r\n")
    if "int uid = Index % 1024;" not in wanted or "Particles.UniqueID" in wanted or "Index*" in wanted:
        raise RuntimeError("HLSL未符合UE5.8分词规约")
    if not BINARY.is_file():raise RuntimeError("仅可修改已生成的P1审核模块")
    script=unreal.load_asset(SCRIPT_PATH)
    system=unreal.load_asset(SYSTEM_PATH)
    if not isinstance(script,unreal.NiagaraScript) or not isinstance(system,unreal.NiagaraSystem):
        raise RuntimeError("资源类型不匹配")
    nodes=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeCustomHlsl)
           if n.get_path_name().startswith(script.get_path_name()+":")]
    if len(nodes)!=1:raise RuntimeError("预期只有一个审核HLSL节点，实际 "+str(len(nodes)))
    node=nodes[0]
    before=str(node.get_editor_property("custom_hlsl")).rstrip("\r\n")
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    result["sourceBeforeSha256"]=hashlib.sha256(before.encode("utf-8")).hexdigest()
    result["sourceAfterSha256"]=hashlib.sha256(wanted.encode("utf-8")).hexdigest()
    result["binaryBeforeSha256"]=hashlib.sha256(BINARY.read_bytes()).hexdigest()
    result["expectedHlsl"]=wanted[:210]
    if before!=wanted:
        backup=EVIDENCE/"NM_DBA_FireballCast_V3Review_BeforeRepair.uasset"
        if not backup.exists():shutil.copy2(BINARY,backup)
        if hashlib.sha256(backup.read_bytes()).hexdigest()!=result["binaryBeforeSha256"]:
            raise RuntimeError("现有P1模块与审核备份不符，拒绝覆盖")
        node.set_editor_property("custom_hlsl",wanted)
        if str(node.get_editor_property("custom_hlsl")).rstrip("\r\n")!=wanted:
            raise RuntimeError("Niagara HLSL内存回读失败")
        if not unreal.EditorAssetLibrary.save_loaded_asset(script,False):
            raise RuntimeError("UE5.8模块保存失败")
        result["status"]="patched"
    else:
        result["status"]="already_matching"
    inputs=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeInput)
            if n.get_path_name().startswith(script.get_path_name()+":")]
    if len(inputs)!=1:raise RuntimeError("审核模块预期只有一个InputMap输入")
    options=inputs[0].get_editor_property("exposure_options")
    result["inputMapBefore"]={k:bool(options.get_editor_property(k)) for k in ("exposed","required","hidden")}
    if result["inputMapBefore"]!={"exposed":True,"required":True,"hidden":False}:
        options.set_editor_property("exposed",True)
        options.set_editor_property("required",True)
        options.set_editor_property("hidden",False)
        inputs[0].set_editor_property("exposure_options",options)
        if not unreal.EditorAssetLibrary.save_loaded_asset(script,False):
            raise RuntimeError("InputMap合同修复保存失败")
        result["status"]="patched_input_map"
    result["inputMapAfter"]={k:bool(inputs[0].get_editor_property("exposure_options").get_editor_property(k)) for k in ("exposed","required","hidden")}
    result["binaryAfterSha256"]=hashlib.sha256(BINARY.read_bytes()).hexdigest()
    result["saved"]=True
    result["shaderGpuCompiled"]=False
    result["nativeVisualCaptured"]=False
    print("SSLM_FIREBALL_P1_NATIVE_SHADER",result["status"],SCRIPT_PATH,flush=True)
except Exception as ex:
    result["status"]="failed"
    result["error"]=str(ex)
    result["traceback"]=traceback.format_exc()[-3600:]
    print("SSLM_FIREBALL_P1_SHADER_FAILED",str(ex),flush=True)
    raise
finally:
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
