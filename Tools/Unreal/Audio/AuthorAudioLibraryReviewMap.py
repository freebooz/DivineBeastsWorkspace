# -*- coding: utf-8 -*-
"""SSLM第二批音频独立人工审查关卡制作器（只由UE5.8 Editor生成.umap）。

目的：通过真实UE原生地图和AmbientSound（环境声音Actor）人工检查声源空间/循环/音质。
仅在 /Game/Development/Audio 新建审核地图，不修改正式Village、Shared服务器地图、
音频主资产或Runtime中立目录。默认inspect不会修改。apply时拒绝覆盖现存地图。
"""
from __future__ import annotations

import os
import unreal

MAP = "/Game/Development/Audio/L_SSLM_AudioLibrary_Review"
MODE = os.environ.get("SSLM_AUDIO_REVIEW_MODE","inspect").lower().strip()
MOUNT = "/DBASFXPack_Core/Audio"
SOUNDSET = (
    ("流水A（空间循环）","Library/Ambience/Waves/S_DBA_Ambience_WaterStreamA",True),
    ("流水B（空间试听）","Library/Ambience/Waves/S_DBA_Ambience_WaterStreamB",False),
    ("鸟鸣（单次）","Library/Ambience/Waves/S_DBA_Ambience_Birds",False),
    ("落叶脚步（单次）","Library/Footsteps/Waves/S_DBA_Footsteps_LeavesA",False),
    ("石面脚步（单次）","Library/Footsteps/Waves/S_DBA_Footsteps_Stone",False),
    ("狼嚎（单次）","Library/Creatures/Waves/S_DBA_Creatures_Howl",False),
    ("狼吠（单次）","Library/Creatures/Waves/S_DBA_Creatures_Bark",False),
    ("奥术施法（单次）","Library/Spells/Waves/S_DBA_Spells_ArcaneCast",False),
    ("治疗术（单次）","Library/Spells/Waves/S_DBA_Spells_Healing",False),
    ("传送（单次）","Library/Spells/Waves/S_DBA_Spells_Teleport",False),
    ("兵器碰撞（单次）","Library/Combat/Waves/S_DBA_Combat_SwordClashA",False),
    ("界面确认（二维试听）","UI/Waves/S_DBA_UI_Confirm_01",False)
)

def inspect():
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/DBASFXPack_Core"],True)
    library=unreal.EditorAssetLibrary
    print("SSLM_AUDIO_REVIEW_EXISTING_MAP",library.does_asset_exist(MAP))
    for label,rel,loop in SOUNDSET:
        item=library.load_asset(f"{MOUNT}/{rel}")
        if not isinstance(item,unreal.SoundWave):
            raise RuntimeError("试听声音真实uasset缺失或类型错误："+rel)
        print("SSLM_AUDIO_REVIEW_SOURCE_OK",label,rel,loop)

def author():
    library=unreal.EditorAssetLibrary
    if library.does_asset_exist(MAP):
        raise RuntimeError("审核地图已存在，拒绝覆盖其他编辑器内容："+MAP)
    created=unreal.EditorLevelLibrary.new_level(MAP)
    if not created:
        raise RuntimeError("UE无法创建本次独立审核地图："+MAP)
    # 所有坐标仅用于独立审查关卡，不是Village/瀑布真实世界位置。
    floor=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-75))
    if floor:
        floor.set_actor_label("00_审查区域地面（非正式游戏世界）")
        cube=library.load_asset("/Engine/BasicShapes/Cube")
        if cube and isinstance(cube,unreal.StaticMesh):
            floor.static_mesh_component.set_static_mesh(cube)
            floor.set_actor_scale3d(unreal.Vector(110,75,1.0))
    atten=library.load_asset(f"{MOUNT}/Shared/Policies/S_DBA_AudioLibrary_Ambient_Attenuation")
    common=library.load_asset(f"{MOUNT}/Shared/Policies/S_DBA_SpellShort_Attenuation")
    if not isinstance(atten,unreal.SoundAttenuation) or not isinstance(common,unreal.SoundAttenuation):
        raise RuntimeError("3D试听音效的真实空间衰减策略缺失")
    count=0
    for index,(label,relative,loop) in enumerate(SOUNDSET):
        row=index//4
        col=index%4
        position=unreal.Vector(float(col*1850-2800),float(row*1550-1800),165.0)
        actor=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.AmbientSound,position)
        if not actor:
            raise RuntimeError("无法创建环境声音Actor："+label)
        actor.set_actor_label(f"{index+1:02d}_{label}")
        component=actor.get_editor_property("audio_component")
        asset=library.load_asset(f"{MOUNT}/{relative}")
        if not isinstance(asset,unreal.SoundWave) or not component:
            raise RuntimeError("音频审查对象缺失："+relative)
        component.set_sound(asset)
        component.set_editor_property("auto_activate",bool(loop))
        if index!=11:
            component.set_editor_property("attenuation_settings", atten if index<3 else common)
        count+=1
        print("SSLM_AUDIO_REVIEW_ACTOR_CREATED",index+1,relative)
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("UE无法保存新建音频审核.umap")
    if not library.does_asset_exist(MAP):
        raise RuntimeError("审查地图保存后资产注册不可见")
    print("SSLM_AUDIO_REVIEW_MAP_OK",MAP,count)

if __name__ == "__main__":
    inspect()
    if MODE=="apply":
        author()
    elif MODE=="inspect":
        print("SSLM_AUDIO_REVIEW_INSPECT_ONLY",len(SOUNDSET))
    else:
        raise ValueError("SSLM_AUDIO_REVIEW_MODE仅允许inspect/apply")
