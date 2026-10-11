#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8十层Landscape PBR母材质原生节点创作，项目归属第三层Village内容包。

本工具不创建独立Surface插件和第二套雪权威；10个稳定LandscapeLayerSample属性取源WeightMap。
Basic WeightBlend计算BaseColor/Normal/Roughness，Dirt降级层避免全零输入黑斑；细节高度混合
留给通用Surface后续质量档/层函数，不能把Gaea海拔当微观Height。
保留Village原雪MPC动态参数名，默认无雪，便于未来换世界材质后复用DBAClient客户端天气桥。
默认只读inspect；TERRAIN_MATERIAL_AUTHOR_MODE=apply显式创建真实UMaterial和项目MI。
已有同名资源若不符不得覆盖；未使用UE Editor不能生成uasset，保存与回读必验证。
"""
from __future__ import annotations

import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = Path(__file__).with_name("TerrainSourceManifest_v1.json")
MODE = os.getenv("TERRAIN_MATERIAL_AUTHOR_MODE","inspect").strip().lower()
BASE = "/DBAWorldPack_Village/Terrain/Materials"
MASTER = BASE+"/M_DBA_Terrain_MultiLayer10"
INSTANCE = BASE+"/MI_DBA_Terrain_Calibration"
LAYERS = ("Grass","ForestSoil","Dirt","Rock","Scree","Sand","Gravel","RoadDirt",
          "FarmSoil","ShoreMud")

def create() -> None:
    manifest=json.loads(MANIFEST.read_text(encoding="utf-8"))
    if (manifest["schemaVersion"]!=1 or manifest["baseLayers"]!=list(LAYERS)):
        raise ValueError("多层地形资产身份不符合已审批的地表10层")
    if MODE not in ("inspect","apply","verify"):
        raise ValueError("TERRAIN_MATERIAL_AUTHOR_MODE只能inspect/apply/verify")
    print("TERRAIN_MATERIAL_AUTHOR_PLAN",MODE,MASTER,INSTANCE)
    if MODE=="inspect": return
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("必须用真实UE5.8 Editor创作和保存多层材质") from exc

    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("必须先加载项目唯一正式.uproject")
    library=unreal.EditorAssetLibrary
    tool=unreal.AssetToolsHelpers.get_asset_tools()
    edit=unreal.MaterialEditingLibrary
    tex_by_layer={}
    for layer in LAYERS:
        for role in ("BaseColor","Normal","ORM"):
            path=f"/DBAWorldPack_Village/Terrain/Textures/{layer}/T_DBA_Terrain_{layer}_{role}"
            texture=library.load_asset(path)
            if not isinstance(texture,unreal.Texture2D):
                raise TypeError("先导入49张真实UE纹理："+path)
            tex_by_layer[(layer,role)]=texture
    snow={}
    for name in ("SnowAlbedoDefault","SnowCoverageDefault","SnowNormalDefault"):
        path=f"/DBAWorldPack_Village/Materials/Landscape/Defaults/T_DBA_Village_{name}"
        obj=library.load_asset(path)
        if not isinstance(obj,unreal.Texture2D):
            raise TypeError("缺少现有Village服务器安全雪纹理："+path)
        snow[name]=obj

    def validate_asset():
        material=library.load_asset(MASTER)
        mi=library.load_asset(INSTANCE)
        if not isinstance(material,unreal.Material) or not isinstance(mi,unreal.MaterialInstanceConstant):
            raise RuntimeError("10层材质或项目实例未保存")
        count=edit.get_num_material_expressions(material)
        if count<100:
            raise RuntimeError("保存后母材质缺少十层权重/PBR节点："+str(count))
        if mi.get_editor_property("parent")!=material:
            raise RuntimeError("实例父级不是已生成10层Landscape母材质")
        print("TERRAIN_MULTILAYER_UE_VERIFY_PASS",MASTER,INSTANCE,"expressionCount",count)
    if MODE=="verify":
        validate_asset()
        return
    if library.does_asset_exist(MASTER) or library.does_asset_exist(INSTANCE):
        validate_asset()
        print("TERRAIN_MATERIAL_ALREADY_PRESENT_SKIP_OVERWRITE")
        return
    if not library.does_directory_exist(BASE):
        library.make_directory(BASE)
    m=tool.create_asset("M_DBA_Terrain_MultiLayer10",BASE,unreal.Material,
                        unreal.MaterialFactoryNew())
    if not isinstance(m,unreal.Material):
        raise RuntimeError("无法创建真实UMaterial资产")
    def node(cls,x,y):
        out=edit.create_material_expression(m,cls,int(x),int(y))
        if out is None: raise RuntimeError("无法创建材质表达式："+str(cls))
        return out
    def wire(a,b,pin,out=""):
        ok=edit.connect_material_expressions(a,out,b,pin)
        if not ok and pin in ("Input","Coordinates","VectorInput"):
            if pin=="Coordinates": ok=edit.connect_material_expressions(a,out,b,"UVs")
            if not ok: ok=edit.connect_material_expressions(a,out,b,"")
        if not ok: raise RuntimeError(f"无法连线{a.get_name()}→{b.get_name()}.{pin}")
    def const(v,x,y):
        expr=node(unreal.MaterialExpressionConstant,x,y)
        expr.set_editor_property("r",float(v))
        return expr
    def rgb(a,b,c,x,y):
        expr=node(unreal.MaterialExpressionConstant3Vector,x,y)
        expr.set_editor_property("constant",unreal.LinearColor(a,b,c,1))
        return expr
    def scalar(name,value,x,y):
        expr=node(unreal.MaterialExpressionScalarParameter,x,y)
        expr.set_editor_property("parameter_name",unreal.Name(name))
        expr.set_editor_property("default_value",float(value))
        return expr
    def expression(cls,a,b,x,y,oa="",ob=""):
        e=node(cls,x,y)
        wire(a,e,"A",oa);wire(b,e,"B",ob)
        return e
    def add(a,b,x,y,oa="",ob=""):
        return expression(unreal.MaterialExpressionAdd,a,b,x,y,oa,ob)
    def mul(a,b,x,y,oa="",ob=""):
        return expression(unreal.MaterialExpressionMultiply,a,b,x,y,oa,ob)
    def lerp(a,b,mask,x,y,oa="",ob=""):
        e=node(unreal.MaterialExpressionLinearInterpolate,x,y)
        wire(a,e,"A",oa);wire(b,e,"B",ob);wire(mask,e,"Alpha")
        return e
    def sample(tex,parameter,uv,x,y):
        e=node(unreal.MaterialExpressionTextureSampleParameter2D,x,y)
        e.set_editor_property("parameter_name",unreal.Name(parameter))
        e.set_editor_property("texture",tex)
        wire(uv,e,"Coordinates")
        return e
    def sat(a,x,y):
        e=node(unreal.MaterialExpressionSaturate,x,y);wire(a,e,"Input");return e
    # 本地纹理UV以Landscape UV×TerrainDetailTiling控制，未来Quality档用Scalar覆盖。
    uvs=node(unreal.MaterialExpressionTextureCoordinate,-2250,-850)
    tile=scalar("TerrainDetailTiling",12.0,-2270,-570)
    tex_uv=mul(uvs,tile,-2030,-690)
    weights=[]
    col=[]
    norms=[]
    roughness=[]
    for i,layer in enumerate(LAYERS):
        # LandscapeLayerSample返回引擎真实LayerInfo绘制权重，不依赖项目固定坐标输入。
        y=-1950+i*580
        w=node(unreal.MaterialExpressionLandscapeLayerSample,-1930,y)
        w.set_editor_property("parameter_name",unreal.Name("LS_"+layer))
        w.set_editor_property("preview_weight",1.0 if layer=="Dirt" else 0.0)
        weights.append(w)
        tx=sample(tex_by_layer[(layer,"BaseColor")],
                  "Terrain"+layer+"BaseColor",tex_uv,-1600,y-135)
        tn=sample(tex_by_layer[(layer,"Normal")],
                  "Terrain"+layer+"Normal",tex_uv,-1620,y+55)
        orm=sample(tex_by_layer[(layer,"ORM")],
                   "Terrain"+layer+"ORM",tex_uv,-1610,y+240)
        col.append(mul(tx,w,-1330,y-120,"RGB"))
        norms.append(mul(tn,w,-1310,y+35,"RGB"))
        roughness.append(mul(orm,w,-1310,y+220,"G"))
    def reduce_sum(items,x,y,step):
        out=items[0]
        for i,item in enumerate(items[1:],1):
            out=add(out,item,x+i*150,y+i*step)
        return out
    sum_w=reduce_sum(weights,-1150,-2550,120)
    sum_c=reduce_sum(col,-1150,-350,120)
    sum_n=reduce_sum(norms,-1100,980,120)
    sum_r=reduce_sum(roughness,-1150,2310,120)
    one=const(1,500,-2410)
    missing=sat(expression(unreal.MaterialExpressionSubtract,one,sum_w,650,-2270),840,-2250)
    dirt_base=sample(tex_by_layer[("Dirt","BaseColor")],
                     "TerrainFallbackDirtColor",tex_uv,430,-1570)
    fill=mul(dirt_base,missing,670,-1580,"RGB")
    result_color=add(sum_c,fill,1040,-50)
    flat=rgb(0,0,1,450,950)
    fill_n=mul(flat,missing,680,1060)
    result_normal=add(sum_n,fill_n,1040,1050)
    dry=const(.82,420,2440)
    fill_rough=mul(dry,missing,670,2430)
    result_rough=add(sum_r,fill_rough,1030,2380)

    # 与当前正式Village天气MID字段保持同名，后续若按审批迁移母材质可直接复用WeatherClient。
    wet=scalar("VillageWetness",0.0,980,2880)
    wet_value=mul(wet,const(.21,1180,3020),1320,2850)
    dark=expression(unreal.MaterialExpressionSubtract,
                    const(1,1200,3170),wet_value,1510,2850)
    wet_color=mul(result_color,dark,1610,-50)
    wet_rough=lerp(result_rough,const(.37,1280,2550),wet,1620,2210)
    snow_amt=scalar("VillageSnowAmount",0.0,1000,-3050)
    macro_uv=mul(uvs,scalar("TerrainSnowMacroTiling",2.2,1070,-3380),1380,-3360)
    cov=sample(snow["SnowCoverageDefault"],"VillageSnowCoverageTexture",
               macro_uv,1600,-3290)
    # 雪量、宏观噪声和坡度同时决定覆盖；避免将雪铺满垂直岩壁。
    vertex_normal=node(unreal.MaterialExpressionVertexNormalWS,1280,-2710)
    up=rgb(0,0,1,1300,-2570)
    dot=node(unreal.MaterialExpressionDotProduct,1480,-2610)
    wire(vertex_normal,dot,"A");wire(up,dot,"B")
    angle=sat(dot,1650,-2650)
    coverage_base=mul(snow_amt,cov,1840,-3140,ob="R")
    snow_coverage=sat(mul(coverage_base,angle,2080,-2920),2280,-2790)
    snow_color=sample(snow["SnowAlbedoDefault"],"VillageSnowAlbedoTexture",
                      tex_uv,1620,-1740)
    final_color=lerp(wet_color,snow_color,snow_coverage,2670,-240,ob="RGB")
    snow_normal=sample(snow["SnowNormalDefault"],"VillageSnowNormalTexture",
                      tex_uv,1680,750)
    final_normal=lerp(result_normal,snow_normal,snow_coverage,2670,1060,ob="RGB")
    final_rough=lerp(wet_rough,const(.85,2110,2280),
                     snow_coverage,2650,2230)
    if not edit.connect_material_property(final_color,"",unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError("Landscape BaseColor 未连接")
    if not edit.connect_material_property(final_normal,"",unreal.MaterialProperty.MP_NORMAL):
        raise RuntimeError("Landscape Normal 未连接")
    if not edit.connect_material_property(final_rough,"",unreal.MaterialProperty.MP_ROUGHNESS):
        raise RuntimeError("Landscape Roughness 未连接")
    expression_count=edit.get_num_material_expressions(m)
    if expression_count<100:
        raise RuntimeError("10层Shader节点数不足："+str(expression_count))
    diagnostics=[str(message) for message in edit.recompile_material(m)]
    for message in diagnostics:
        print("TERRAIN_MATERIAL_SHADER_DIAGNOSTIC",message)
    if any("error" in msg.lower() for msg in diagnostics):
        raise RuntimeError("UE Terrain母材质Shader编译失败")
    if not library.save_loaded_asset(m,only_if_is_dirty=False):
        raise RuntimeError("保存十层UMaterial失败")
    print("TERRAIN_MULTILAYER_UE_MATERIAL_SAVED",MASTER,
          "expressionCount",expression_count)
    mi=tool.create_asset("MI_DBA_Terrain_Calibration",BASE,
                         unreal.MaterialInstanceConstant,
                         unreal.MaterialInstanceConstantFactoryNew())
    if not isinstance(mi,unreal.MaterialInstanceConstant):
        raise RuntimeError("未创建真实MaterialInstanceConstant")
    edit.set_material_instance_parent(mi,m)
    edit.set_material_instance_scalar_parameter_value(mi,"TerrainDetailTiling",9.0)
    edit.set_material_instance_scalar_parameter_value(mi,"VillageSnowAmount",0.0)
    if not library.save_loaded_asset(mi,only_if_is_dirty=False):
        raise RuntimeError("保存多层材质实例失败")
    validate_asset()

if __name__=="__main__":
    create()
