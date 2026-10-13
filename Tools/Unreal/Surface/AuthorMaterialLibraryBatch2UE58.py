#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 平台环境 / 植被 / 水体 / VFX 中性遮罩函数第二批作者。

职责：为已有GamePlatformSurface（平台表面）和GamePlatformVFX（平台特效）插件
增量生成可复用的真实UMaterialFunction及独立Development审核UMaterial。
绝不移动/清空已有资产，不操作用户正式地图、主题UI或服务器业务逻辑。
模式 inspect（默认，只读显示）| apply（在正式UE编辑器原生创建/编译/保存）| verify（只读审查）。
中间失败时暂停，未保存的QA内存半成品需要手动审查，不自动销毁。
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
QA = "/Game/Development/MaterialLibrary/Materials"
QADISK = ROOT / "Game/Content/Development/MaterialLibrary/Materials"
ENV = "Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Content"
VFX = "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content"

# 每项具有稳定的201台账唯一能力编号、物理归属、输入协议、运算语义及输出。
# 数值输入统一为0..1区间（明确距离、时间和方向例外）；复杂水面母版不是本批职责。
SPECS = [
 ("MAT-02-02","SnowThicknessMask","Surface","Layers",[("SnowAmount","S",.7),("LocalSnowDepthCm","S",25),("MaxSnowDepthCm","S",50),("SlopeAvailability","S",.85)],
  ["sat",["mul",["div","LocalSnowDepthCm","MaxSnowDepthCm"],["mul","SnowAmount","SlopeAvailability"]]],"积雪厚度和坡度权重"),
 ("MAT-02-03","WindDrivenSnow","Surface","Layers",[("SnowAmount","S",.8),("SlopeAvailability","S",.9),("WindFacing","S",.6),("WindStrength","S",.3)],
  ["sat",["mul",["mul","SnowAmount","SlopeAvailability"],["add",1,["mul","WindFacing","WindStrength"]]]],"迎风积雪加权"),
 ("MAT-02-04","SnowMelting","Surface","Layers",[("SnowCover","S",.8),("MeltFactor","S",.3),("SunExposure","S",.9)],
  ["sat",["mul","SnowCover",["one_minus",["sat",["mul","MeltFactor","SunExposure"]]]]],"融雪后剩余积雪"),
 ("MAT-02-06","IceCoverage","Surface","Layers",[("SurfaceWetness","S",.6),("FreezeFactor","S",.5),("TemperatureColdFactor","S",.9)],
  ["sat",["mul","SurfaceWetness",["mul","FreezeFactor","TemperatureColdFactor"]]],"温度驱动表面冰覆盖"),
 ("MAT-02-10","RainStreaks","Surface","Layers",[("RainIntensity","S",.75),("VerticalExposure","S",.8),("FlowNoise","S",.5),("StreakContrast","S",1.2)],
  ["sat",["mul",["mul","RainIntensity","VerticalExposure"],["mul","FlowNoise","StreakContrast"]]],"垂直表面雨痕遮罩"),
 ("MAT-02-11","RainDrying","Surface","Layers",[("CurrentWetness","S",.7),("DryingFactor","S",.6),("ElapsedNormalized","S",.3)],
  ["sat",["mul","CurrentWetness",["one_minus",["sat",["mul","DryingFactor","ElapsedNormalized"]]]]],"蒸发后剩余湿润"),
 ("MAT-02-16","SurfaceCondensation","Surface","Layers",[("HumidityFactor","S",.65),("ColdSurfaceFactor","S",.8),("Affinity","S",.7)],
  ["sat",["mul","HumidityFactor",["mul","ColdSurfaceFactor","Affinity"]]],"凝露敏感度"),
 ("MAT-03-06","GrassEdgeMask","Surface","Layers",[("GrassCoverage","S",1),("DistanceToEdgeCm","S",18),("EdgeFadeWidthCm","S",60)],
  ["sat",["mul","GrassCoverage",["one_minus",["sat",["div","DistanceToEdgeCm","EdgeFadeWidthCm"]]]]],"草丛边缘宽度"),
 ("MAT-03-07","RockErosion","Surface","Layers",[("NoiseMask","S",.6),("SlopeMask","S",.8),("RainExposure","S",.7),("Porosity","S",.65)],
  ["sat",["mul",["mul","NoiseMask","SlopeMask"],["mul","RainExposure","Porosity"]]],"岩石侵蚀分布"),
 ("MAT-03-08","SoilWash","Surface","Layers",[("RainAmount","S",.7),("LooseSoil","S",.6),("FlowMask","S",.8)],
  ["sat",["mul","RainAmount",["mul","LooseSoil","FlowMask"]]],"土壤冲刷沉积"),
 ("MAT-03-10","CrevicePlantMask","Surface","Layers",[("CavityMask","S",.8),("Humidity","S",.7),("LightAvailability","S",.6),("Fertility","S",.9)],
  ["sat",["mul",["mul","CavityMask","Humidity"],["mul","LightAvailability","Fertility"]]],"石缝适生遮罩"),
 ("MAT-05-04","ShoreFoam","Surface","Water",[("ShoreDistanceNormalized","S",.1),("WaveEnergy","S",.7),("FoamNoise","S",.85)],
  ["sat",["mul",["one_minus",["sat","ShoreDistanceNormalized"]],["mul","WaveEnergy","FoamNoise"]]],"岸边浪花近岸权重"),
 ("MAT-05-05","CrestFoam","Surface","Water",[("CrestHeightFactor","S",.8),("WaveEnergy","S",.75),("FoamNoise","S",.7)],
  ["sat",["mul","CrestHeightFactor",["mul","WaveEnergy","FoamNoise"]]],"波峰泡沫权重"),
 ("MAT-06-07","FoliageSnow","Surface","Foliage",[("SnowAmount","S",.8),("UpFacingMask","S",.75),("OcclusionMask","S",.15)],
  ["sat",["mul",["mul","SnowAmount","UpFacingMask"],["one_minus",["sat","OcclusionMask"]]]],"叶片积雪朝向与遮挡"),
 ("MAT-06-08","FoliageWetness","Surface","Foliage",[("GlobalWetness","S",.6),("RainExposure","S",.8),("LeafAbsorption","S",.9)],
  ["sat",["mul","GlobalWetness",["mul","RainExposure","LeafAbsorption"]]],"叶片受雨湿润"),
 ("MAT-09-08","CameraDistanceFade","VFX","VFX",[("NearDistanceCm","S",300),("FarDistanceCm","S",2300)],
  ["sat",["div",["sub","pixel_depth","NearDistanceCm"],["sub","FarDistanceCm","NearDistanceCm"]]],"按摄像机距离渐隐遮罩"),
 ("MAT-09-13","EmissivePulse","VFX","VFX",[("TimeSeconds","S",.25),("PulseFrequencyHz","S",1.0),("MinIntensity","S",.1),("MaxIntensity","S",1.5)],
  ["add","MinIntensity",["mul",["sub","MaxIntensity","MinIntensity"],["sat",["add",.5,["mul",.5,["sin",["mul","TimeSeconds","PulseFrequencyHz"]]]]]]],"可调频率发光脉冲"),
 ("MAT-09-16","ShockwaveRingMask","VFX","VFX",[("UV","V2",(.5,.5)),("CenterUV","V2",(.5,.5)),("RingRadius","S",.4),("RingWidth","S",.1)],
  ["one_minus",["sat",["div",["abs",["sub",["distance","UV","CenterUV"],"RingRadius"]],"RingWidth"]]],"二维冲击波环形遮罩")
]


def path_for(item) -> str:
    """平台内容挂载点须是既有插件，不能生成平行库。"""
    _, name, owner, folder, *_ = item
    if owner == "Surface":
        return f"/GamePlatformSurface/MaterialFunctions/{folder}/MF_GP_{name}"
    return f"/GamePlatformVFX/Materials/Functions/MF_GP_VFX_{name}"


def disk_for(item) -> Path:
    """空AssetRegistry结果不能视为可写：同时核对磁盘同名及侧车文件。"""
    rel = path_for(item)
    owner = item[2]
    if owner == "Surface":
        return ROOT / ENV / (rel.removeprefix("/GamePlatformSurface/") + ".uasset")
    return ROOT / VFX / (rel.removeprefix("/GamePlatformVFX/") + ".uasset")


def precheck_file(file: Path) -> None:
    """只允许创建全新的引擎资产包，发现任何物理占用即阻断。"""
    if any(file.with_suffix(s).exists() for s in (".uasset",".uexp",".ubulk",".umap")):
        raise RuntimeError("同名原生资产或侧车文件已存在但未注册：" + str(file))


def require_editor(unreal) -> None:
    """编辑器内生成才可视为MaterialFunction真实制作。"""
    project = str(unreal.Paths.get_project_file_path()).replace("\\", "/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("禁止在其它UE项目中创建材质函数：" + project)


def author_function(unreal, asset, item) -> int:
    """把正式语义运算表达为原生连线图，输出为单个Scalar遮罩。"""
    _, name, _, _, pins, tree, _ = item
    edit = unreal.MaterialEditingLibrary

    def node(name_of_class: str, x: int, y: int):
        k = getattr(unreal, "MaterialExpression" + name_of_class, None)
        if k is None:
            raise RuntimeError("缺少UE原生表达式：" + name_of_class)
        n = edit.create_material_expression_in_function(asset,k,x,y)
        if n is None:
            raise RuntimeError("材质函数节点创建失败：" + name_of_class)
        return n

    def link(source, target, which: str) -> None:
        done = edit.connect_material_expressions(source,"",target,which)
        if not done and which in {"Input","VectorInput"}:
            done = edit.connect_material_expressions(source,"",target,"")
        if not done:
            raise RuntimeError("材质表达式真实接线失败：" + str(which) + " " + name)

    values = {}
    for idx,(pname,typ,default) in enumerate(pins):
        inp = node("FunctionInput",-1700,idx*170-650)
        inp.set_editor_property("input_name",pname)
        enum_name = "FUNCTION_INPUT_SCALAR" if typ=="S" else "FUNCTION_INPUT_VECTOR2"
        inp.set_editor_property("input_type",getattr(unreal.FunctionInputType,enum_name))
        inp.set_editor_property("use_preview_value_as_default",False)
        values[pname]=inp

    counter=[0]
    def const(value):
        counter[0]+=1
        if isinstance(value,(float,int)):
            n=node("Constant",-1350+((counter[0]%6)*160),880+(counter[0]//6)*170)
            n.set_editor_property("r",float(value))
            return n
        raise TypeError("非标量常量未经设计，不得创建：" + repr(value))

    def expr(tree):
        if isinstance(tree,str):
            if tree in values:return values[tree]
            if tree=="pixel_depth":return node("PixelDepth",-1300,750)
            raise ValueError("未声明的材质函数输入：" + tree)
        if isinstance(tree,(float,int)):
            return const(tree)
        if not isinstance(tree,list) or not tree:
            raise TypeError("运算树必须是列表且有运算符")
        op=tree[0]
        funcs={"add":"Add","sub":"Subtract","mul":"Multiply","max":"Max",
               "min":"Min","distance":"Distance","abs":"Abs","sin":"Sine",
               "sat":"Saturate","one_minus":"OneMinus"}
        if op=="div":
            # 避免零分母；宽度、距离/厚度范围均为正参数（非带符号流向）。
            a=expr(tree[1]);b=expr(tree[2]);eps=const(.0001)
            bounded=node("Max",-480,counter[0]*60-300)
            link(b,bounded,"A");link(eps,bounded,"B")
            n=node("Divide",-220,counter[0]*60-300)
            link(a,n,"A");link(bounded,n,"B")
            return n
        if op not in funcs:raise ValueError("无法创建未知材质运算：" + op)
        cls=funcs[op]
        counter[0]+=1
        n=node(cls, -980+((counter[0]%7)*130),counter[0]*110-600)
        if len(tree)==2:
            if op not in {"abs","sin","sat","one_minus"}:
                raise ValueError("二元运算缺失输入："+op)
            link(expr(tree[1]),n,"Input")
        elif len(tree)==3:
            link(expr(tree[1]),n,"A")
            link(expr(tree[2]),n,"B")
        else:
            raise ValueError("材质运算参数个数不正确："+op)
        return n

    output_source=expr(tree)
    out=node("FunctionOutput",550,0)
    out.set_editor_property("output_name","Mask")
    link(output_source,out,"Input")
    count=edit.get_num_material_expressions_in_function(asset)
    if count < len(pins)+3:
        raise RuntimeError("材质函数节点数不符合运算图期望："+name)
    return count


def make_review(unreal, item):
    """真实测试材质必须引用平台MF、具有独立输入常量和实连的Shader BaseColor。"""
    edit=unreal.MaterialEditingLibrary
    lib=unreal.EditorAssetLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    cid,name,owner,folder,pins,formula,zh=item
    path=f"{QA}/M_DBA_MaterialLibrary_{name}_Review"
    fn=lib.load_asset(path_for(item))
    if not isinstance(fn,unreal.MaterialFunction):
        raise TypeError("审核材质缺少真实MaterialFunction："+path_for(item))
    disk=QADISK/(path.rsplit("/",1)[-1]+".uasset")
    old=lib.load_asset(path) if lib.does_asset_exist(path) else None
    if old is not None:
        if not isinstance(old,unreal.Material):
            raise RuntimeError("目标QA路径已有其它类型："+path)
        count=edit.get_num_material_expressions(old)
        if count < len(pins)+1:
            raise RuntimeError("既有QA母版图不完整，拒绝覆盖："+path)
        print("QA_EXISTING_VERIFIED",path,count)
        return
    precheck_file(disk)
    if not lib.does_directory_exist(QA):
        lib.make_directory(QA)
    mat=tools.create_asset(path.rsplit("/",1)[-1],QA,unreal.Material,unreal.MaterialFactoryNew())
    if not isinstance(mat,unreal.Material):
        raise RuntimeError("UE真实审核UMaterial创建失败："+path)
    def m_node(name_of_class, x,y):
        k=getattr(unreal,"MaterialExpression"+name_of_class,None)
        if k is None:raise RuntimeError("UE缺少材质审核节点："+name_of_class)
        result=edit.create_material_expression(mat,k,x,y)
        if result is None:raise RuntimeError("审核材质创建节点失败："+name_of_class)
        return result
    call=m_node("MaterialFunctionCall",-350,-200)
    if not call.set_material_function(fn):
        raise RuntimeError("QA无法引用真实函数："+path_for(item))
    for idx,(pname,typ,default) in enumerate(pins):
        if typ=="S":
            c=m_node("Constant",-1050,-650+idx*160)
            c.set_editor_property("r",float(default))
        else:
            c=m_node("Constant2Vector",-1050,-650+idx*160)
            c.set_editor_property("r",float(default[0]))
            c.set_editor_property("g",float(default[1]))
        if not edit.connect_material_expressions(c,"",call,pname):
            raise RuntimeError("审核材料的函数调用引脚无法连接："+pname)
    if not edit.connect_material_property(call,"Mask",unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError("审核材料无法连接基础色Shader输出："+path)
    diagnostics=list(edit.recompile_material(mat) or [])
    for message in diagnostics:
        print("SHADER_DIAGNOSTIC",name,str(message))
        if "error" in str(message).lower():
            raise RuntimeError("UE真实Material Shader报错："+name)
    if not lib.save_loaded_asset(mat,only_if_is_dirty=False):
        raise RuntimeError("UE保存审核材质失败："+path)
    reopened=lib.load_asset(path)
    if not isinstance(reopened,unreal.Material) or edit.get_num_material_expressions(reopened)<len(pins)+1:
        raise RuntimeError("审核材质回读失败或节点不足："+path)
    print("QA_COMPILED_SAVED",path,"nodes",edit.get_num_material_expressions(reopened))


def main():
    """两个阶段均拒绝替换既有正式资产，遇失败明确退出，不假装全批成功。"""
    parser=argparse.ArgumentParser(description="通用材质函数第二批 UE5.8 增量作者")
    parser.add_argument("--mode",choices=("inspect","apply","verify","review"),
                        default=os.getenv("MATERIAL_LIBRARY_B2_MODE","inspect"))
    ns,_=parser.parse_known_args()
    for item in SPECS:
        print("LIBRARY_B2_PLAN",item[0],item[1],item[2],path_for(item),item[-1])
    if ns.mode=="inspect":
        print("INSPECT_ONLY：非UE文件创建模式")
        return
    try:import unreal # type: ignore[import-not-found]
    except ImportError as err:raise RuntimeError("必须在真实UE5.8虚幻编辑器中制作和验收") from err
    require_editor(unreal)
    lib=unreal.EditorAssetLibrary
    edit=unreal.MaterialEditingLibrary
    reg=unreal.AssetRegistryHelpers.get_asset_registry()
    reg.scan_paths_synchronous(["/GamePlatformSurface","/GamePlatformVFX",QA],force_rescan=True)
    # 整批保护性预检：先检查全部目标，避免创建部分后才发现历史冲突。
    for item in SPECS:
        path=path_for(item)
        found=lib.load_asset(path) if lib.does_asset_exist(path) else None
        if found is not None and not isinstance(found,unreal.MaterialFunction):
            raise TypeError("同名资产类别冲突："+path)
        if found is None and ns.mode in ("apply","verify","review"):
            precheck_file(disk_for(item))
        if ns.mode in ("verify","review") and found is None:
            raise FileNotFoundError("正式UE函数尚未创建："+path)
        if ns.mode=="review":
            review_path=QA+"/M_DBA_MaterialLibrary_"+item[1]+"_Review"
            review_existing=lib.load_asset(review_path) if lib.does_asset_exist(review_path) else None
            if review_existing is None: precheck_file(QADISK/(review_path.rsplit("/",1)[-1]+".uasset"))
            elif not isinstance(review_existing,unreal.Material):
                raise TypeError("QA母材质目标身份冲突："+review_path)

    if ns.mode=="review":
        for item in SPECS:make_review(unreal,item)
        print("LIBRARY_B2_REVIEW_DONE",len(SPECS))
        return

    tools=unreal.AssetToolsHelpers.get_asset_tools()
    factory=unreal.MaterialFunctionFactoryNew()
    for item in SPECS:
        path=path_for(item);cid,name,owner,folder,pins,tree,description=item
        if lib.does_asset_exist(path):
            found=lib.load_asset(path)
            if not isinstance(found,unreal.MaterialFunction):
                raise TypeError("已有平台资产类型变更："+path)
            count=edit.get_num_material_expressions_in_function(found)
            if count < len(pins)+3:
                raise RuntimeError("旧函数存在但不完整，禁止覆盖："+path)
            print("FUNCTION_EXISTING_VERIFIED",cid,path,"nodes",count)
            continue
        if ns.mode=="verify":
            raise FileNotFoundError("只读验证缺失UE函数："+path)
        folder_path=path.rsplit("/",1)[0]
        if not lib.does_directory_exist(folder_path):lib.make_directory(folder_path)
        func=tools.create_asset(path.rsplit("/",1)[-1],folder_path,unreal.MaterialFunction,factory)
        if not isinstance(func,unreal.MaterialFunction):
            raise RuntimeError("UE材质函数创建失败："+path)
        count=author_function(unreal,func,item)
        func.set_editor_property("description",description+"；平台中性视觉逻辑，不参与服务端权威结算。")
        func.set_editor_property("expose_to_library",True)
        edit.update_material_function(func)
        if not lib.save_loaded_asset(func,only_if_is_dirty=False):
            raise RuntimeError("UE材质函数未成功保存："+path)
        reopened=lib.load_asset(path)
        if not isinstance(reopened,unreal.MaterialFunction) or edit.get_num_material_expressions_in_function(reopened)<len(pins)+3:
            raise RuntimeError("UE材质函数保存回读失败："+path)
        print("FUNCTION_CREATED_SAVED",cid,path,"nodes",count)
    print("LIBRARY_B2_FUNCTIONS_DONE",len(SPECS))


if __name__=="__main__":
    main()
