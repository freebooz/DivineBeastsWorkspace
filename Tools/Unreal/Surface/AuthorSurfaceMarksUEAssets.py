#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8五种局部痕迹资源：真实Texture2D、DeferredDecal母材质、5份实例和UDataAsset目录。
环境SURFACE_MARK_ASSETS_MODE=inspect(默认)/author-materials/verify-materials/author/verify；UE写入只允许专用完整编辑器。
不覆盖已有同身份UE资产；失败保留逐项原始状态。源PNG不算Texture2D。
"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = Path(__file__).with_name("SurfaceMarkSourceManifest.json")
MODE = os.environ.get("SURFACE_MARK_ASSETS_MODE", "inspect").strip().lower()
ROOT_MOUNT = "/GamePlatformSurface/Marks"
TEXTURES = ROOT_MOUNT + "/Textures"
MATERIALS = ROOT_MOUNT + "/Materials"
MASTER = MATERIALS + "/M_GP_SurfaceMark_Master"
CATALOG = ROOT_MOUNT + "/DA_GP_SurfaceMarkCatalog"
KINDS = ("Bullet", "Scorch", "Slash", "Fluid", "Crack")
WRITING = MODE in ("author", "author-materials")

# SourceArt与Contents独立：目标`.uasset`必须由UE存盘，且不覆写注册表尚未发现的已有物理包。
CONTENT_ROOT = ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Content"

def assert_native_package_not_occupied(package):
    suffix=package.removeprefix("/GamePlatformSurface/") + ".uasset"
    path=CONTENT_ROOT / suffix
    if path.exists() or path.with_suffix(".uexp").exists() or path.with_suffix(".ubulk").exists():
        raise RuntimeError("已有磁盘包或侧车数据，禁止覆盖／借缺失注册表创建：" + str(path))


def validate_sources():
    doc = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if doc.get("schemaVersion") != 1 or len(doc.get("records", [])) != 15:
        raise RuntimeError("表面痕迹源图合同必须为五类×三通道")
    expected={(k,r) for k in KINDS for r in ("Albedo","Normal","ORM")}
    actual=set()
    for item in doc["records"]:
        kind,role=item["kind"],item["role"]
        if (kind,role) in actual: raise RuntimeError("纹理源图存在重复类别/通道")
        actual.add((kind,role))
        source=(ROOT/item["path"]).resolve()
        if not source.is_relative_to(ROOT) or source.is_symlink() or not source.is_file():
            raise RuntimeError("源图路径越界或缺失："+str(source))
        if hashlib.sha256(source.read_bytes()).hexdigest()!=item["sha256"]:
            raise RuntimeError("源纹理SHA256发生漂移："+str(source))
        if (item["width"],item["height"])!=(1024,1024):
            raise RuntimeError("纹理尺寸合同错误")
    if actual!=expected: raise RuntimeError("纹理源图类别/通道不完整")
    return doc

def obj_path(package):
    return package + "." + package.rsplit("/",1)[-1]

def require_instance(assets,unreal,package,cls):
    value=assets.load_asset(package)
    if not isinstance(value,cls): raise TypeError("UE资产缺失或类别错误："+package)
    return value

def import_textures(doc,unreal,assets,tools):
    result={}
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/GamePlatformSurface"],force_rescan=True)
    if WRITING and not assets.does_directory_exist(TEXTURES): assets.make_directory(TEXTURES)
    for item in doc["records"]:
        kind,role=item["kind"],item["role"]
        package=f"{TEXTURES}/T_GP_Mark_{kind}_{role}"
        if not assets.does_asset_exist(package):
            if not WRITING: raise RuntimeError("尚未导入真实Texture2D："+package)
            assert_native_package_not_occupied(package)
            task=unreal.AssetImportTask()
            task.set_editor_property("filename",str(ROOT/item["path"]))
            task.set_editor_property("destination_path",TEXTURES)
            task.set_editor_property("destination_name",package.rsplit("/",1)[-1])
            task.set_editor_property("replace_existing",False)
            task.set_editor_property("automated",True)
            task.set_editor_property("save",False)
            tools.import_asset_tasks([task])
            if not task.get_editor_property("imported_object_paths"):
                raise RuntimeError("纹理导入失败："+package)
            tex=require_instance(assets,unreal,package,unreal.Texture2D)
            tex.set_editor_property("srgb",role=="Albedo")
            compression={"Albedo":unreal.TextureCompressionSettings.TC_DEFAULT,
                         "Normal":unreal.TextureCompressionSettings.TC_NORMALMAP,
                         "ORM":unreal.TextureCompressionSettings.TC_MASKS}[role]
            tex.set_editor_property("compression_settings",compression)
            tex.set_editor_property("mip_gen_settings",unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
            if not assets.save_loaded_asset(tex,only_if_is_dirty=False):
                raise RuntimeError("纹理保存失败："+package)
        tex=require_instance(assets,unreal,package,unreal.Texture2D)
        if tex.get_editor_property("srgb")!=(role=="Albedo"):
            raise RuntimeError("源图的sRGB/线性采样配置不符："+package)
        result[(kind,role)]=tex
        print("SURFACE_MARK_TEXTURE",kind,role,package,flush=True)
    return result

def build_material(unreal,edit,master,textures):
    # UE编辑器原生节点而非空白占位。所有贴花必须连接RGB、Alpha、Normal和Roughness。
    if edit.get_num_material_expressions(master)!=0:
        raise RuntimeError("同身份母材质非空，拒绝无条件覆盖已有图。")
    master.set_editor_property("material_domain",unreal.MaterialDomain.MD_DEFERRED_DECAL)
    master.set_editor_property("blend_mode",unreal.BlendMode.BLEND_TRANSLUCENT)
    albedo=edit.create_material_expression(master,unreal.MaterialExpressionTextureSampleParameter2D,-800,-400)
    normal=edit.create_material_expression(master,unreal.MaterialExpressionTextureSampleParameter2D,-800,-160)
    orm=edit.create_material_expression(master,unreal.MaterialExpressionTextureSampleParameter2D,-800,80)
    opacity=edit.create_material_expression(master,unreal.MaterialExpressionScalarParameter,-800,340)
    tint=edit.create_material_expression(master,unreal.MaterialExpressionVectorParameter,-800,540)
    mul_opacity=edit.create_material_expression(master,unreal.MaterialExpressionMultiply,-420,250)
    mul_color=edit.create_material_expression(master,unreal.MaterialExpressionMultiply,-420,-350)
    for expression,name,texture in (
        (albedo,"MarkAlbedo",textures[("Bullet","Albedo")]),
        (normal,"MarkNormal",textures[("Bullet","Normal")]),
        (orm,"MarkORM",textures[("Bullet","ORM")])):
        expression.set_editor_property("parameter_name",unreal.Name(name))
        expression.set_editor_property("texture",texture)
    opacity.set_editor_property("parameter_name",unreal.Name("OpacityGain"))
    opacity.set_editor_property("default_value",1.0)
    tint.set_editor_property("parameter_name",unreal.Name("Tint"))
    tint.set_editor_property("default_value",unreal.LinearColor(1,1,1,1))

    def wire(a,output,b,input):
        if not edit.connect_material_expressions(a,output,b,input):
            raise RuntimeError("贴花母材质节点未连接："+str(a.get_name())+" -> "+str(b.get_name())+" "+input)
    def material(a,output,prop):
        if not edit.connect_material_property(a,output,prop):
            raise RuntimeError("贴花母材质输出连接失败："+str(prop))
    wire(albedo,"A",mul_opacity,"A")
    wire(opacity,"",mul_opacity,"B")
    wire(albedo,"RGB",mul_color,"A")
    wire(tint,"",mul_color,"B")
    material(mul_color,"",unreal.MaterialProperty.MP_BASE_COLOR)
    material(mul_opacity,"",unreal.MaterialProperty.MP_OPACITY)
    material(normal,"RGB",unreal.MaterialProperty.MP_NORMAL)
    material(orm,"G",unreal.MaterialProperty.MP_ROUGHNESS)
    diagnostic=list(edit.recompile_material(master))
    if any("error" in str(d).lower() for d in diagnostic):
        raise RuntimeError("真实Shader编译失败："+str(diagnostic))
    if edit.get_num_material_expressions(master)<7:
        raise RuntimeError("贴花母材质节点数不足")
    print("SURFACE_MARK_MASTER_GRAPH",edit.get_num_material_expressions(master),flush=True)

def ensure_materials(unreal,assets,tools,edit,textures):
    parent_exists=assets.does_asset_exist(MASTER)
    if not parent_exists and not WRITING:
        raise RuntimeError("真实贴花母材质尚未生成")
    if parent_exists:
        master=require_instance(assets,unreal,MASTER,unreal.Material)
    else:
        assert_native_package_not_occupied(MASTER)
        if not assets.does_directory_exist(MATERIALS):assets.make_directory(MATERIALS)
        master=tools.create_asset(MASTER.rsplit("/",1)[-1],MATERIALS,unreal.Material,unreal.MaterialFactoryNew())
        if not isinstance(master,unreal.Material):raise RuntimeError("UE没有创建贴花母材质")
        build_material(unreal,edit,master,textures)
        if not assets.save_loaded_asset(master,only_if_is_dirty=False):
            raise RuntimeError("贴花母材质未能保存")
    if master.get_editor_property("material_domain")!=unreal.MaterialDomain.MD_DEFERRED_DECAL:
        raise RuntimeError("现有材质不是DeferredDecal，不可按贴花复用")
    if edit.get_num_material_expressions(master)<7:
        raise RuntimeError("现有贴花母材质不是完整PBR节点图")

    instances={}
    for kind in KINDS:
        package=f"{MATERIALS}/MI_GP_SurfaceMark_{kind}"
        exists=assets.does_asset_exist(package)
        if not exists and not WRITING:raise RuntimeError("缺失贴花实例："+package)
        if exists:
            mi=require_instance(assets,unreal,package,unreal.MaterialInstanceConstant)
        else:
            assert_native_package_not_occupied(package)
            mi=tools.create_asset(package.rsplit("/",1)[-1],MATERIALS,
                                  unreal.MaterialInstanceConstant,
                                  unreal.MaterialInstanceConstantFactoryNew())
            if not isinstance(mi,unreal.MaterialInstanceConstant):
                raise RuntimeError("UE创建材质实例失败："+package)
            edit.set_material_instance_parent(mi,master)
            for role,param in (("Albedo","MarkAlbedo"),("Normal","MarkNormal"),("ORM","MarkORM")):
                edit.set_material_instance_texture_parameter_value(mi,param,textures[(kind,role)])
            if not assets.save_loaded_asset(mi,only_if_is_dirty=False):
                raise RuntimeError("UE保存材质实例失败："+package)
        if mi.get_editor_property("parent")!=master:
            raise RuntimeError("材质实例未继承正确Decal母材质："+package)
        instances[kind]=mi
        print("SURFACE_MARK_DECAL_MI",kind,package,flush=True)
    return instances

def ensure_catalog(unreal,assets,tools,instances):
    exists=assets.does_asset_exist(CATALOG)
    if not exists and MODE!="author": raise RuntimeError("缺少真实SurfaceMarkCatalog")
    if exists:
        catalog=require_instance(assets,unreal,CATALOG,unreal.GamePlatformSurfaceMarkCatalog)
    else:
        assert_native_package_not_occupied(CATALOG)
        if not assets.does_directory_exist(ROOT_MOUNT):assets.make_directory(ROOT_MOUNT)
        factory=unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class",unreal.GamePlatformSurfaceMarkCatalog)
        catalog=tools.create_asset(CATALOG.rsplit("/",1)[-1],ROOT_MOUNT,
                                   unreal.GamePlatformSurfaceMarkCatalog,factory)
        if not isinstance(catalog,unreal.GamePlatformSurfaceMarkCatalog):
            raise RuntimeError("UE创建目录资产失败")
        styles=[]
        for kind in KINDS:
            entry=unreal.GamePlatformSurfaceMarkStyle()
            enum_value=getattr(unreal.GamePlatformSurfaceMarkKind,kind.upper())
            entry.set_editor_property("kind",enum_value)
            entry.set_editor_property("material",instances[kind])
            dims={"Bullet":(11,23,23),"Scorch":(18,180,180),"Slash":(16,125,38),
                  "Fluid":(13,110,110),"Crack":(18,180,180)}[kind]
            entry.set_editor_property("size_cm",unreal.Vector(*dims))
            entry.set_editor_property("lifetime_seconds",{"Bullet":60,"Scorch":100,"Slash":75,"Fluid":45,"Crack":100}[kind])
            entry.set_editor_property("fade_seconds",10.0)
            entry.set_editor_property("normal_offset_cm",.35)
            styles.append(entry)
        catalog.set_editor_property("styles",styles)
        catalog.set_editor_property("max_active_marks",128)
        catalog.set_editor_property("max_pending_marks",32)
        catalog.set_editor_property("max_recent_event_ids",256)
        if not assets.save_loaded_asset(catalog,only_if_is_dirty=False):
            raise RuntimeError("UE目录数据资产保存失败")
    styles=list(catalog.get_editor_property("styles"))
    if len(styles)<5:raise RuntimeError("真实目录样式不足五类")
    for kind in KINDS:
        expect=getattr(unreal.GamePlatformSurfaceMarkKind,kind.upper())
        matched=[x for x in styles if x.get_editor_property("kind")==expect]
        if len(matched)!=1: raise RuntimeError("真实目录缺失或重复类别："+kind)
    print("SURFACE_MARK_CATALOG_VERIFIED",CATALOG,"styles",len(styles),flush=True)

def main():
    if MODE not in ("inspect", "author-materials", "verify-materials", "author", "verify"):
        raise ValueError("只接受inspect、author-materials、verify-materials、author、verify")
    doc=validate_sources()
    print("SURFACE_MARK_SOURCE_VERIFIED",len(doc["records"]),MODE,flush=True)
    if MODE=="inspect":return
    import unreal
    project=unreal.Paths.get_project_file_path().replace("\\","/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("拒绝在非神兽联盟唯一主工程中创作")
    if WRITING:
        command_line=unreal.SystemLibrary.get_command_line().lower()
        if any(flag not in command_line for flag in ("-unattended", "-scripterrorsarefatal", "-executepythonscript=")) or "-run=" in command_line:
            raise RuntimeError("禁止在普通人工编辑器或Commandlet写入；需无人值守完整Editor脚本")
        if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():
            raise RuntimeError("写入宿主已有未保存修改，禁止创建资源")
    assets=unreal.EditorAssetLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    edit=unreal.MaterialEditingLibrary
    textures=import_textures(doc,unreal,assets,tools)
    instances=ensure_materials(unreal,assets,tools,edit,textures)
    if MODE.endswith("-materials"):
        print("SURFACE_MARK_UE_MATERIALS_PASSED",MODE,len(instances),flush=True)
        return
    ensure_catalog(unreal,assets,tools,instances)
    print("SURFACE_MARK_UE_ASSETS_PASSED",MODE,flush=True)

if __name__=="__main__":
    main()
