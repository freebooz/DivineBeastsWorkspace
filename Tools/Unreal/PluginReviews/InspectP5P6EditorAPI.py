#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P5/P6真实UE5.8 编辑器反射能力只读检查，不创建或修改工程资产。"""
import unreal
wanted=("GamePlatformAIController","GamePlatformInteractableComponent","GamePlatformInteractorComponent",
    "GamePlatformAIStateComponent","GamePlatformArenaGameMode","DivineBeastsGameplayCharacter",
    "GameModeBase","BlueprintFactory","SubobjectDataSubsystem","AddNewSubobjectParams",
    "BlueprintEditorLibrary","NavMeshBoundsVolume","PlayerStart","StaticMeshActor",
    "DataAssetFactory","BehaviorTreeFactory","BlackboardDataFactory")
print("P5P6_ENGINE",unreal.SystemLibrary.get_engine_version())
print("P5P6_PROJECT",unreal.Paths.get_project_file_path())
for name in wanted:
    klass=getattr(unreal,name,None)
    print("P5P6_API",name, bool(klass),str(klass)[:110])
    if name in ("SubobjectDataSubsystem","AddNewSubobjectParams","BlueprintEditorLibrary") and klass:
        print("P5P6_API_METHODS",name,[v for v in dir(klass) if any(k in v.lower() for k in ("add","gather","compile","class","component","subobject"))][:50])
if hasattr(unreal,"GameModeBase"):
    print("P5P6_GAMEMODE_PROPS",[v for v in dir(unreal.GameModeBase) if any(k in v.lower() for k in ("pawn","game_mode"))][:40])
path="/DBAContentPack_Common/Mannequins/UE5/Meshes/SK_Mannequin"
print("P5P6_ASSET_COMMON_MOUNT",unreal.EditorAssetLibrary.does_directory_exist("/DBAContentPack_Common"))
print("P5P6_ASSET_MANNEQUIN_EXIST",unreal.EditorAssetLibrary.does_asset_exist(path))
print("P5P6_ASSET_MANNEQUIN_LOADED",isinstance(unreal.EditorAssetLibrary.load_asset(path),unreal.SkeletalMesh))
for path2 in ("/DBAContentPack_Common/Mannequins/DBA/Meshes/SK_Mannequin",
              "/DBAContentPack_Common/Mannequins/UE5/Meshes/SK_Mannequin",
              "/Engine/EngineMeshes/SkeletalCube"):
    obj=unreal.EditorAssetLibrary.load_asset(path2)
    print("P5P6_ASSET_CLASS",path2,(obj.get_class().get_name() if obj else "None"))
print("P5P6_PROBE_COMPLETE")
