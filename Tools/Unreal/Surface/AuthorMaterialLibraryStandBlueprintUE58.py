#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""材质函数库审查展示架：真正UE Actor Blueprint（蓝图）原生作者。

范围：仅 /Game/Development/MaterialLibrary/Blueprints 下新建一份BP。
PreviewMesh 是已命名的 StaticMeshComponent（静态网格组件），使用引擎Sphere作为中性
预览模型，正式关卡和平台插件不依赖本项目审核资源。禁用碰撞、Tick、Replicates。
模式：inspect默认只读；apply创建蓝图+组件并原生保存；verify只读检查实际SCS模板；
finalize由用户要求在所有制作完成后统一触发蓝图编译与回读。
支持断点重连：已存在资源只读检查，不覆盖、不修改人工蓝图。
"""
from __future__ import annotations
import argparse
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
ASSET="/Game/Development/MaterialLibrary/Blueprints/BP_DBA_MaterialLibraryReviewStand"
CONTENT=ROOT/"Game/Content/Development/MaterialLibrary/Blueprints"
MESH="/Engine/BasicShapes/Sphere"


def check_owner(unreal) -> None:
    """防止在另一工程意外修改同名资源。"""
    project=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("仅允许在DivineBeastsWorkspace正式工程制作材质审核蓝图：" + project)


def physical_collides() -> bool:
    """UE AssetRegistry暂时缺失时不能强制占用已经存在的文件与侧车包。"""
    path=CONTENT/(ASSET.rsplit("/",1)[-1]+".uasset")
    return any(path.with_suffix(ext).exists() for ext in (".uasset",".uexp",".ubulk",".umap"))


def find_preview(unreal, blueprint):
    """读取实际Blueprint SCS模板，不能只凭控件名猜测组件已经创建。"""
    subsystem=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library=unreal.SubobjectDataBlueprintFunctionLibrary
    templates={}
    handles=subsystem.k2_gather_subobject_data_for_blueprint(blueprint)
    for handle in handles:
        data=library.get_data(handle)
        obj=library.get_object_for_blueprint(data,blueprint)
        if isinstance(obj,unreal.StaticMeshComponent):
            templates[obj.get_path_name()]=obj
    return list(templates.values())


def make_asset(unreal):
    """新建Actor蓝图，在真实SCS里添加PreviewMesh并绑定引擎中性网格。"""
    lib=unreal.EditorAssetLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    if lib.does_asset_exist(ASSET):
        bp=lib.load_asset(ASSET)
        if not isinstance(bp,unreal.Blueprint):
            raise TypeError("审核蓝图路径被其它原生类型占用：" + ASSET)
        components=find_preview(unreal,bp)
        if len(components)!=1 or components[0].get_name().split("_")[0]!="PreviewMesh":
            raise RuntimeError("既有审核蓝图组件身份不匹配，拒绝自动修改：" + ASSET)
        print("MATERIAL_LIBRARY_STAND_BLUEPRINT_PRESERVED",ASSET)
        return bp
    if physical_collides():
        raise RuntimeError("磁盘已有同名未注册UE蓝图包，拒绝覆盖")
    directory=ASSET.rsplit("/",1)[0]
    if not lib.does_directory_exist(directory):
        lib.make_directory(directory)
    sphere=lib.load_asset(MESH)
    if not isinstance(sphere,unreal.StaticMesh):
        raise FileNotFoundError("UE中性审查网格缺失："+MESH)
    factory=unreal.BlueprintFactory()
    factory.set_editor_property("parent_class",unreal.Actor)
    bp=tools.create_asset(ASSET.rsplit("/",1)[-1],directory,unreal.Blueprint,factory)
    if not isinstance(bp,unreal.Blueprint):
        raise RuntimeError("UE没有真正生成Actor Blueprint")
    sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles=sub.k2_gather_subobject_data_for_blueprint(bp)
    if not handles:
        raise RuntimeError("新Blueprint缺少SCS根句柄")
    params=unreal.AddNewSubobjectParams(parent_handle=handles[0],
                                       new_class=unreal.StaticMeshComponent,
                                       blueprint_context=bp)
    handle,reason=sub.add_new_subobject(params=params)
    if str(reason):
        raise RuntimeError("SCS PreviewMesh创建失败："+str(reason))
    if not sub.rename_subobject(handle=handle,new_name=unreal.Text("PreviewMesh")):
        raise RuntimeError("真实SCS组件无法命名PreviewMesh")
    components=find_preview(unreal,bp)
    if len(components)!=1:
        raise RuntimeError("新Blueprint没有唯一StaticMeshComponent")
    preview=components[0]
    preview.set_static_mesh(sphere)
    preview.set_editor_property("cast_shadow",True)
    preview.set_editor_property("mobility",unreal.ComponentMobility.MOVABLE)
    preview.set_editor_property("collision_enabled",unreal.CollisionEnabled.NO_COLLISION)
    if not lib.save_loaded_asset(bp,only_if_is_dirty=False):
        raise RuntimeError("UE没有保存真实Blueprint二进制资源")
    print("MATERIAL_LIBRARY_STAND_BLUEPRINT_SAVED",ASSET)
    return bp


def main():
    """依照用户顺序先源码与Blueprint，再统一编译和运行验收。"""
    parser=argparse.ArgumentParser(description="UE5.8材质库独立审核展示架原生BP")
    parser.add_argument("--mode",choices=("inspect","apply","verify","finalize"),
                        default=os.environ.get("MATERIAL_LIBRARY_BP_MODE","inspect"))
    opts,_=parser.parse_known_args()
    print("MATERIAL_LIBRARY_BP_TARGET",ASSET,MESH)
    if opts.mode=="inspect":
        print("INSPECT_ONLY：不生成.uasset或触发蓝图编译")
        return
    try:import unreal # type: ignore[import-not-found]
    except ImportError as error:
        raise RuntimeError("蓝图资产必须由真实UE5.8 Editor制作") from error
    check_owner(unreal)
    lib=unreal.EditorAssetLibrary
    bp=make_asset(unreal) if opts.mode=="apply" else lib.load_asset(ASSET)
    if not isinstance(bp,unreal.Blueprint):
        raise FileNotFoundError("蓝图尚未通过apply制作："+ASSET)
    components=find_preview(unreal,bp)
    if len(components)!=1:
        raise RuntimeError("真实蓝图SCS组件数量异常："+str(len(components)))
    preview=components[0]
    mesh=preview.get_editor_property("static_mesh")
    if not isinstance(mesh,unreal.StaticMesh):
        raise RuntimeError("审查用蓝图没有真实中性网格")
    if opts.mode=="finalize":
        # finalize属于用户指定的统一蓝图编译阶段，默认apply不执行。
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        if not lib.save_loaded_asset(bp,only_if_is_dirty=False):
            raise RuntimeError("最终蓝图编译后保存失败")
        klass=lib.load_blueprint_class(ASSET)
        if klass is None:
            raise RuntimeError("生成的蓝图类无法由UE加载")
        print("MATERIAL_LIBRARY_BP_FINALIZED",ASSET,klass.get_path_name())
        return
    print("MATERIAL_LIBRARY_BP_NATIVE_CHECK",ASSET,preview.get_name(),mesh.get_path_name(),
          "fullBlueprintCompilePending",True)


if __name__=="__main__":
    main()
