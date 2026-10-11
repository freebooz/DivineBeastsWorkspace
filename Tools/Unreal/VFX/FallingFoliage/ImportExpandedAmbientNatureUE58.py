# -*- coding: utf-8 -*-
"""UE5.8原生自然环境纹理/材质/特效定义制作。
平台层：复用既有FallingFoliage母材质，并增加两张中立Soft/Glow母材质。
项目层：只导入新增33种风格贴图和MI，不覆盖已交付的4个落叶版本。
Niagara：花瓣/叶片使用真正复制的模板实例，修改内部Sprite渲染材质、重新保存。
非落叶漂浮体只交付Textures+Material；不同运动未验证前禁止伪称可播放。
"""
from __future__ import annotations
import os
import json
import hashlib
import unreal
from pathlib import Path

ROOT=Path(unreal.Paths.project_dir()).resolve().parent
SOURCE=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/AmbientNature"
CATALOG=SOURCE/"AmbientNatureStyleCatalog.json"
MANIFEST=SOURCE/"GeneratedTextureManifest.json"
REPORT=ROOT/"Saved/Validation/FallingFoliage/ExpandedLibrary/UEImportReport.json"
BASE="/DBAPresentationPack_Core/VFX/Environment/AmbientNature"
PLATFORM="/GamePlatformVFX/Environment/AmbientNature/Materials"
FOLIAGE_MASTER="/GamePlatformVFX/Environment/FallingFoliage/Materials/M_GP_FallingFoliage_Master"
TEMPLATES={
    "petal":"/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/Niagara/NS_DBA_FallingFoliage_Peach",
    "leaf":"/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/Niagara/NS_DBA_FallingFoliage_Maple",
    "debris":"/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/Niagara/NS_DBA_FallingFoliage_Peach",
    # 非叶片的飘浮体复用平台中立漂浮母系统，第一版继承雪花的缓慢漂移运动。
    # 这只是可运行模板，萤火虫趋光/火星上升与风速专项调优仍需后续视觉验收。
    "floater":"/GamePlatformVFX/Environment/AmbientNature/Niagara/NS_GP_AmbientFloat_Base"
}
ASSETS=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
LIB=unreal.MaterialEditingLibrary
result={"version":1,"project":str(unreal.Paths.get_project_file_path()),
        "kindFilter":os.environ.get("AMBIENT_ASSET_KIND","ALL"),
        "styles":[],"platformMaterials":[],"failures":[]}


def check_project():
    if not result["project"].replace("\\","/").endswith("/DivineBeastsArena.uproject"):
        raise RuntimeError("编辑器身份错误，不允许修改非神兽联盟工程")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("本次必须用UE5.8")
    if not CATALOG.exists() or not MANIFEST.exists():
        raise FileNotFoundError("缺少风格目录或源纹理哈希清单")


def save_asset(obj,label):
    if obj is None or not ASSETS.save_loaded_asset(obj,only_if_is_dirty=False):
        raise RuntimeError("UE资产保存失败："+label)
    if ASSETS.load_asset(label) is None:
        raise RuntimeError("UE资产重新加载失败："+label)


def load_asset(path,klass):
    obj=ASSETS.load_asset(path)
    if not isinstance(obj,klass):
        raise RuntimeError("资产类型错误或缺失："+path+" expected="+klass.__name__)
    return obj


def make_soft_master(name,additive=False):
    path=PLATFORM+"/"+name
    if ASSETS.does_asset_exist(path):
        m=load_asset(path,unreal.Material)
        result["platformMaterials"].append({"path":path,"reused":True})
        return m
    engine_default=load_asset("/Engine/EngineResources/DefaultTexture",unreal.Texture)
    m=TOOLS.create_asset(name,PLATFORM,unreal.Material,unreal.MaterialFactoryNew())
    if not isinstance(m,unreal.Material):
        raise RuntimeError("无法创建真实平台环境母材质："+path)
    m.set_editor_property("blend_mode",unreal.BlendMode.BLEND_ADDITIVE if additive else unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model",unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided",True)
    m.set_editor_property("used_with_niagara_sprites",True)
    tex=LIB.create_material_expression(m,unreal.MaterialExpressionTextureSampleParameter2D,-760,-250)
    tex.set_editor_property("parameter_name",unreal.Name("LeafAlbedo"))
    tex.set_editor_property("texture",engine_default)
    particle=LIB.create_material_expression(m,unreal.MaterialExpressionParticleColor,-760,150)
    rgba=LIB.create_material_expression(m,unreal.MaterialExpressionMultiply,-475,-120)
    strength=LIB.create_material_expression(m,unreal.MaterialExpressionScalarParameter,-475,220)
    strength.set_editor_property("parameter_name",unreal.Name("GlowIntensity"))
    strength.set_editor_property("default_value",2.0 if additive else 0.9)
    emissive=LIB.create_material_expression(m,unreal.MaterialExpressionMultiply,-240,-125)
    opacity=LIB.create_material_expression(m,unreal.MaterialExpressionMultiply,-240,160)
    def connect(node,output,dest,slot):
        if not LIB.connect_material_expressions(node,output,dest,slot):
            raise RuntimeError("平台母材质连接失败 "+name+" "+slot)
    connect(tex,"RGB",rgba,"A")
    connect(particle,"RGB",rgba,"B")
    connect(rgba,"",emissive,"A")
    connect(strength,"",emissive,"B")
    connect(tex,"A",opacity,"A")
    connect(particle,"A",opacity,"B")
    if not LIB.connect_material_property(emissive,"",unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("环境材质Emissive输出失败")
    if not LIB.connect_material_property(opacity,"",unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError("环境材质Opacity输出失败")
    errors=list(LIB.recompile_material(m))
    if any("error" in str(x).lower() for x in errors):
        raise RuntimeError("环境母材质编译失败："+str(errors))
    save_asset(m,path)
    result["platformMaterials"].append({"path":path,"reused":False})
    print("AMBIENT_UE_MASTER_SAVED",path,flush=True)
    return m


def ensure_masters():
    if not ASSETS.does_directory_exist(PLATFORM):
        ASSETS.make_directory(PLATFORM)
    foliage=load_asset(FOLIAGE_MASTER,unreal.Material)
    soft=make_soft_master("M_GP_AmbientNature_Soft")
    glow=make_soft_master("M_GP_AmbientNature_Glow",additive=True)
    return foliage,soft,glow


def ensure_platform_float_template(soft_master,effect):
    """从已经运行验证过的粒子基线派生平台中立漂浮粒子模板，清除项目材质依赖。"""
    target="/GamePlatformVFX/Environment/AmbientNature/Niagara/NS_GP_AmbientFloat_Base"
    if ASSETS.does_asset_exist(target):
        return load_asset(target,unreal.NiagaraSystem)
    source="/GamePlatformVFX/Weather/Niagara/NS_GP_Weather_Snow"
    if not isinstance(ASSETS.load_asset(source),unreal.NiagaraSystem):
        raise RuntimeError("平台雪花Niagara未找到，拒绝以空资产冒充漂浮模板")
    if not ASSETS.does_directory_exist(target.rsplit("/",1)[0]):
        ASSETS.make_directory(target.rsplit("/",1)[0])
    ns=ASSETS.duplicate_asset(source,target)
    if not isinstance(ns,unreal.NiagaraSystem):
        raise RuntimeError("未能创建真实平台漂浮Niagara模板")
    ns.set_editor_property("effect_type",effect)
    prefix=ns.get_path_name()+":"
    count=0
    for sprite in unreal.ObjectIterator(unreal.NiagaraSpriteRendererProperties):
        if sprite.get_path_name().startswith(prefix):
            sprite.set_editor_property("material",soft_master)
            count+=1
    if count<1:
        raise RuntimeError("平台漂浮模板缺少Sprite渲染器")
    save_asset(ns,target)
    result["platformFloatTemplate"]={"path":target,"renderers":count,
                                     "mode":"provisional_weather_drift_reuse"}
    print("AMBIENT_UE_FLOAT_BASE_SAVED",target,count,flush=True)
    return ns


def verify_sources(style_id,source):
    for typ in ("Albedo","Normal"):
        name="T_DBA_Ambient_"+style_id+"_"+typ+"_Source.png"
        entry=next((x for x in source if x["name"]==name),None)
        if entry is None:
            raise RuntimeError("没有源资产元数据："+name)
        path=SOURCE/"Textures"/name
        if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest()!=entry["sha256"]:
            raise RuntimeError("源资产SHA-256不符："+name)


def import_texture(style,typ):
    name="T_DBA_Ambient_"+style+"_"+typ
    path=BASE+"/Textures/"+name
    if ASSETS.does_asset_exist(path):
        return load_asset(path,unreal.Texture2D),False
    src=SOURCE/"Textures"/(name+"_Source.png")
    task=unreal.AssetImportTask()
    for key,value in (("filename",str(src)),("destination_path",BASE+"/Textures"),
                      ("destination_name",name),("automated",True),
                      ("replace_existing",False),("save",False)):
        task.set_editor_property(key,value)
    TOOLS.import_asset_tasks([task])
    obj=load_asset(path,unreal.Texture2D)
    obj.set_editor_property("srgb",typ=="Albedo")
    obj.set_editor_property("compression_settings",
        unreal.TextureCompressionSettings.TC_DEFAULT if typ=="Albedo"
        else unreal.TextureCompressionSettings.TC_NORMALMAP)
    save_asset(obj,path)
    print("AMBIENT_UE_TEXTURE_IMPORTED",name,flush=True)
    return obj,True


def make_material(style,albedo,normal,parents):
    name=style["styleId"]
    path=BASE+"/Materials/MI_DBA_Ambient_"+name
    if ASSETS.does_asset_exist(path):
        return load_asset(path,unreal.MaterialInstanceConstant),False
    family=style["category"]
    if family in ("petal","leaf","debris"):
        parent=parents[0]
    elif name in ("Firefly","Ember"):
        parent=parents[2]
    else:
        parent=parents[1]
    mi=TOOLS.create_asset("MI_DBA_Ambient_"+name,BASE+"/Materials",
        unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    if not isinstance(mi,unreal.MaterialInstanceConstant):
        raise RuntimeError("材质实例创建失败："+name)
    LIB.set_material_instance_parent(mi,parent)
    LIB.set_material_instance_texture_parameter_value(mi,unreal.Name("LeafAlbedo"),albedo)
    if family in ("petal","leaf","debris"):
        LIB.set_material_instance_texture_parameter_value(mi,unreal.Name("LeafNormal"),normal)
        LIB.set_material_instance_scalar_parameter_value(mi,unreal.Name("LeafRoughness"),
            .83 if family=="petal" else .76)
        LIB.set_material_instance_scalar_parameter_value(mi,unreal.Name("LeafSpecular"),.16)
        LIB.set_material_instance_scalar_parameter_value(mi,unreal.Name("LeafTransmission"),
            .48 if family=="petal" else .30)
    else:
        LIB.set_material_instance_scalar_parameter_value(mi,unreal.Name("GlowIntensity"),
            1.85 if name in ("Firefly","Ember") else .87)
    save_asset(mi,path)
    print("AMBIENT_UE_MATERIAL_SAVED",name,flush=True)
    return mi,True


def niagara_variant(style,material):
    name=style["styleId"]
    path=BASE+"/Niagara/NS_DBA_Ambient_"+name
    if ASSETS.does_asset_exist(path):
        return load_asset(path,unreal.NiagaraSystem),False
    source=TEMPLATES.get(style["category"])
    if source is None:
        return None,False
    ns=ASSETS.duplicate_asset(source,path)
    if not isinstance(ns,unreal.NiagaraSystem):
        raise RuntimeError("Niagara复制失败："+name)
    prefix=ns.get_path_name()+":"
    change_count=0
    for sprite in unreal.ObjectIterator(unreal.NiagaraSpriteRendererProperties):
        if sprite.get_path_name().startswith(prefix):
            sprite.set_editor_property("material",material)
            change_count+=1
    if change_count<1:
        raise RuntimeError("复制系统未发现内部SpriteRenderer，拒绝发布错误外观："+name)
    save_asset(ns,path)
    # 同名源模板仍应保持原项目材质引用，复制实例只修改新包内嵌Renderer。
    for sprite in unreal.ObjectIterator(unreal.NiagaraSpriteRendererProperties):
        if sprite.get_path_name().startswith(prefix) and sprite.get_editor_property("material") != material:
            raise RuntimeError("Niagara Sprite材质变体绑定失败："+name)
    print("AMBIENT_UE_NIAGARA_VARIANT",name,"renderers",change_count,flush=True)
    return ns,True


def logical_id(value):
    root,version=value.rsplit("@",1)
    namespace,name=root.rsplit(".",1)
    obj=unreal.GamePlatformId()
    obj.set_editor_property("namespace",namespace)
    obj.set_editor_property("name",name)
    obj.set_editor_property("logical_version",int(version))
    return obj


def make_definition(style,ns,effect):
    name=style["styleId"]
    path=BASE+"/Definitions/DA_DBA_VFX_Ambient_"+name
    if ASSETS.does_asset_exist(path):
        return load_asset(path,unreal.GamePlatformVFXWorldDefinition),False
    if ns is None:
        return None,False
    factory=unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class",unreal.GamePlatformVFXWorldDefinition)
    asset=TOOLS.create_asset("DA_DBA_VFX_Ambient_"+name,BASE+"/Definitions",
        unreal.GamePlatformVFXWorldDefinition,factory)
    if not isinstance(asset,unreal.GamePlatformVFXWorldDefinition):
        raise RuntimeError("真实特效定义创建失败："+name)
    asset.set_editor_property("logical_id",logical_id(style["definitionId"]))
    asset.set_editor_property("data_version",unreal.GamePlatformDataVersion())
    asset.set_editor_property("niagara_system",ns)
    asset.set_editor_property("effect_type",effect)
    asset.set_editor_property("content_category",unreal.GamePlatformVFXContentCategory.ENVIRONMENT)
    asset.set_editor_property("auto_destroy",False)
    asset.set_editor_property("allow_pooling",True)
    asset.set_editor_property("enable_scalability",True)
    asset.set_editor_property("require_fixed_bounds",True)
    schema=unreal.GamePlatformVFXParameterSchema()
    if style["category"] in ("petal","leaf","debris"):
        text='(MaxOverrideCount=8,Rules=((Name="User.FoliageGroundHeight",Type=Float,bRequired=True,MinValue=-10000000.0,MaxValue=10000000.0)))'
        if not schema.import_text(text):
            raise RuntimeError("平台特效参数Schema创建失败")
    # 漂浮微粒不需要地面高度；禁止误用平台World落叶碰撞合同。
    asset.set_editor_property("parameter_schema",schema)
    save_asset(asset,path)
    print("AMBIENT_UE_DEFINITION_SAVED",name,style["definitionId"],flush=True)
    return asset,True


def main():
    check_project()
    catalog=json.loads(CATALOG.read_text(encoding="utf-8"))
    source=json.loads(MANIFEST.read_text(encoding="utf-8"))["textures"]
    chosen=result["kindFilter"].strip()
    styles=[s for s in catalog["styles"] if s["assetStatus"]=="planned" and
            (chosen=="ALL" or s["styleId"]==chosen)]
    if not styles:
        raise RuntimeError("未找到待导入的新风格："+chosen)
    for sub in ("Textures","Materials","Niagara","Definitions"):
        folder=BASE+"/"+sub
        if not ASSETS.does_directory_exist(folder):ASSETS.make_directory(folder)
    parents=ensure_masters()
    effect=load_asset("/GamePlatformVFX/Environment/FallingFoliage/EffectTypes/NET_GP_FallingFoliage_Ambient",
                      unreal.NiagaraEffectType)
    if any(style["category"]=="floater" for style in styles):
        ensure_platform_float_template(parents[1],effect)
    try:
        for style in styles:
            key=style["styleId"]
            verify_sources(key,source)
            albedo,a_import=import_texture(key,"Albedo")
            normal,n_import=import_texture(key,"Normal")
            material,m_created=make_material(style,albedo,normal,parents)
            ns,ns_created=niagara_variant(style,material)
            definition,d_created=make_definition(style,ns,effect)
            state={
              "styleId":key,"category":style["category"],"texturesReady":True,
              "materialReady":True,"niagaraVariantReady":ns is not None,
              "definitionReady":definition is not None,
              "motionRuntimeVerified":False,
              "motionImplementationState":"provisional_float_drift" if style["category"]=="floater"
                else "reused_falling_motion",
              "createdCounts":{
                "textures":int(a_import)+int(n_import),
                "material":int(m_created),"niagara":int(ns_created),
                "definition":int(d_created)}}
            result["styles"].append(state)
        result["success"]=True
    except Exception as exc:
        result["failures"].append(str(exc))
        result["success"]=False
        raise
    finally:
        REPORT.parent.mkdir(parents=True,exist_ok=True)
        REPORT.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
        print("AMBIENT_UE_IMPORT_REPORT",REPORT,"styles",len(result["styles"]),
              "success",result.get("success"),flush=True)

if __name__=="__main__":
    main()
