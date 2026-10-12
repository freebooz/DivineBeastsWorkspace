#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""正式鸟群 Niagara 样条/Boids/羽翼扩展的唯一编辑器制作入口。

--inspect：离线校验4份平台GPU源码、三物种Niagara正式资产及路径契约，不改资产。
--author-modules：必须通过当前项目UE5.8的Monolith，为现有平台插件创建4个中立Niagara模块。
--integrate-species：用Monolith将新模块添加到三种项目真实Niagara系统，再检测编译、保存、重新读取。

实施边界：不创建第二套VFX播放器、不向平台硬引用DBA物种、不得重写7个已发布参数。
所有写入须先满足Monolith实时编辑器身份、用户指定本任务独占的Niagara目录，
发生第一个错误即中止，留存失败证据；只读扫描不能冒充资产已生成。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
PLATFORM="/GamePlatformVFX/Environment/BirdFlock/Niagara"
PROJECT="/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Niagara"
SOURCE=ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/BirdFlock"
REPORT=ROOT/"Saved/Validation/BirdFlockSplineFlight/Authoring"
EMITTER="BirdFlock"

MODULES=[
    {
        "name":"BirdFlockSplineProgress",
        "source":"BirdFlockSplineProgress.hlsl",
        "path":PLATFORM+"/NM_GP_BirdFlockSplineProgress",
        "inputs":("Clock","Individual","PathLength","Speed","Seed","Mode"),
        "description":"通用GPU路径进度：旧模式0..2不进入该模块，3单程、4循环、5往返。"
    },
    {
        "name":"BirdFlockSplineFollow",
        "source":"BirdFlockSplineFollow.hlsl",
        "path":PLATFORM+"/NM_GP_BirdFlockSplineFollow",
        "inputs":("Clock","Individual","RoutePosition","RouteTangent","Radius","Wind","Speed","Seed"),
        "description":"Spline数据接口位置/切向采样后的群飞偏移与姿态，不产生新Actor。"
    },
    {
        "name":"BirdFlockBoidsSteering",
        "source":"BirdFlockBoidsSteering.hlsl",
        "path":PLATFORM+"/NM_GP_BirdFlockBoidsSteering",
        "inputs":("Neighbors","NeighborPositionSum","NeighborVelocitySum","SeparationSum",
                  "SeparationWeight","AlignmentWeight","CohesionWeight"),
        "description":"有界邻域群体规则，未配置NeighborGrid3D时安全零向量。"
    },
    {
        "name":"BirdFlockWingState",
        "source":"BirdFlockWingState.hlsl",
        "path":PLATFORM+"/NM_GP_BirdFlockWingState",
        "inputs":("Clock","Individual","Seed","BaseFlapHz","BaseAmplitude","GlideBlend"),
        "description":"逐鸟相位/滑翔输出至Dynamic Material Parameter；RGB羽色和Alpha旧权重不变。"
    }
]
SYSTEMS=[PROJECT+"/NS_DBA_BirdFlock_"+name for name in ("Seagull","Swallow","Eagle")]
LEGACY_SYSTEM=PLATFORM+"/NS_GP_BirdFlock_Base"
MATERIAL="/GamePlatformVFX/Environment/BirdFlock/Materials/M_GP_BirdFlock_Master"
ADVANCED_RULES={
    "User.SeparationWeight":("float",0.0),
    "User.AlignmentWeight":("float",0.0),
    "User.CohesionWeight":("float",0.0),
    "User.GlideBlend":("float",0.0)
}


def check_local_sources():
    if not SOURCE.is_dir():
        raise RuntimeError("找不到平台BirdFlock独立源码目录")
    code=[]
    for module in MODULES:
        path=SOURCE/module["source"]
        if not path.is_file() or path.stat().st_size < 150:
            raise RuntimeError("缺少有效平台GPU模块源代码："+str(path))
        b=path.read_bytes()
        if b"GPU_SIMULATION" not in b:
            raise RuntimeError("文件缺少GPU_SIMULATION边界："+str(path))
        code.append({"path":str(path.relative_to(ROOT)).replace("\\","/"),
                     "sha256":hashlib.sha256(b).hexdigest()})
    asset_base=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/BirdFlock/Niagara"
    assets=[]
    for species in ("Seagull","Swallow","Eagle"):
        f=asset_base/("NS_DBA_BirdFlock_"+species+".uasset")
        if not f.is_file() or f.stat().st_size < 1024:
            raise RuntimeError("原正式Niagara资产丢失或异常："+str(f))
        assets.append({"path":str(f.relative_to(ROOT)).replace("\\","/"),
                       "sha256":hashlib.sha256(f.read_bytes()).hexdigest(),
                       "asset_size":f.stat().st_size})
    result={"status":"source_audit_only","engine_asset_authoring":"not_run",
            "modules":code,"original_formal_systems":assets,
            "note":"读Git文件路径和散列不能替代UE5.8 GPU图创建/编译/运行。"}
    return result


def guard_monolith():
    # 必须在任务专属UE Editor中操作；不触碰其它任务未保存的地图/材质/系统。
    if os.environ.get("BIRD_SPLINE_ASSET_AUTHOR") != "1":
        raise RuntimeError("未显式设置BIRD_SPLINE_ASSET_AUTHOR=1，拒绝所有UE资产写入")
    from BirdFlockMonolithProtocol import niagara, editor_python  # 懒加载，inspect离线可用
    project_identity=editor_python(
        "import unreal\n"
        "p=unreal.Paths.get_project_file_path().replace('\\\\','/')\n"
        "assert p.endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject'),p\n"
        "assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')\n"
        "print('BIRD_SPLINE_EDITOR_IDENTITY_OK',p)\n"
    )
    if not project_identity:
        raise RuntimeError("Monolith编辑器身份回读为空")
    return niagara, editor_python


def author_modules(niagara, editor_python):
    # 不能以Monolith“已连接”状态替代资产真实类型核验；检查不通过不创建下一步资源。
    listing=editor_python(
        "import unreal\n"
        "a=unreal.EditorAssetLibrary\n"
        "p='/GamePlatformVFX/Environment/BirdFlock/Niagara/'\n"
        "print('BIRD_SPLINE_PLATFORM_BASE',a.does_asset_exist(p+'NM_GP_BirdFlockMotion'))\n"
        "print('BIRD_SPLINE_NIAGARA_BASE',a.does_asset_exist(p+'NS_GP_BirdFlock_Base'))")
    if not listing:
        raise RuntimeError("已有平台基础Niagara资源不能由UE回读")
    out=[]
    for module in MODULES:
        source=(SOURCE/module["source"]).read_text(encoding="utf-8")
        # 内置create_module_from_hlsl必须对同名路径做幂等存在性验证；
        # Monolith出现半写/编译错误时不自动覆盖重试。
        result=niagara("create_module_from_hlsl",
            name=module["name"],save_path=module["path"],
            hlsl=source,inputs=[{"name":x,"type":"float3" if x in (
                    "RoutePosition","RouteTangent","NeighborPositionSum",
                    "NeighborVelocitySum","SeparationSum") else "float"} for x in module["inputs"]],
            outputs=[],description=module["description"])
        save=niagara("save_system",asset_path=module["path"])
        out.append({"asset":module["path"],"created":result,"saved":save})
        print("BIRD_SPLINE_MODULE_AUTHORED",module["path"])
    return out


def integrate_species(niagara, editor_python):
    # 先检查三个项目资产都能被UE独立加载；不修改旧DefinitionId。
    editor_python(
        "import unreal\n"
        "a=unreal.EditorAssetLibrary\n"
        "prefix='/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Niagara/'\n"
        "for n in ('Seagull','Swallow','Eagle'):\n"
        " p=prefix+'NS_DBA_BirdFlock_'+n\n"
        " x=a.load_asset(p)\n"
        " assert isinstance(x,unreal.NiagaraSystem),p\n"
        "print('BIRD_SPLINE_SPECIES_PRECHECK_OK')")
    # 通过GetOrderedModules读取真实粒子更新模块，防止重复加入相同脚本；不破坏Emitter Self/Once。
    results=[]
    for target in SYSTEMS:
        before=niagara("get_ordered_modules",asset_path=target,emitter=EMITTER)
        before_json=json.dumps(before,ensure_ascii=False)
        for module in MODULES:
            if module["path"].split("/")[-1] in before_json:
            # 只有满足真实Niagara模块图资源与Spline DI前提才允许连接到既有三物种系统。
                print("BIRD_SPLINE_MODULE_ALREADY_PRESENT",target,module["name"])
                continue
            # Niagara编辑器动作把模块加入现有Emitter，不创建平行Particle System。
            niagara("add_module",asset_path=target,emitter=EMITTER,
                stage="particle_update",script=module["path"])
        for key,(typ,default) in ADVANCED_RULES.items():
            # 参数只能经过同一真实Niagara UserParameter协议创建；
            # Definitions参数Schema必须先由独立UE编辑器脚本扩展。
            niagara("add_user_parameter",asset_path=target,
                parameter=key,type=typ,default=str(default))
        diag=niagara("get_system_diagnostics",asset_path=target,compile_first=True)
        error_count=diag.get("error_count",None)
        if error_count is None or error_count!=0:
            raise RuntimeError("正式Niagara新模块编译未通过，不能保存/标完成："+target+
                               ",errors="+str(diag.get("errors")))
        niagara("save_system",asset_path=target,only_if_dirty=False)
        after=niagara("get_ordered_modules",asset_path=target,emitter=EMITTER)
        results.append({"system":target,"modules_before":before,"modules_after":after,
                        "diagnostics":diag,"gpu_runtime":"not_proven",
                        "spline_data_interface":"requires_real_editor_binding"})
    return results


def main():
    parser=argparse.ArgumentParser(description="鸟群Spline Niagara资产作者")
    parser.add_argument("--inspect",action="store_true",help="仅审计源码/原资产，不改UE")
    parser.add_argument("--author-modules",action="store_true",help="需独占Monolith编辑器")
    parser.add_argument("--integrate-species",action="store_true",help="需NIagara图/DI与白名单已准备")
    args=parser.parse_args()
    audit=check_local_sources()
    if args.inspect:
        print(json.dumps(audit,ensure_ascii=False,indent=2))
        return
    if not args.author_modules and not args.integrate_species:
        raise RuntimeError("必须显式选择--inspect、--author-modules或--integrate-species")
    niagara, editor_python=guard_monolith()
    REPORT.mkdir(parents=True,exist_ok=True)
    output={"source_audit":audit,"actions":[],"result":"pending","visual_acceptance":"pending"}
    try:
        if args.author_modules:
            output["actions"]=author_modules(niagara,editor_python)
        if args.integrate_species:
            output["actions"]=integrate_species(niagara,editor_python)
        output["result"]="author_actions_submitted"
        # Niagara编辑器编译成功≠真实GPU绑定完成；不把此结果写为正式视觉通过。
    except Exception as e:
        output["result"]="failed"
        output["error"]=str(e)
        raise
    finally:
        (REPORT/"NiagaraSplineAuthoring.json").write_text(
            json.dumps(output,ensure_ascii=False,indent=2,default=str)+"\n",encoding="utf-8")
    print("BIRD_SPLINE_NIAGARA_AUTHOR_ACTIONS_SUBMITTED",len(output["actions"]))


if __name__=="__main__":
    main()
