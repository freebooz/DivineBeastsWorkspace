# -*- coding: utf-8 -*-
"""UE5.8平台通用飘落Niagara的Ground Motion HLSL增量修复。

平台归属：GamePlatformVFX（平台通用视觉特效插件）。
修复原因：CustomHlsl表达式直接引用Particles.Age但对应参数未进入模块ParameterMapHistory，
UE5.8 GPU编译器报“无法在自定义表达式中使用变量：Particles.Age”，致Niagara不可见。
解决范围：仅替换模块内该一处未经绑定的粒子年龄引用为可用的Position/Velocity。
不会更改项目外观MI/纹理，不重新生成Niagara System，不改变网络/Gameplay逻辑。
后续必须验证GPU编译、真实粒子数量/位置及人工可见性，不能仅凭save资产认定完成。
"""
from __future__ import annotations
from pathlib import Path
import hashlib,json,shutil
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve().parent
ASSET="/GamePlatformVFX/Environment/FallingFoliage/Niagara/Modules/NM_GP_FoliageGroundMotion"
DISK=ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/FallingFoliage/Niagara/Modules/NM_GP_FoliageGroundMotion.uasset"
BACKUP=ROOT/"Saved/Validation/FallingFoliage/PBRAtlas20261011/GPUSceneReview/GroundMotionPreHLSLFix.uasset"
REPORT=ROOT/"Saved/Validation/FallingFoliage/PBRAtlas20261011/GPUSceneReview/GroundMotionHLSLFix.json"
SOURCE="float phase = Particles.Age*1.7 + p.x*0.047 + p.y*0.031;"
# 仅从有上下文保障的p/v派生空间相位；下风过程下位置/速度自然变化。
# 专门的跨区域阵风时间变化仍由将来数据化风场合法输入实现，不伪造时间变量。
FIXED="float phase = p.x*0.047 + p.y*0.031 + p.z*0.002 + (v.x+v.y)*0.015;"
result={"asset":ASSET,"status":"unknown","saved":False,"errors":[]}
try:
    if not str(unreal.Paths.get_project_file_path()).replace("\\","/").endswith("/DivineBeastsArena.uproject"):
        raise RuntimeError("非当前神兽联盟工程，拒绝修改")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("本工具只支持当前UE5.8")
    if not DISK.is_file():
        raise RuntimeError("目标.uasset缺失："+str(DISK))
    obj=unreal.load_asset(ASSET)
    if not isinstance(obj,unreal.NiagaraScript):
        raise RuntimeError("平台Ground Motion资源类型不符")
    nodes=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeCustomHlsl)
           if n.get_path_name().startswith(obj.get_path_name()+":")]
    if len(nodes)!=1:
        raise RuntimeError("预期平台模块恰好一个CustomHLSL节点，实际："+str(len(nodes)))
    node=nodes[0]
    hlsl=str(node.get_editor_property("custom_hlsl"))
    result["oldSourceSha256"]=hashlib.sha256(hlsl.encode("utf8")).hexdigest()
    if hlsl.count(FIXED)==1 and SOURCE not in hlsl:
        result["status"]="already_patched"
        result["saved"]=True
    else:
        if hlsl.count(SOURCE)!=1:
            raise RuntimeError("平台原HLSL目标片段不唯一或其他会话已修改，拒绝覆盖")
        if not BACKUP.exists():
            BACKUP.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(DISK,BACKUP)
        original_hash=hashlib.sha256(BACKUP.read_bytes()).hexdigest()
        file_hash=hashlib.sha256(DISK.read_bytes()).hexdigest()
        if original_hash!=file_hash:
            raise RuntimeError("原资产备份不匹配当前文件，拒绝继续覆盖")
        result["originalBinarySha256"]=file_hash
        patch=hlsl.replace(SOURCE,FIXED)
        node.set_editor_property("custom_hlsl",patch)
        actual=str(node.get_editor_property("custom_hlsl"))
        if actual!=patch:
            raise RuntimeError("UE CustomHLSL节点未回读到修正内容")
        # 节点EditorPropertyChanged将NiagaraGraph标记为需要重新编译。
        if not unreal.EditorAssetLibrary.save_loaded_asset(obj,only_if_is_dirty=False):
            raise RuntimeError("NiagaraScript正式保存失败")
        result["status"]="patched"
        result["saved"]=True
    result["sourceSha256"]=hashlib.sha256(str(node.get_editor_property("custom_hlsl")).encode("utf8")).hexdigest()
    result["backupExists"]=BACKUP.exists()
    print("FOLIAGE_GROUND_HLSL_FIX",result["status"],"saved",result["saved"],flush=True)
except Exception as exc:
    result["status"]="failed"
    result["errors"].append(str(exc))
    print("FOLIAGE_GROUND_HLSL_FIX_ERROR",str(exc),flush=True)
    raise
finally:
    REPORT.parent.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
