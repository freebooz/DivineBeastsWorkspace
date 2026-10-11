#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 鸟群真实资源创建工具：导入FBX/PNG并建立平台材质与项目材质实例。

工程边界：只操作新的 BirdFlock 目录，创建前验证工程和源清单；不覆盖既有已编辑资源。
inspect=列出预计资产；apply=UE5.8 编辑器 PythonScriptPlugin 正式写入并保存。
不在本脚本里伪造 Niagara 发射器、蓝图或材质资产文件扩展名。
"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
SOURCE=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/BirdFlock"
MANIFEST=SOURCE/"BirdFlockSourceManifest.json"
MASTER_ROOT="/GamePlatformVFX/Environment/BirdFlock/Materials"
MESH_ROOT="/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Meshes"
TEX_ROOT="/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Textures"
MI_ROOT="/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Materials"
SPECIES=("Seagull","Swallow","Eagle")
MODE=os.getenv("BIRD_FLOCK_AUTHOR_MODE","inspect").lower()


def verified():
    if not MANIFEST.is_file():
        raise FileNotFoundError("鸟群源资源清单不存在："+str(MANIFEST))
    data=json.loads(MANIFEST.read_text(encoding="utf-8"))
    if data.get("schemaVersion")!=1 or len(data.get("species",[]))!=len(SPECIES):
        raise RuntimeError("源资源清单格式或鸟种数不正确")
    for entry in data["files"]:
        path=SOURCE/entry["path"]
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=entry["sha256"]:
            raise RuntimeError("源纹理/网格哈希不一致："+str(path))
    for row in data["species"]:
        if row["name"] not in SPECIES:
            raise RuntimeError("清单存在不支持的鸟类型："+row["name"])
    return data


def require_ue():
    import unreal # type: ignore
    project=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not project.endswith("/DivineBeastsArena.uproject"):
        raise RuntimeError("只允许 DivineBeastsArena 正式工程："+project)
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("必须使用锁定 UE5.8")
    return unreal


def ensure_dir(asset_lib,path):
    if not asset_lib.does_directory_exist(path):
        if not asset_lib.make_directory(path):
            raise RuntimeError("无法创建 UE 挂载目录："+path)


def import_texture(unreal,tools,assets,name):
    dest=f"{TEX_ROOT}/T_DBA_Bird_{name}_Albedo"
    if assets.does_asset_exist(dest):
        old=assets.load_asset(dest)
        if not isinstance(old,unreal.Texture2D):
            raise RuntimeError("已有目标资源类型不是Texture2D："+dest)
        print("BIRD_UE_TEXTURE_REUSED",dest)
        return old
    filename=SOURCE/"Textures"/f"T_DBA_Bird_{name}_Albedo_Source.png"
    task=unreal.AssetImportTask()
    task.set_editor_property("filename",str(filename))
    task.set_editor_property("destination_path",TEX_ROOT)
    task.set_editor_property("destination_name",f"T_DBA_Bird_{name}_Albedo")
    task.set_editor_property("automated",True)
    task.set_editor_property("replace_existing",False)
    task.set_editor_property("save",False)
    tools.import_asset_tasks([task])
    t=assets.load_asset(dest)
    if not isinstance(t,unreal.Texture2D):
        raise RuntimeError("源纹理导入未获得Texture2D："+dest)
    t.set_editor_property("srgb",True)
    if not assets.save_loaded_asset(t,only_if_is_dirty=False):
        raise RuntimeError("纹理保存失败："+dest)
    print("BIRD_UE_TEXTURE_SAVED",dest)
    return t


def import_mesh(unreal,tools,assets,name):
    # UE自带Blender米到厘米转换；不再施加额外100倍缩放。
    # _Cm后缀区分早期导入错误的测试资源，避免覆盖并发编辑器正在加载的网格。
    dest=f"{MESH_ROOT}/SM_DBA_Bird_{name}_Cm"
    if assets.does_asset_exist(dest):
        old=assets.load_asset(dest)
        if not isinstance(old,unreal.StaticMesh):
            raise RuntimeError("已有目标资源不是静态网格："+dest)
        print("BIRD_UE_MESH_REUSED",dest)
        return old
    opt=unreal.FbxImportUI()
    opt.set_editor_property("import_mesh",True)
    opt.set_editor_property("import_as_skeletal",False)
    opt.set_editor_property("mesh_type_to_import",unreal.FBXImportType.FBXIT_STATIC_MESH)
    opt.set_editor_property("import_materials",False)
    opt.set_editor_property("import_textures",False)
    data=opt.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes",True)
    data.set_editor_property("import_mesh_lo_ds",False)
    data.set_editor_property("import_uniform_scale",1.0)
    task=unreal.AssetImportTask()
    task.set_editor_property("filename",str(SOURCE/"Meshes"/f"SM_DBA_Bird_{name}.fbx"))
    task.set_editor_property("destination_path",MESH_ROOT)
    task.set_editor_property("destination_name",f"SM_DBA_Bird_{name}_Cm")
    task.set_editor_property("options",opt)
    task.set_editor_property("automated",True)
    task.set_editor_property("replace_existing",False)
    task.set_editor_property("save",False)
    tools.import_asset_tasks([task])
    mesh=assets.load_asset(dest)
    if not isinstance(mesh,unreal.StaticMesh):
        raise RuntimeError("FBX未生成真实UStaticMesh："+dest)
    if not assets.save_loaded_asset(mesh,only_if_is_dirty=False):
        raise RuntimeError("FBX生成的网格无法保存："+dest)
    print("BIRD_UE_MESH_SAVED",dest)
    return mesh


def make_master(unreal,tools,assets):
    path=MASTER_ROOT+"/M_GP_BirdFlock_Master"
    if assets.does_asset_exist(path):
        value=assets.load_asset(path)
        if not isinstance(value,unreal.Material):
            raise TypeError("母材质目标路径存在非Material资产："+path)
        print("BIRD_UE_MASTER_REUSED",path)
        return value

    lib=unreal.MaterialEditingLibrary
    mat=tools.create_asset("M_GP_BirdFlock_Master",MASTER_ROOT,unreal.Material,
                           unreal.MaterialFactoryNew())
    if not isinstance(mat,unreal.Material):
        raise RuntimeError("无法创建通用母材质")
    mat.set_editor_property("two_sided",True)
    mat.set_editor_property("shading_model",unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    for flag in ("used_with_instanced_static_meshes","used_with_niagara_mesh_particles"):
        try:
            mat.set_editor_property(flag,True)
        except Exception as exc:
            print("BIRD_UE_MATERIAL_USAGE_FLAG_UNAVAILABLE",flag,type(exc).__name__)

    def node(cls,x,y):
        n=lib.create_material_expression(mat,cls,x,y)
        if n is None: raise RuntimeError("材质节点创建失败："+str(cls))
        return n

    def link(a,out,b,inp):
        if lib.connect_material_expressions(a,out,b,inp):
            return
        # UE5.8 某些单输入表达式在Python API中仅暴露未命名默认引脚。
        if inp == "Input" and lib.connect_material_expressions(a,out,b,""):
            return
        raise RuntimeError("连接材质节点失败："+str(inp))

    def output(a,out,property_):
        if not lib.connect_material_property(a,out,property_):
            raise RuntimeError("材质输出失败："+str(property_))

    # 平台母材质只引用 Engine 通用纹理，不能依赖项目层贴图。
    tex=node(unreal.MaterialExpressionTextureSampleParameter2D,-1100,-450)
    tex.set_editor_property("parameter_name",unreal.Name("BirdAlbedo"))
    default=assets.load_asset("/Engine/EngineResources/DefaultTexture")
    if not isinstance(default,unreal.Texture):
        raise RuntimeError("引擎默认纹理不可用")
    tex.set_editor_property("texture",default)
    color=node(unreal.MaterialExpressionVertexColor,-1100,-130)
    color_mix=node(unreal.MaterialExpressionMultiply,-750,-360)
    link(tex,"RGB",color_mix,"A")
    link(color,"",color_mix,"B")
    output(color_mix,"",unreal.MaterialProperty.MP_BASE_COLOR)
    rough=node(unreal.MaterialExpressionScalarParameter,-480,-70)
    rough.set_editor_property("parameter_name",unreal.Name("BirdRoughness"))
    rough.set_editor_property("default_value",.82)
    output(rough,"",unreal.MaterialProperty.MP_ROUGHNESS)
    spec=node(unreal.MaterialExpressionScalarParameter,-480,120)
    spec.set_editor_property("parameter_name",unreal.Name("BirdSpecular"))
    spec.set_editor_property("default_value",.26)
    output(spec,"",unreal.MaterialProperty.MP_SPECULAR)

    # 顶点色 Alpha=翼尖权重，使用时间与每实例随机相位推动 GPU 顶点振翅。
    # 此变形只是远景翼部上下拍动；若做近景高精度翼骨变形，后续可切换VAT资源。
    time=node(unreal.MaterialExpressionTime,-1250,360)
    freq=node(unreal.MaterialExpressionScalarParameter,-1250,560)
    freq.set_editor_property("parameter_name",unreal.Name("WingBeatHz"))
    freq.set_editor_property("default_value",2.3)
    rate=node(unreal.MaterialExpressionMultiply,-980,400)
    link(time,"",rate,"A")
    link(freq,"",rate,"B")
    tau=node(unreal.MaterialExpressionConstant,-1120,710)
    tau.set_editor_property("r",6.28318530718)
    clock=node(unreal.MaterialExpressionMultiply,-740,430)
    link(rate,"",clock,"A")
    link(tau,"",clock,"B")
    per_instance=node(unreal.MaterialExpressionPerInstanceRandom,-950,790)
    phase=node(unreal.MaterialExpressionMultiply,-720,770)
    link(per_instance,"",phase,"A")
    link(tau,"",phase,"B")
    angle=node(unreal.MaterialExpressionAdd,-470,460)
    link(clock,"",angle,"A")
    link(phase,"",angle,"B")
    wave=node(unreal.MaterialExpressionSine,-260,460)
    link(angle,"",wave,"Input")
    amp=node(unreal.MaterialExpressionScalarParameter,-650,1050)
    amp.set_editor_property("parameter_name",unreal.Name("WingAmplitudeCm"))
    amp.set_editor_property("default_value",12.0)
    scaled=node(unreal.MaterialExpressionMultiply,0,480)
    link(wave,"",scaled,"A")
    link(amp,"",scaled,"B")
    flap=node(unreal.MaterialExpressionMultiply,200,480)
    link(scaled,"",flap,"A")
    link(color,"A",flap,"B")
    direction=node(unreal.MaterialExpressionConstant3Vector,210,740)
    direction.set_editor_property("constant",unreal.LinearColor(0,0,1,1))
    offset=node(unreal.MaterialExpressionMultiply,460,490)
    link(direction,"",offset,"A")
    link(flap,"",offset,"B")
    output(offset,"",unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    errors=list(lib.recompile_material(mat))
    if any("error" in str(e).lower() for e in errors):
        raise RuntimeError("母材质编译错误："+str(errors))
    if not assets.save_loaded_asset(mat,only_if_is_dirty=False):
        raise RuntimeError("通用母材质保存失败")
    print("BIRD_UE_MASTER_SAVED",path)
    return mat


def make_instances(unreal,tools,assets,master,textures,meshes):
    lib=unreal.MaterialEditingLibrary
    for name in SPECIES:
        path=f"{MI_ROOT}/MI_DBA_Bird_{name}"
        if assets.does_asset_exist(path):
            instance=assets.load_asset(path)
            if not isinstance(instance,unreal.MaterialInstanceConstant):
                raise TypeError("目标实例路径已有其他资源："+path)
            print("BIRD_UE_INSTANCE_REUSED",path)
        else:
            instance=tools.create_asset("MI_DBA_Bird_"+name,MI_ROOT,
                unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
            if not isinstance(instance,unreal.MaterialInstanceConstant):
                raise RuntimeError("创建材质实例失败："+path)
            lib.set_material_instance_parent(instance,master)
            lib.set_material_instance_texture_parameter_value(instance,
                unreal.Name("BirdAlbedo"),textures[name])
            hz={"Seagull":2.0,"Swallow":4.3,"Eagle":1.5}[name]
            amp={"Seagull":11.0,"Swallow":7.0,"Eagle":20.0}[name]
            lib.set_material_instance_scalar_parameter_value(instance,unreal.Name("WingBeatHz"),hz)
            lib.set_material_instance_scalar_parameter_value(instance,unreal.Name("WingAmplitudeCm"),amp)
            if not assets.save_loaded_asset(instance,only_if_is_dirty=False):
                raise RuntimeError("材质实例保存失败："+path)
            print("BIRD_UE_INSTANCE_SAVED",path)
        # 有其他Editor读取网格时，反复保存已落盘FBX会遇到Windows文件共享锁。
        # 因此不再重新写SM资源；运行时Niagara/蓝图组件只引用独立MI作为材质覆盖。
        if not isinstance(meshes[name],unreal.StaticMesh):
            raise RuntimeError("鸟模型类型异常："+name)
        if not isinstance(assets.load_asset(path),unreal.MaterialInstanceConstant):
            raise RuntimeError("材质实例重新加载失败："+path)


def main():
    data=verified()
    for entry in data["files"]:
        print("BIRD_SOURCE_CHECK_OK",entry["path"],entry["bytes"])
    if MODE=="inspect":
        print("BIRD_UE_ASSET_PLAN",list(SPECIES),MASTER_ROOT,MESH_ROOT,TEX_ROOT,MI_ROOT)
        return
    if MODE!="apply":
        raise RuntimeError("不支持执行模式："+MODE)
    unreal=require_ue()
    assets=unreal.EditorAssetLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    for path in (MASTER_ROOT,MESH_ROOT,TEX_ROOT,MI_ROOT):
        ensure_dir(assets,path)
    textures={name:import_texture(unreal,tools,assets,name) for name in SPECIES}
    meshes={name:import_mesh(unreal,tools,assets,name) for name in SPECIES}
    master=make_master(unreal,tools,assets)
    make_instances(unreal,tools,assets,master,textures,meshes)
    for name in SPECIES:
        mesh=assets.load_asset(f"{MESH_ROOT}/SM_DBA_Bird_{name}_Cm")
        if not isinstance(mesh,unreal.StaticMesh):
            raise RuntimeError("鸟模型回读缺少有效几何："+name)
    print("BIRD_UE_ASSET_CREATION_OK",len(SPECIES))


if __name__=="__main__":
    main()
