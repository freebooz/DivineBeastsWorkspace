#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P5开发角色的真实蓝图父类与SkeletalMesh独立UE编辑器回读，不改资源。"""
from __future__ import annotations
import unreal  # type: ignore

ASSETS=unreal.EditorAssetLibrary
PATH="/Game/Development/Foundation/PluginReviews/Blueprints/BP_GP_P5ReviewCharacter"
MESH="/DBAContentPack_Common/Mannequins/DBA/Meshes/SK_Mannequin"
assert str(unreal.SystemLibrary.get_engine_version()).startswith("5.8.")
if not ASSETS.does_asset_exist(PATH):raise RuntimeError("开发角色蓝图不存在")
cls=ASSETS.load_blueprint_class(PATH)
if cls is None:raise RuntimeError("无法装载项目角色蓝图类")
cdo=unreal.get_default_object(cls)
print("P5P6_PAWN_CLASS",str(cls))
print("P5P6_PAWN_CDO",str(cdo))
component=cdo.get_editor_property("mesh")
if not isinstance(component,unreal.SkeletalMeshComponent):
    raise RuntimeError("不是正式ACharacter的SkeletalMesh组件")
mesh=component.get_skeletal_mesh_asset()
result=mesh.get_path_name().split(".")[0] if mesh else ""
print("P5P6_PAWN_SKELETAL_MESH",result)
if result!=MESH:
    raise RuntimeError("审核Pawn的真实SkeletalMesh未能在独立编辑器重启后持久化："+result)
print("P5P6_PAWN_MESH_RELOAD_OK")
