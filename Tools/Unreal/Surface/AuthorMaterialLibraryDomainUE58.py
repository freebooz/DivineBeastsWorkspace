#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 通用材质能力库的领域函数作者：50项平台 + 10项项目视觉。

层级：GamePlatformSurface（环境/水体/植被/道具着色）、GamePlatformVFX（粒子视觉）
与 DBAPresentationPack_Core（神兽联盟项目角色材质）三种唯一归属。
本工具只把已审批的不同数学配方变为真实MaterialFunction（材质函数），不生成空图、
不伪造.uasset、不替换已有母材质、用户地图与蓝图。UI函数由Monolith制作，不走此脚本。
模式：inspect（只读计划）、apply（原生函数创建）、review（隔离开发审核母材质，
先建图与保存）、verify（仅核对真实包和图）、compile（统一编译审核资产）。
在所有代码、蓝图、资源创建完毕前，不使用compile模式。
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
RECIPES = (
    ROOT / "Tools/Unreal/Surface/MaterialLibraryDomainRecipes_Core.json",
    ROOT / "Tools/Unreal/Surface/MaterialLibraryDomainRecipes_World.json",
    ROOT / "Tools/Unreal/Surface/MaterialLibraryDomainRecipes_Surface.json",
    ROOT / "Tools/Unreal/Surface/MaterialLibraryDomainRecipes_Project.json",
)
INDEX = ROOT / "Docs/Implementation/Materials/MaterialCapabilityRegistry_V1.json"
REVIEW_ROOT = "/Game/Development/MaterialLibrary/Materials"
ROOTS = {
    "Surface": ("/GamePlatformSurface", ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Content"),
    "VFX": ("/GamePlatformVFX", ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content"),
    "Project": ("/DBAPresentationPack_Core", ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content"),
}
KNOWN_OPERATIONS = {"add","sub","mul","div","min","max","power","dot","distance",
                    "sat","one_minus","abs","sin","cos","frac","floor","sqrt",
                    "normalize","lerp","append","channel","v2","v3"}


def load_catalog():
    """用201项台账验证真实能力身份；缺任一分领域文件时拒绝“全库完成”。"""
    data = json.loads(INDEX.read_text(encoding="utf-8"))
    by_id = {r["CapabilityId"]:r for r in data["items"]}
    if len(by_id)!=201:
        raise ValueError("通用材质函数能力唯一索引不是201条")
    listed = []
    for file in RECIPES:
        if not file.is_file():
            raise FileNotFoundError("正式分领域材质算法文件不存在：" + str(file))
        source=json.loads(file.read_text(encoding="utf-8"))
        if source.get("schemaVersion")!=1:
            raise ValueError("材质算法文件Schema不兼容：" + str(file))
        for item in source.get("functions",[]):
            cid=item.get("id","")
            owner=item.get("owner")
            original=by_id.get(cid)
            if not original or original["NameEN"]!=item.get("name"):
                raise ValueError("算法未映射201项正式索引："+cid)
            if owner not in ROOTS:
                raise ValueError("错误的函数所属插件："+str(owner))
            expected_owner={"Surface":"GamePlatformSurface","VFX":"GamePlatformVFX",
                            "Project":"DBAPresentationPack_Core"}[owner]
            if expected_owner != original["OwnerPlugin"]:
                raise ValueError("材质函数跨层归属与原工程台账冲突："+cid)
            if original["ImplementationMode"]!="MF":
                raise ValueError("非MF能力不应为凑数生成材质函数："+cid)
            if owner!="Project" and original.get("ProposedAssetPath")!=asset_path(item):
                raise ValueError("原平台函数建议路径身份发生漂移："+cid)
            assert_valid_recipe(item)
            listed.append(item)
    if len({x["id"] for x in listed}) != len(listed):
        raise ValueError("函数算法编号重复")
    return listed


def asset_path(item):
    """分别定位平台环境、平台视觉和第三层内容包资源。"""
    name=item["name"]
    owner=item["owner"]
    if owner=="Surface":
        return "/GamePlatformSurface/MaterialFunctions/"+item["folder"]+"/MF_GP_"+name
    if owner=="VFX":
        return "/GamePlatformVFX/Materials/Functions/MF_GP_VFX_"+name
    if owner=="Project":
        return "/DBAPresentationPack_Core/MaterialFunctions/Character/MF_DBA_"+name
    raise ValueError("不允许的插件："+owner)


def disk_path(item):
    """已注册资产之外还检查文件系统，避免Unreal加载旧包时发生覆盖。"""
    mount,folder=ROOTS[item["owner"]]
    return folder/(asset_path(item).removeprefix(mount+"/")+".uasset")


def validate_tree(tree, inputs):
    """编译前静态拒绝空表达式与未知参数，确保每条公式有实质数学意义。"""
    if isinstance(tree,(float,int)):
        return
    if isinstance(tree,str):
        if tree not in inputs:
            raise ValueError("算法引用未声明材质输入："+tree)
        return
    if not isinstance(tree,list) or not tree or tree[0] not in KNOWN_OPERATIONS:
        raise ValueError("不能执行的原生材质运算表达式："+str(tree))
    op=tree[0]
    counts={"sat":1,"one_minus":1,"abs":1,"sin":1,"cos":1,"frac":1,"floor":1,
            "sqrt":1,"normalize":1,"add":2,"sub":2,"mul":2,"div":2,
            "min":2,"max":2,"power":2,"dot":2,"distance":2,"append":2,
            "channel":2,"lerp":3,"v2":2,"v3":3}
    if len(tree)!=counts[op]+1:
        raise ValueError("运算节点输入参数数量错误："+op)
    if op=="channel":
        if tree[2] not in ("R","G","B"):
            raise ValueError("错误的ORM通道标识")
        validate_tree(tree[1],inputs)
        return
    if op in ("v2","v3"):
        if not all(isinstance(v,(int,float)) for v in tree[1:]):
            raise ValueError("向量常量应为数值")
        return
    for expr in tree[1:]:
        validate_tree(expr,inputs)


def assert_valid_recipe(item):
    """逐函数校验参数、输出、路径及非占位实现；避免资产创建中途才发现合同错误。"""
    pins=item.get("pins",[])
    outs=item.get("outputs",[])
    names={p["name"] for p in pins}
    if len(names)!=len(pins) or not 1<=len(pins)<=12:
        raise ValueError("材质函数输入重复或数量异常："+item["id"])
    if not 1<=len(outs)<=3 or len({o["name"] for o in outs})!=len(outs):
        raise ValueError("材质函数输出不完整："+item["id"])
    for pin in pins:
        if pin["type"] not in {"S","V2","V3"}:
            raise ValueError("材质输入不支持的类型："+str(pin))
        val=pin.get("preview")
        if pin["type"]=="S" and not isinstance(val,(float,int)):
            raise TypeError("数值参数应有默认预览数值")
        if pin["type"]=="V2" and (not isinstance(val,list) or len(val)!=2):
            raise TypeError("二维向量预览默认值必须是两个数值")
        if pin["type"]=="V3" and (not isinstance(val,list) or len(val)!=3):
            raise TypeError("三维向量预览默认值必须是三个数值")
    for out in outs:
        validate_tree(out["expression"],names)
        if isinstance(out["expression"],str):
            raise ValueError("材质函数输出不得只是输入的空直通："+item["id"])


def require_engine(unreal):
    """只有锁定的虚幻主工程可产生真实二进制资产。"""
    file=str(unreal.Paths.get_project_file_path())
    expected=str(ROOT/"Game/DivineBeastsArena.uproject")
    if os.path.normcase(os.path.realpath(file)) != os.path.normcase(os.path.realpath(expected)):
        raise RuntimeError("仅允许在已锁定的DivineBeastsWorkspace正式工程生成资源："+file)
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("材质函数作者要求UE5.8，禁止用其他引擎版本保存资产")


def safe_target(unreal,item,writable):
    """不修改任何旧材质；未注册资源的物理uasset和侧车文件也应阻断。"""
    ue=unreal.EditorAssetLibrary
    path=asset_path(item)
    existing=ue.load_asset(path) if ue.does_asset_exist(path) else None
    if existing is not None and not isinstance(existing,unreal.MaterialFunction):
        raise TypeError("同名路径是不同的原生资源类型："+path)
    disk=disk_path(item)
    if existing is not None and not disk.is_file():
        raise RuntimeError("原生MF已在注册表中但uasset尚未落盘，拒绝误认成已完成："+path)
    if existing is None and any(disk.with_suffix(ext).exists() for ext in (".uasset",".uexp",".ubulk",".umap")):
        raise RuntimeError("该包磁盘已存在但不在资产注册表，拒绝覆盖："+str(disk))
    return existing


def make_graph(unreal,function,item):
    """构建输入→独立原生数学表达式→输出；每个输出均真实接线。"""
    editing=unreal.MaterialEditingLibrary
    count=[0]
    def node(short,x=0,y=0):
        klass=getattr(unreal,"MaterialExpression"+short,None)
        if klass is None:
            raise RuntimeError("UE5.8没有所需原生材质表达式："+short)
        result=editing.create_material_expression_in_function(function,klass,int(x),int(y))
        if result is None:
            raise RuntimeError("UE没有创建真实材质节点："+short)
        count[0]+=1
        return result
    def link(a,b,pin):
        ok=editing.connect_material_expressions(a,"",b,pin)
        if not ok and pin in {"Input","VectorInput"}:
            ok=editing.connect_material_expressions(a,"",b,"")
        if not ok:
            raise RuntimeError("UE原生函数图引脚没有接通："+str(pin)+" in "+item["id"])
    sources={}
    for ix,p in enumerate(item["pins"]):
        v=node("FunctionInput",-1350,ix*170-650)
        v.set_editor_property("input_name",p["name"])
        dtype={"S":"FUNCTION_INPUT_SCALAR","V2":"FUNCTION_INPUT_VECTOR2",
               "V3":"FUNCTION_INPUT_VECTOR3"}[p["type"]]
        v.set_editor_property("input_type",getattr(unreal.FunctionInputType,dtype))
        v.set_editor_property("use_preview_value_as_default",False)
        sources[p["name"]]=v
    unary={"sat":"Saturate","one_minus":"OneMinus","abs":"Abs","sin":"Sine",
           "cos":"Cosine","frac":"Frac","floor":"Floor","sqrt":"SquareRoot",
           "normalize":"Normalize"}
    binary={"add":"Add","sub":"Subtract","mul":"Multiply","min":"Min",
            "max":"Max","dot":"DotProduct","distance":"Distance",
            "append":"AppendVector","power":"Power"}
    def constant(v):
        if isinstance(v,(float,int)):
            result=node("Constant",-950+count[0]%7*95,900+count[0]//7*165)
            result.set_editor_property("r",float(v))
            return result
        if v[0]=="v2":
            result=node("Constant2Vector",-950+count[0]%7*95,950+count[0]//7*165)
            result.set_editor_property("r",float(v[1]))
            result.set_editor_property("g",float(v[2]))
            return result
        if v[0]=="v3":
            result=node("Constant3Vector",-950+count[0]%7*95,950+count[0]//7*165)
            result.set_editor_property("constant",unreal.LinearColor(float(v[1]),float(v[2]),float(v[3]),1.0))
            return result
        raise TypeError("不支持的算法常量："+repr(v))
    def compile_expr(tree):
        if isinstance(tree,str):
            return sources[tree]
        if isinstance(tree,(float,int)):
            return constant(tree)
        op=tree[0]
        if op in ("v2","v3"):
            return constant(tree)
        if op=="channel":
            mask=node("ComponentMask",count[0]*60-800,count[0]*105-650)
            mask.set_editor_property(tree[2].lower(),True)
            link(compile_expr(tree[1]),mask,"Input")
            return mask
        if op=="div":
            numerator=compile_expr(tree[1]); denominator=compile_expr(tree[2])
            eps=constant(.0001)
            positive=node("Max",count[0]*55-870,count[0]*120-620)
            link(denominator,positive,"A");link(eps,positive,"B")
            target=node("Divide",count[0]*60-700,count[0]*110-600)
            link(numerator,target,"A");link(positive,target,"B")
            return target
        if op in unary:
            source=compile_expr(tree[1])
            target=node(unary[op],count[0]*60-820,count[0]*120-610)
            link(source,target,"Input")
            return target
        if op in binary:
            a=compile_expr(tree[1]);b=compile_expr(tree[2])
            target=node(binary[op],count[0]*60-780,count[0]*120-520)
            if op=="power":
                link(a,target,"Base");link(b,target,"Exp")
            else:
                link(a,target,"A");link(b,target,"B")
            return target
        if op=="lerp":
            a=compile_expr(tree[1]);b=compile_expr(tree[2]);alpha=compile_expr(tree[3])
            target=node("LinearInterpolate",count[0]*65-670,count[0]*110-460)
            link(a,target,"A");link(b,target,"B");link(alpha,target,"Alpha")
            return target
        raise ValueError("未实现的材质表达式："+str(op))
    for ix,o in enumerate(item["outputs"]):
        value=compile_expr(o["expression"])
        target=node("FunctionOutput",700,ix*220-250)
        target.set_editor_property("output_name",o["name"])
        link(value,target,"Input")
    nodes=editing.get_num_material_expressions_in_function(function)
    if nodes<count[0] or nodes<len(item["pins"])+len(item["outputs"])+1:
        raise RuntimeError("原生材质函数图节点或输出未完整创建："+item["id"])
    return nodes


def make_review(unreal,item):
    """开发审核UMaterial需消费真实函数，正式运行/服务器不引用这些独立QA。"""
    edit=unreal.MaterialEditingLibrary
    library=unreal.EditorAssetLibrary
    name=item["name"]
    target=REVIEW_ROOT+"/M_DBA_MaterialLibrary_"+name+"_DomainReview"
    source=library.load_asset(asset_path(item))
    if not isinstance(source,unreal.MaterialFunction):
        raise RuntimeError("审核母版引用的函数未实际生成："+item["id"])
    old=library.load_asset(target) if library.does_asset_exist(target) else None
    if old is not None:
        if not isinstance(old,unreal.Material):
            raise TypeError("QA路径存在非Material资产："+target)
        qa_disk=ROOT/"Game/Content/Development/MaterialLibrary/Materials"/(target.rsplit("/",1)[-1]+".uasset")
        if not qa_disk.is_file():
            raise RuntimeError("QA审核材质仅在内存存在，磁盘没有.uasset："+target)
        if edit.get_num_material_expressions(old)<len(item["pins"])+1:
            raise RuntimeError("已有QA材质图不足，拒绝清图覆盖："+target)
        print("QA_PRESERVED",item["id"],target)
        return
    qa_file=ROOT/"Game/Content/Development/MaterialLibrary/Materials"/(target.rsplit("/",1)[-1]+".uasset")
    if any(qa_file.with_suffix(ext).exists() for ext in (".uasset",".uexp",".ubulk",".umap")):
        raise RuntimeError("未注册QA文件已存在："+str(qa_file))
    if not library.does_directory_exist(REVIEW_ROOT):
        library.make_directory(REVIEW_ROOT)
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        target.rsplit("/",1)[-1],REVIEW_ROOT,unreal.Material,unreal.MaterialFactoryNew())
    if not isinstance(mat,unreal.Material):
        raise RuntimeError("未实际生成审核材质："+target)
    call=edit.create_material_expression(mat,unreal.MaterialExpressionMaterialFunctionCall,-420,-180)
    if not call or not call.set_material_function(source):
        raise RuntimeError("QA审核材质函数调用初始化失败："+target)
    for ix,p in enumerate(item["pins"]):
        cls={"S":"MaterialExpressionConstant","V2":"MaterialExpressionConstant2Vector",
             "V3":"MaterialExpressionConstant3Vector"}[p["type"]]
        expression=edit.create_material_expression(mat,getattr(unreal,cls),-1260,ix*185-620)
        if not expression:
            raise RuntimeError("QA参数节点创建失败："+item["id"])
        preview=p["preview"]
        if p["type"]=="S":
            expression.set_editor_property("r",float(preview))
        elif p["type"]=="V2":
            expression.set_editor_property("r",float(preview[0]))
            expression.set_editor_property("g",float(preview[1]))
        else:
            expression.set_editor_property("constant",unreal.LinearColor(*[float(n) for n in preview],1.0))
        if not edit.connect_material_expressions(expression,"",call,p["name"]):
            raise RuntimeError("QA参数无法连接到函数输入："+item["id"]+"."+p["name"])
    # 只连接真实输出至可用Material属性，不自动运行Shader编译。
    used=set()
    for out in item["outputs"]:
        label=out["name"]
        prop={"AO":"MP_AMBIENT_OCCLUSION","Roughness":"MP_ROUGHNESS",
              "Metallic":"MP_METALLIC","Normal":"MP_NORMAL",
              "Emissive":"MP_EMISSIVE_COLOR"}.get(label)
        if not prop:
            prop="MP_BASE_COLOR"
        if prop in used:
            # 避免多个不同颜色输出竞争同一个材质属性；附加输出由母版选择。
            continue
        if not edit.connect_material_property(call,label,getattr(unreal.MaterialProperty,prop)):
            raise RuntimeError("QA材质未连通真实Shader输出："+item["id"]+"."+label)
        used.add(prop)
    if not used or not library.save_loaded_asset(mat,only_if_is_dirty=False):
        raise RuntimeError("QA材质没有真实输出或未能保存："+target)
    print("QA_CREATED_NO_COMPILE",item["id"],target)


def main():
    """编译统一后置：本轮先完成所有源资产和独立审核图。"""
    parser=argparse.ArgumentParser(description="通用材质库50平台函数与10项目视觉函数UE原生制作")
    parser.add_argument("--mode",choices=("inspect","apply","review","verify","compile"),
                        default=os.getenv("MATERIAL_DOMAIN_MODE","inspect"))
    parser.add_argument("--owner",choices=("all","Surface","VFX","Project"),default="all")
    # Monolith单次run_python限定少量原生节点，避免长任务超时后重复写入。
    parser.add_argument("--start-index",type=int,default=0)
    parser.add_argument("--limit",type=int,default=0,help="0为全部；非零为本次最多处理数")
    ns,_=parser.parse_known_args()
    catalog=load_catalog()
    full=[x for x in catalog if ns.owner=="all" or x["owner"]==ns.owner]
    if ns.start_index<0 or ns.start_index>len(full) or ns.limit<0:
        raise ValueError("非法函数批次范围")
    target=full[ns.start_index:ns.start_index+ns.limit] if ns.limit else full[ns.start_index:]
    print("CATALOG_COUNT",len(catalog),"SELECTED",len(target),ns.mode,ns.owner,
          "OFFSET",ns.start_index,"LIMIT",ns.limit)
    for x in target:
        print("DOMAIN_FUNCTION_TARGET",x["id"],asset_path(x),x["zh"])
    if ns.mode=="inspect":
        print("INSPECT_ONLY：未创建任何UE资源")
        return
    try:
        import unreal # type: ignore[import-not-found]
    except ImportError as error:
        raise RuntimeError("创建和核验必须在UE5.8编辑器内运行") from error
    require_engine(unreal)
    lib=unreal.EditorAssetLibrary
    edit=unreal.MaterialEditingLibrary
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([ROOTS[x][0] for x in ("Surface","VFX","Project")]+[REVIEW_ROOT],force_rescan=True)
    # 全部真实资源先检查，防止中途覆盖已存在的旧资产。
    for item in target:
        existing=safe_target(unreal,item,ns.mode=="apply")
        if ns.mode in ("verify","review","compile") and existing is None:
            raise FileNotFoundError("目标MF还未原生创建："+asset_path(item))
    if ns.mode=="review":
        for item in target:
            make_review(unreal,item)
        print("DOMAIN_QA_ASSETS_AUTHORED",len(target))
        return
    if ns.mode=="compile":
        for item in target:
            qa=REVIEW_ROOT+"/M_DBA_MaterialLibrary_"+item["name"]+"_DomainReview"
            mat=lib.load_asset(qa)
            if not isinstance(mat,unreal.Material):
                raise FileNotFoundError("缺少QA材质，不可声明Shader完成："+qa)
            errors=list(edit.recompile_material(mat) or [])
            for message in errors:
                print("SHADER_DIAGNOSTIC",item["id"],str(message))
                if "error" in str(message).lower():
                    raise RuntimeError("QA Shader编译发现错误："+item["id"])
            if not lib.save_loaded_asset(mat,only_if_is_dirty=False):
                raise RuntimeError("编译后的QA材质未保存："+qa)
            print("DOMAIN_SHADER_OK",item["id"])
        print("DOMAIN_SHADER_BATCH_COMPLETE",len(target))
        return
    factory=unreal.MaterialFunctionFactoryNew()
    for item in target:
        target_path=asset_path(item)
        existing=safe_target(unreal,item,ns.mode=="apply")
        if existing:
            nodes=edit.get_num_material_expressions_in_function(existing)
            if nodes<len(item["pins"])+len(item["outputs"])+1:
                raise RuntimeError("已有材质函数图节点不足，禁止覆盖："+target_path)
            print("DOMAIN_FUNCTION_ALREADY_EXISTS",item["id"],nodes)
            continue
        if ns.mode=="verify":
            raise FileNotFoundError("缺少MaterialFunction："+target_path)
        folder=target_path.rsplit("/",1)[0]
        if not lib.does_directory_exist(folder):
            lib.make_directory(folder)
        created=unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            target_path.rsplit("/",1)[-1],folder,unreal.MaterialFunction,factory)
        if not isinstance(created,unreal.MaterialFunction):
            raise RuntimeError("UE5.8创建材质函数失败："+target_path)
        nodes=make_graph(unreal,created,item)
        created.set_editor_property("description",item["zh"]+"。中立视觉运算，不承担游戏服务器权威计算。")
        created.set_editor_property("expose_to_library",True)
        edit.update_material_function(created)
        if not lib.save_loaded_asset(created,only_if_is_dirty=False):
            raise RuntimeError("原生MaterialFunction保存失败："+target_path)
        reloaded=lib.load_asset(target_path)
        if not isinstance(reloaded,unreal.MaterialFunction) or edit.get_num_material_expressions_in_function(reloaded)<nodes:
            raise RuntimeError("UE已创建MF但真实资源回读未通过："+target_path)
        print("DOMAIN_FUNCTION_CREATED",item["id"],target_path,"nodes",nodes)
    print("DOMAIN_FUNCTION_AUTHOR_COMPLETE",len(target))


if __name__=="__main__":
    main()
