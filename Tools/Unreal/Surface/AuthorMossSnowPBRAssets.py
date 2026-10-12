#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8平台通用苔藓/积雪PBR资产创作。

只允许在真实编辑器中导入Texture2D、生成材质和项目实例。
不修改既有雪母材质、正式Village共享地图或服务器天气权威。
MOSS_SURFACE_MODE默认inspect，apply显式操作，verify只读资产合同。
"""
import hashlib
import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
MANIFEST=Path(__file__).with_name("MossPBRSourceManifest.json")
MODE=os.getenv("MOSS_SURFACE_MODE","inspect").strip().lower()
PLATFORM="/GamePlatformSurface"
MOSS=PLATFORM+"/Textures/Moss"
MASTER=PLATFORM+"/Materials/M_GP_Surface_SnowMoss_PBR"
INSTANCE="/DBAPresentationPack_Core/Materials/Environment/MI_DBA_MossSnow_Cliff"
MPC=PLATFORM+"/ParameterCollections/MPC_GP_SurfaceGlobal"

def run():
    if MODE not in ("inspect","verify","apply"):
        raise ValueError("MOSS_SURFACE_MODE只允许inspect/verify/apply")
    manifest=json.loads(MANIFEST.read_text(encoding="utf8"))
    if manifest["schemaVersion"]!=1 or len(manifest["maps"])!=5:
        raise ValueError("苔藓源图合同非法")
    for rec in manifest["maps"]:
        src=(ROOT/rec["path"]).resolve()
        if not src.is_relative_to(ROOT) or hashlib.sha256(src.read_bytes()).hexdigest()!=rec["sha256"]:
            raise ValueError("苔藓源图不可信或已变更："+str(src))

    targets={r["role"]:MOSS+"/"+Path(r["path"]).stem.removesuffix("_Source")
             for r in manifest["maps"]}
    for name in (MASTER,INSTANCE,*targets.values()):
        print("MOSS_PBR_ASSET_TARGET",name)
    if MODE=="inspect": return
    import unreal  # 只在真正的虚幻编辑器中导入资产
    project=unreal.Paths.get_project_file_path().replace("\\","/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("当前不是唯一正式神兽联盟工程："+project)
    assets=unreal.EditorAssetLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    edit=unreal.MaterialEditingLibrary
    # PythonScriptCommandlet启动时可能尚未枚举全部插件Content的uasset；
    # 强制同步扫描已挂载的插件目录，再加载已有雪和MPC，禁止伪造对象。
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([PLATFORM,"/DBAPresentationPack_Core"],force_rescan=True)

    if MODE=="apply":
        for record in manifest["maps"]:
            target=targets[record["role"]]
            if assets.does_asset_exist(target): continue
            if not assets.does_directory_exist(MOSS): assets.make_directory(MOSS)
            task=unreal.AssetImportTask()
            task.set_editor_property("filename",str(ROOT/record["path"]))
            task.set_editor_property("destination_path",MOSS)
            task.set_editor_property("destination_name",target.rsplit("/",1)[-1])
            task.set_editor_property("automated",True)
            task.set_editor_property("replace_existing",False)
            task.set_editor_property("save",False)
            tools.import_asset_tasks([task])
            if not task.get_editor_property("imported_object_paths"):
                raise RuntimeError("UE Texture2D导入失败："+target)

            tex=assets.load_asset(target)
            if not isinstance(tex,unreal.Texture2D): raise TypeError("纹理类别错误："+target)
            tex.set_editor_property("srgb",record["role"]=="albedo")
            modes={"normal":unreal.TextureCompressionSettings.TC_NORMALMAP,
                   "orm":unreal.TextureCompressionSettings.TC_MASKS,
                   "coverage":unreal.TextureCompressionSettings.TC_MASKS,
                   "height":unreal.TextureCompressionSettings.TC_MASKS}
            tex.set_editor_property("compression_settings",modes.get(record["role"],unreal.TextureCompressionSettings.TC_DEFAULT))
            if not assets.save_loaded_asset(tex,only_if_is_dirty=False):
                raise RuntimeError("纹理保存失败："+target)

    moss={role:assets.load_asset(target) for role,target in targets.items()}
    if not all(isinstance(t,unreal.Texture2D) for t in moss.values()):
        raise TypeError("缺少真实苔藓Texture2D")
    snow_paths={"albedo":PLATFORM+"/Textures/Weather/T_GP_Weather_SnowAlbedo",
                "normal":PLATFORM+"/Textures/Weather/T_GP_Weather_SnowNormal",
                "orm":PLATFORM+"/Textures/Weather/T_GP_Weather_SnowORM",
                "coverage":PLATFORM+"/Textures/Snow/T_GP_Snow_CoverageNoise"}
    snow={k:assets.load_asset(v) for k,v in snow_paths.items()}
    mpc=assets.load_asset(MPC)
    if not all(isinstance(t,unreal.Texture2D) for t in snow.values()) or not isinstance(mpc,unreal.MaterialParameterCollection):
        raise TypeError("缺少平台已交付的雪贴图或Surface MPC")
    if MODE=="verify": return validate(assets, edit, unreal)

    if not assets.does_asset_exist(MASTER):
        folder,name=MASTER.rsplit("/",1)
        if not assets.does_directory_exist(folder): assets.make_directory(folder)
        mat=tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
        if not isinstance(mat,unreal.Material): raise RuntimeError("无法创建真实苔藓积雪母材质")
    else:
        mat=assets.load_asset(MASTER)
        if not isinstance(mat,unreal.Material): raise TypeError("材质同名身份不正确")
    if edit.get_num_material_expressions(mat)==0:
        build_graph(unreal, edit, mat, mpc, moss, snow)

        diagnostics=list(edit.recompile_material(mat))
        if any("error" in str(e).lower() for e in diagnostics):
            raise RuntimeError("原生Shader编译失败："+str(diagnostics))
        if not assets.save_loaded_asset(mat,only_if_is_dirty=False):
            raise RuntimeError("UE母材质未能保存")
    if not assets.does_asset_exist(INSTANCE):
        folder,name=INSTANCE.rsplit("/",1)
        if not assets.does_directory_exist(folder): assets.make_directory(folder)
        mi=tools.create_asset(name,folder,unreal.MaterialInstanceConstant,
                              unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(mi,unreal.MaterialInstanceConstant):
            raise RuntimeError("UE未能创建项目苔藓实例")

        edit.set_material_instance_parent(mi,mat)
        edit.set_material_instance_scalar_parameter_value(mi,"MossLocalCoverage",0.8)
        edit.set_material_instance_scalar_parameter_value(mi,"SnowSlopePower",2.6)
        edit.set_material_instance_scalar_parameter_value(mi,"SnowEdgeSoftness",0.22)
        if not assets.save_loaded_asset(mi,only_if_is_dirty=False):
            raise RuntimeError("UE项目材质实例保存失败")
    validate(assets,edit,unreal)

def validate(assets, edit, unreal):
    """验证真实Texture2D、平台母材质与项目MI的父子契约。"""
    mat=assets.load_asset(MASTER)
    mi=assets.load_asset(INSTANCE)
    if not isinstance(mat,unreal.Material) or not isinstance(mi,unreal.MaterialInstanceConstant):
        raise RuntimeError("真实UE苔藓雪母材质或项目材质实例缺失")
    count=edit.get_num_material_expressions(mat)
    if count<40 or mi.get_editor_property("parent")!=mat:
        raise RuntimeError("苔藓雪材质节点数量或项目实例父级不符："+str(count))
    print("MOSS_SNOW_UE_ASSETS_VERIFIED",MASTER,INSTANCE,"nodes",count)

def build_graph(unreal, lib, mat, mpc, moss, snow):
    """仅在全新空母材质上创作双覆盖层；旧资产不静默覆盖。"""
    sequence=[0]
    def node(cls):
        i=sequence[0]; sequence[0]+=1
        v=lib.create_material_expression(mat,cls,-2400+(i%8)*320,-2200+(i//8)*270)
        if v is None: raise RuntimeError("UE无法创建MaterialExpression："+str(cls))
        return v

    def wire(src,dst,pin,out=""):
        ok=lib.connect_material_expressions(src,out,dst,pin)
        if not ok and pin=="Coordinates":
            ok=lib.connect_material_expressions(src,out,dst,"UVs")
        if not ok and pin in ("Input","Coordinates"):
            ok=lib.connect_material_expressions(src,out,dst,"")
        if not ok: raise RuntimeError("材质节点连接失败："+src.get_name()+" -> "+dst.get_name()+"."+pin)

    def const(value):
        a=node(unreal.MaterialExpressionConstant)
        a.set_editor_property("r",float(value))
        return a
    def color(r,g,b):
        a=node(unreal.MaterialExpressionConstant3Vector)
        a.set_editor_property("constant",unreal.LinearColor(r,g,b,1))
        return a

    def scalar(name,value):
        a=node(unreal.MaterialExpressionScalarParameter)
        a.set_editor_property("parameter_name",unreal.Name(name))
        a.set_editor_property("default_value",float(value))
        return a
    def collection(name):
        a=node(unreal.MaterialExpressionCollectionParameter)
        a.set_editor_property("collection",mpc)
        a.set_editor_property("parameter_name",unreal.Name(name))
        return a

    def tex(name,value,uv):
        a=node(unreal.MaterialExpressionTextureSampleParameter2D)
        a.set_editor_property("parameter_name",unreal.Name(name))
        a.set_editor_property("texture",value)
        wire(uv,a,"Coordinates")
        return a

    def binary(cls,a,b,outa="",outb=""):
        z=node(cls)
        wire(a,z,"A",outa)
        wire(b,z,"B",outb)
        return z
    def mul(a,b,oa="",ob=""):
        return binary(unreal.MaterialExpressionMultiply,a,b,oa,ob)
    def sub(a,b,oa="",ob=""):
        return binary(unreal.MaterialExpressionSubtract,a,b,oa,ob)

    def blend(a,b,mask,oa="",ob=""):
        z=node(unreal.MaterialExpressionLinearInterpolate)
        wire(a,z,"A",oa); wire(b,z,"B",ob); wire(mask,z,"Alpha")
        return z
    def sat(a):
        z=node(unreal.MaterialExpressionSaturate)
        wire(a,z,"Input")
        return z

    def power(a,b):
        z=node(unreal.MaterialExpressionPower)
        wire(a,z,"Base"); wire(b,z,"Exp")
        return z
    def xy(a):
        z=node(unreal.MaterialExpressionComponentMask)
        z.set_editor_property("r",True); z.set_editor_property("g",True)
        z.set_editor_property("b",False); z.set_editor_property("a",False)
        wire(a,z,"Input")
        return z

    # 使用世界空间宏观覆盖避免不同模型UV尺度不一致，微细纹理单独平铺。
    world=node(unreal.MaterialExpressionWorldPosition)
    uv_world=xy(world)
    moss_uv=mul(uv_world,scalar("MossWorldScale",0.00055))
    snow_uv=mul(uv_world,scalar("SnowWorldScale",0.00045))
    detail_uv=mul(node(unreal.MaterialExpressionTextureCoordinate),
                  scalar("MossDetailTiling",7.5))
    moss_albedo=tex("MossAlbedoTexture",moss["albedo"],detail_uv)
    moss_normal=tex("MossNormalTexture",moss["normal"],detail_uv)

    moss_orm=tex("MossORMTexture",moss["orm"],detail_uv)
    moss_height=tex("MossHeightTexture",moss["height"],detail_uv)
    moss_macro=tex("MossCoverageTexture",moss["coverage"],moss_uv)
    snow_albedo=tex("SnowAlbedoTexture",snow["albedo"],detail_uv)
    snow_normal=tex("SnowNormalTexture",snow["normal"],detail_uv)
    snow_orm=tex("SnowORMTexture",snow["orm"],detail_uv)
    snow_macro=tex("SnowCoverageTexture",snow["coverage"],snow_uv)

    # 遮罩只依赖VertexNormalWS；输出法线不会反向改变遮罩造成Shader循环。
    vertex=node(unreal.MaterialExpressionVertexNormalWS)
    slope=sat(binary(unreal.MaterialExpressionDotProduct,vertex,color(0,0,1)))
    moss_angle=blend(const(0.40),const(1),slope)
    moss_clumps=sat(mul(moss_macro,moss_height,"R","R"))
    moss_amount=sat(mul(mul(moss_clumps,moss_angle),
                        mul(collection("GP_Surface_GlobalMossInfluence"),
                            scalar("MossLocalCoverage",0.82))))

    global_snow=collection("GP_Surface_GlobalSnowAmount")
    snow_slope=power(slope,scalar("SnowSlopePower",2.8))
    # 积雪量0严格无雪；微量雪产生稀疏的斑驳覆盖而不是突然全白。
    threshold=sub(const(.5),mul(global_snow,scalar("SnowCoverageShiftScale",.40)))
    snow_noise=sub(snow_macro,threshold,"R")
    snow_edge=binary(unreal.MaterialExpressionDivide,snow_noise,
                     scalar("SnowEdgeSoftness",.22))
    snow_amount=sat(mul(global_snow,mul(sat(snow_edge),snow_slope)))

    # 表面顺序：基础岩石→苔藓→湿润光学变暗→雪覆盖，防止雪下变绿。
    base=color(.31,.33,.30)
    moss_color=blend(base,moss_albedo,moss_amount,ob="RGB")
    wetness=collection("GP_Surface_GlobalWetness")
    wet_factor=sub(const(1),mul(wetness,scalar("WetColorDarken",.23)))
    wet_color=mul(moss_color,wet_factor)
    final_color=blend(wet_color,snow_albedo,snow_amount,ob="RGB")
    if not lib.connect_material_property(final_color,"",unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError("积雪苔藓BaseColor未连接")

    rock_rough=scalar("RockRoughness",.79)
    moss_rough=blend(rock_rough,moss_orm,moss_amount,ob="G")
    wet_rough=blend(moss_rough,scalar("WetRoughness",.43),wetness)
    snow_rough=blend(wet_rough,snow_orm,snow_amount,ob="G")
    if not lib.connect_material_property(snow_rough,"",unreal.MaterialProperty.MP_ROUGHNESS):
        raise RuntimeError("积雪苔藓Roughness未连接")

    # 法线随覆盖率混合，坡度使用几何法线避免依赖环。
    moss_n=blend(color(0,0,1),moss_normal,moss_amount,ob="RGB")
    snow_n=blend(moss_n,snow_normal,snow_amount,ob="RGB")
    if not lib.connect_material_property(snow_n,"",unreal.MaterialProperty.MP_NORMAL):
        raise RuntimeError("积雪苔藓Normal未连接")
    moss_ao=blend(const(1),moss_orm,moss_amount,ob="R")
    final_ao=blend(moss_ao,snow_orm,snow_amount,ob="R")
    if not lib.connect_material_property(final_ao,"",unreal.MaterialProperty.MP_AMBIENT_OCCLUSION):
        raise RuntimeError("积雪苔藓AO未连接")

    if not lib.connect_material_property(const(0),"",unreal.MaterialProperty.MP_METALLIC):
        raise RuntimeError("非金属环境表面Metallic未连接")
    print("MOSS_SNOW_UE_GRAPH_CREATED expressions",sequence[0])

if __name__=="__main__":
    run()
