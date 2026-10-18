# -*- coding: utf-8 -*-
"""UE5.8独立进程冷重载音频审核地图并验证12个原生声源Actor（只读）。"""
import unreal

MAP = "/Game/Development/Audio/L_SSLM_AudioLibrary_Review"
reg=unreal.AssetRegistryHelpers.get_asset_registry()
reg.scan_paths_synchronous(["/Game/Development/Audio", "/DBASFXPack_Core"], True)
asset=unreal.EditorAssetLibrary.load_asset(MAP)
if not isinstance(asset, unreal.World):
    raise RuntimeError("音频审核umap无法作为UE World冷重载："+MAP)
level=unreal.EditorLevelLibrary.load_level(MAP)
if level is None or level is False:
    raise RuntimeError("音频审核地图无法成为编辑器当前Level")
actors=unreal.EditorLevelLibrary.get_all_level_actors()
sounds=[]
loops=[]
for actor in actors:
    if not isinstance(actor, unreal.AmbientSound):
        continue
    audio=actor.get_editor_property("audio_component")
    sound=audio.get_editor_property("sound") if audio else None
    if not isinstance(sound, unreal.SoundWave):
        raise RuntimeError("审核声源存在无效SoundWave："+actor.get_actor_label())
    sounds.append(actor.get_actor_label())
    if audio.get_editor_property("auto_activate"):
        loops.append(actor.get_actor_label())
    print("SSLM_AUDIO_REVIEW_COLD_ACTOR_OK",actor.get_actor_label(),sound.get_name())
if len(sounds)!=12 or len(loops)!=1:
    raise RuntimeError(f"音频试听关卡数量/自动循环异常：sound={len(sounds)} loop={len(loops)}")
print("SSLM_AUDIO_REVIEW_COLD_VERIFY_OK",len(sounds),len(loops),MAP)
