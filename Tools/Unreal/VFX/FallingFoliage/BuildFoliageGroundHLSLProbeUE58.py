# -*- coding: utf-8 -*-
"""UE5.8平台落叶地面运动HLSL独立探针（不会修改正式已有.uasset）。

克隆GamePlatformVFX的GroundMotion模块及一份项目层PearPetal Niagara到/Game/Development/FallingFoliage/Probes。
仅改副本未在Niagara参数图中绑定的Particles.Age自定义HLSL引用，并把两个函数调用切换到修复副本。
通过D3D12审核系统是否能正常GPU编译及渲染，之后再另行评估正式资产的安全修复时机。
"""
from pathlib import Path
import json,hashlib,traceback
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve().parent
ORIG_SCRIPT="/GamePlatformVFX/Environment/FallingFoliage/Niagara/Modules/NM_GP_FoliageGroundMotion"
ORIG_SYS="/DBAPresentationPack_Core/VFX/Environment/AmbientNature/Niagara/NS_DBA_Ambient_PearPetal"
PROBE_ROOT="/Game/Development/FallingFoliage/Probes"
PROBE_SCRIPT=PROBE_ROOT+"/NM_GP_FoliageGroundMotion_AgeSafeProbe"
PROBE_SYS=PROBE_ROOT+"/NS_DBA_Ambient_PearPetal_HlslProbe"
REPORT=ROOT/"Saved/Validation/FallingFoliage/PBRAtlas20261011/GPUSceneReview/GroundHLSLProbeAssets.json"
OLD="float phase = Particles.Age*1.7 + p.x*0.047 + p.y*0.031;"
NEW="float phase = p.x*0.047 + p.y*0.031 + p.z*0.002 + (v.x+v.y)*0.015;"
ASSET=unreal.EditorAssetLibrary
result={"scope":"isolated_new_dev_assets","newAssets":[],"status":"unknown","errors":[],"oldReferences":0,"newReferences":0}

def asset(kind,path):
    obj=ASSET.load_asset(path)
    if not isinstance(obj,kind):raise RuntimeError("资产类型不正确："+path)
    return obj

try:
    if not str(unreal.Paths.get_project_file_path()).replace("\\","/").endswith("/DivineBeastsArena.uproject"):
        raise RuntimeError("仅支持锁定的《神兽联盟》UE5.8工作区")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("项目实际引擎不是UE5.8")
    source=asset(unreal.NiagaraScript,ORIG_SCRIPT)
    original_ns=asset(unreal.NiagaraSystem,ORIG_SYS)
    if not ASSET.does_directory_exist(PROBE_ROOT):
        ASSET.make_directory(PROBE_ROOT)
    if ASSET.does_asset_exist(PROBE_SCRIPT) or ASSET.does_asset_exist(PROBE_SYS):
        raise RuntimeError("独立探针资产已存在，拒绝覆盖/修改不明状态："+PROBE_ROOT)
    probe_script=ASSET.duplicate_asset(ORIG_SCRIPT,PROBE_SCRIPT)
    if not isinstance(probe_script,unreal.NiagaraScript):
        raise RuntimeError("无法复制平台模块到独立审核目录")
    result["newAssets"].append(PROBE_SCRIPT)
    nodes=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeCustomHlsl)
           if n.get_path_name().startswith(probe_script.get_path_name()+":")]
    if len(nodes)!=1:
        raise RuntimeError("修复模块副本中的CustomHLSL节点数量不为1："+str(len(nodes)))
    node=nodes[0]
    old_code=str(node.get_editor_property("custom_hlsl"))
    if old_code.count(OLD)!=1:
        raise RuntimeError("副本没有唯一可修复的未经绑定年龄表达式")
    corrected=old_code.replace(OLD,NEW)
    node.set_editor_property("custom_hlsl",corrected)
    if str(node.get_editor_property("custom_hlsl"))!=corrected:
        raise RuntimeError("Niagara模块CustomHLSL写入后未通过内存回读")
    result["probeHLSLSha256"]=hashlib.sha256(corrected.encode("utf8")).hexdigest()
    if not ASSET.save_loaded_asset(probe_script,only_if_is_dirty=False):
        raise RuntimeError("修复模块副本未被UE5.8保存")
    probe_sys=ASSET.duplicate_asset(ORIG_SYS,PROBE_SYS)
    if not isinstance(probe_sys,unreal.NiagaraSystem):
        raise RuntimeError("无法创建独立梨花Niagara审核系统")
    result["newAssets"].append(PROBE_SYS)
    actual=[]
    for function in unreal.ObjectIterator(unreal.NiagaraNodeFunctionCall):
        if not function.get_path_name().startswith(probe_sys.get_path_name()+":"):
            continue
        original=function.get_editor_property("function_script")
        if original and original.get_path_name()==source.get_path_name():
            result["oldReferences"]+=1
            function.set_editor_property("function_script",probe_script)
            rebound=function.get_editor_property("function_script")
            if rebound!=probe_script:
                raise RuntimeError("Niagara副本函数调用未被指向修复模块："+function.get_path_name())
            result["newReferences"]+=1
            actual.append(function.get_path_name())
    if result["newReferences"]!=2:
        raise RuntimeError("预计近/远景各有一个GroundMotion函数调用，实际："+str(result["newReferences"]))
    if not ASSET.save_loaded_asset(probe_sys,only_if_is_dirty=False):
        raise RuntimeError("独立Niagara探针系统保存失败")
    result["status"]="created"
    result["probeModule"]=PROBE_SCRIPT
    result["probeSystem"]=PROBE_SYS
    result["functionNodes"]=actual
    print("FOLIAGE_AGE_SAFE_PROBE_CREATED",result["newReferences"],"system",PROBE_SYS,flush=True)
except Exception as exc:
    result["status"]="failed"
    result["errors"].append(str(exc))
    print("FOLIAGE_AGE_SAFE_PROBE_ERROR",str(exc),flush=True)
    raise
finally:
    REPORT.parent.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
