#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8平台表面材质层MaterialLayer / MaterialLayerBlend原生制作者。

复用GamePlatformSurface（平台环境表面）单一插件；十二个配方来自
MaterialLibraryLayerProfiles_V1.json（平台层PBR覆盖配方），
只输出真实Material Attributes（完整材质属性）而非空白层。
遮罩混合为单个复用MLB，12个层不复制第二套环境或天气状态引擎。
默认inspect无副作用；apply仅新建，发现已有资产一律只读核对，不覆盖或清空。
verify仅只读检查；compile留到代码、蓝图及资源全部实现后统一执行。
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
PROFILE=ROOT/"Tools/Unreal/Surface/MaterialLibraryLayerProfiles_V1.json"
FOLDER="/GamePlatformSurface/MaterialLayers"
DISK=ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Content/MaterialLayers"
BLEND=FOLDER+"/MLB_GP_CoverageBlend"


def identities():
    """配方身份必须由201项台账登记，任何临时材料项不得冒充正式材质层。"""
    catalog=json.loads(PROFILE.read_text(encoding="utf8"))
    all_rows=json.loads((ROOT/"Docs/Implementation/Materials/MaterialCapabilityRegistry_V1.json").read_text(encoding="utf8"))
    names={r["CapabilityId"]:r for r in all_rows["items"]}
    layer_rows=catalog.get("items",[])
    if len(layer_rows)!=12:
        raise RuntimeError("材质层必须为12个正式MaterialLayer候选，不得自动填充")
    seen=set()
    for row in layer_rows:
        cid=row["capabilityId"]
        if cid in seen:raise ValueError("材料层能力编号重复："+cid)
        seen.add(cid)
        original=names.get(cid)
        if (not original or original["ImplementationMode"]!="ML" or
            original["NameEN"]!=row["name"] or
            original["OwnerPlugin"]!="GamePlatformSurface"):
            raise RuntimeError("PBR Layer归属或名称与201项索引冲突："+cid)
        if row["materialLayerAssetPath"]!=FOLDER+"/ML_GP_"+row["name"]:
            raise RuntimeError("材质层资产路径不合规："+cid)
        if len(row["baseColor"])!=3 or not all(isinstance(v,(float,int)) for v in row["baseColor"]):
            raise ValueError("材料层基础色默认值缺失："+cid)
        if not all(0<=float(row[x])<=1 for x in ("roughness","metallic","coverage")):
            raise ValueError("材料层PBR参数越界："+cid)
    return layer_rows


def require_editor(unreal):
    """禁止普通系统Python写入虚幻二进制资源。"""
    active=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not active.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("当前不是正式UE5.8神兽联盟项目："+active)


def file_is_occupied(path):
    """将包路径换算为物理文件并检查侧车，防止同名资源被部分索引漏掉。"""
    filename=DISK/(path.rsplit("/",1)[-1]+".uasset")
    return any(filename.with_suffix(ext).exists() for ext in (".uasset",".uexp",".ubulk",".umap"))


def expect_type(unreal,path,cls,allow_missing):
    """已存在资产只检查，不进行清图、重命名或资产类型转换。"""
    lib=unreal.EditorAssetLibrary
    obj=lib.load_asset(path) if lib.does_asset_exist(path) else None
    if obj is not None and not isinstance(obj,cls):
        raise TypeError("已有平台层材质Layer路径被其它类型占用："+path)
    if obj is None:
        if file_is_occupied(path):
            raise RuntimeError("磁盘包存在但未注册，禁止重建覆盖："+path)
        if not allow_missing:
            raise FileNotFoundError("正式原生MaterialLayer尚未创建："+path)
    return obj


def create_node(unreal,function,short,x,y):
    """原生材质节点由UE编辑器创建，无伪二进制文件。"""
    klass=getattr(unreal,"MaterialExpression"+short,None)
    if klass is None:
        raise RuntimeError("当前UE5.8不支持所需MaterialLayer表达式："+short)
    node=unreal.MaterialEditingLibrary.create_material_expression_in_function(
        function,klass,int(x),int(y))
    if node is None:
        raise RuntimeError("UE原生材质层节点创建失败："+short)
    return node


def connect(unreal,left,right,pin):
    edit=unreal.MaterialEditingLibrary
    valid=edit.connect_material_expressions(left,"",right,pin)
    if not valid and pin in ("Input","VectorInput"):
        valid=edit.connect_material_expressions(left,"",right,"")
    if not valid:
        raise RuntimeError("MaterialLayer输入引脚未连接："+pin)
    

def create_blend(unreal):
    """单一MaterialLayerBlend混合：底层属性 + 顶层属性 + 独立0~1覆盖遮罩。"""
    assets=unreal.EditorAssetLibrary
    factory=unreal.MaterialFunctionFactoryNew()
    blend=unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        BLEND.rsplit("/",1)[-1],FOLDER,unreal.MaterialFunctionMaterialLayerBlend,factory)
    if not isinstance(blend,unreal.MaterialFunctionMaterialLayerBlend):
        raise TypeError("UE编辑器没有创建MaterialLayerBlend原生子类")
    nodes=[]
    for index,(name,kind) in enumerate((("BaseAttributes","FUNCTION_INPUT_MATERIAL_ATTRIBUTES"),
                                       ("TopAttributes","FUNCTION_INPUT_MATERIAL_ATTRIBUTES"),
                                       ("CoverageMask","FUNCTION_INPUT_SCALAR"))):
        n=create_node(unreal,blend,"FunctionInput",-1050,index*230-420)
        n.set_editor_property("input_name",name)
        n.set_editor_property("input_type",getattr(unreal.FunctionInputType,kind))
        n.set_editor_property("use_preview_value_as_default",False)
        nodes.append(n)
    combine=create_node(unreal,blend,"BlendMaterialAttributes",-390,-70)
    for source,pin in zip(nodes,("A","B","Alpha")):
        connect(unreal,source,combine,pin)
    out=create_node(unreal,blend,"FunctionOutput",280,-70)
    out.set_editor_property("output_name","MaterialAttributes")
    connect(unreal,combine,out,"Input")
    if unreal.MaterialEditingLibrary.get_num_material_expressions_in_function(blend)<5:
        raise RuntimeError("MaterialLayerBlend真实属性连接不完整")
    blend.set_editor_property("description","平台MaterialAttributes材质覆盖混合；统一遮罩，雨雪与自然覆盖由上层控制。")
    blend.set_editor_property("expose_to_library",True)
    unreal.MaterialEditingLibrary.update_material_function(blend)
    if not assets.save_loaded_asset(blend,only_if_is_dirty=False):
        raise RuntimeError("无法保存平台MaterialLayerBlend二进制资产")
    print("MATERIAL_LAYER_BLEND_SAVED",BLEND)


def create_layer(unreal,info):
    """真正PBR覆盖层：独立颜色、粗糙度、金属度与AO属性，不复用单纯颜色遮罩。"""
    path=info["materialLayerAssetPath"]
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    layer=tools.create_asset(path.rsplit("/",1)[-1],FOLDER,
                             unreal.MaterialFunctionMaterialLayer,unreal.MaterialFunctionFactoryNew())
    if not isinstance(layer,unreal.MaterialFunctionMaterialLayer):
        raise TypeError("UE无法创建原生MaterialFunctionMaterialLayer："+path)
    def vec_param(name,color,x,y):
        n=create_node(unreal,layer,"VectorParameter",x,y)
        n.set_editor_property("parameter_name",name)
        n.set_editor_property("default_value",unreal.LinearColor(*[float(v) for v in color],1.0))
        return n
    def scalar_param(name,value,x,y):
        n=create_node(unreal,layer,"ScalarParameter",x,y)
        n.set_editor_property("parameter_name",name)
        n.set_editor_property("default_value",float(value))
        return n
    color=vec_param("GP_Layer_BaseColor",info["baseColor"],-1230,-650)
    rough=scalar_param("GP_Layer_Roughness",info["roughness"],-1230,-430)
    metal=scalar_param("GP_Layer_Metallic",info["metallic"],-1230,-220)
    ao=scalar_param("GP_Layer_AmbientOcclusion",1.0,-1230,0)
    normal=create_node(unreal,layer,"Constant3Vector",-1230,220)
    normal.set_editor_property("constant",unreal.LinearColor(0,0,1,1))
    attrs=create_node(unreal,layer,"MakeMaterialAttributes",-570,-150)
    for source,pin in ((color,"BaseColor"),(rough,"Roughness"),(metal,"Metallic"),
                       (ao,"AmbientOcclusion"),(normal,"Normal")):
        connect(unreal,source,attrs,pin)
    out=create_node(unreal,layer,"FunctionOutput",110,-180)
    out.set_editor_property("output_name","MaterialAttributes")
    connect(unreal,attrs,out,"Input")
    nodes=unreal.MaterialEditingLibrary.get_num_material_expressions_in_function(layer)
    if nodes<7:
        raise RuntimeError("MaterialLayer实际PBR属性图不完整："+path)
    layer.set_editor_property("description",info["nameZH"]+"（PBR材质层）；Coverage在平台MaterialLayerBlend外控制。")
    layer.set_editor_property("expose_to_library",True)
    unreal.MaterialEditingLibrary.update_material_function(layer)
    if not unreal.EditorAssetLibrary.save_loaded_asset(layer,only_if_is_dirty=False):
        raise RuntimeError("真实MaterialLayer未保存："+path)
    loaded=unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(loaded,unreal.MaterialFunctionMaterialLayer):
        raise RuntimeError("MaterialLayer保存后原生类型回读失败："+path)
    print("MATERIAL_LAYER_SAVED",info["capabilityId"],path,"nodes",nodes)


def main():
    """先资源后统一编译，不在apply中调用全量编译或修改正式场景。"""
    p=argparse.ArgumentParser(description="12个PBR覆盖材质层及共享属性混合UE5.8制作")
    p.add_argument("--mode",choices=("inspect","apply","verify"),default=os.getenv("MATERIAL_LAYER_MODE","inspect"))
    opt,_=p.parse_known_args()
    items=identities()
    for item in items:
        print("LAYER_PLAN",item["capabilityId"],item["materialLayerAssetPath"],item["nameZH"])
    print("LAYER_BLEND_PLAN",BLEND)
    if opt.mode=="inspect":
        print("INSPECT_ONLY：不写任何UE二进制资产")
        return
    try:
        import unreal # type: ignore[import-not-found]
    except ImportError as error:
        raise RuntimeError("需要实际UE5.8编辑器，不支持普通Python写Layer资源") from error
    require_editor(unreal)
    reg=unreal.AssetRegistryHelpers.get_asset_registry()
    reg.scan_paths_synchronous(["/GamePlatformSurface"],force_rescan=True)
    if not hasattr(unreal,"MaterialFunctionMaterialLayer") or not hasattr(unreal,"MaterialFunctionMaterialLayerBlend"):
        raise RuntimeError("锁定UE5.8未暴露MaterialLayer/MaterialLayerBlend原生类")
    missing=(opt.mode=="apply")
    main_blend=expect_type(unreal,BLEND,unreal.MaterialFunctionMaterialLayerBlend,missing)
    for item in items:
        expect_type(unreal,item["materialLayerAssetPath"],unreal.MaterialFunctionMaterialLayer,missing)
    if opt.mode=="verify":
        print("ALL_MATERIAL_LAYER_NATIVE_TYPES_VERIFIED",len(items))
        return
    if not unreal.EditorAssetLibrary.does_directory_exist(FOLDER):
        unreal.EditorAssetLibrary.make_directory(FOLDER)
    if main_blend is None:create_blend(unreal)
    else:print("MATERIAL_LAYER_BLEND_PRESERVED",BLEND)
    for item in items:
        if unreal.EditorAssetLibrary.does_asset_exist(item["materialLayerAssetPath"]):
            print("MATERIAL_LAYER_EXISTING_PRESERVED",item["materialLayerAssetPath"])
            continue
        create_layer(unreal,item)
    print("MATERIAL_LAYER_AUTHOR_DONE",len(items),"加共享MaterialLayerBlend；真实Shader和Cook需最后验收")


if __name__=="__main__":
    main()
