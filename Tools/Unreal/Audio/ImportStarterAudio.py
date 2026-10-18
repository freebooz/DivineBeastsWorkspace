# -*- coding: utf-8 -*-
"""SSLM首批商用CC0音乐/音效真实UE5.8资产导入器。

所有权：第三层DBASFXPack_Core客户端资源；本脚本仅在编辑器执行，不创造假uasset。
默认只读Inspect；传SSLM_AUDIO_IMPORT_MODE=apply才创建或更新真实SoundWave/SFX定义。
声明只覆盖客户端音频资源，不接触游戏权威、不修改已存在不同类型资产。
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBASFXPack_Core/SourceArt/Audio"
MODE = os.environ.get("SSLM_AUDIO_IMPORT_MODE", "inspect").lower().strip()
# folder、文件名、定义名、稳定LogicalId、2D/3D、循环、音量、淡入淡出，均以中文说明。
AUDIO = [
 ("UI","SW_DBA_UI_Click_01.ogg","DA_DBA_SFX_UI_Click","presentation.dba.audio.ui.click@1","2D",False,1.0,0.0,0.0),
 ("UI","SW_DBA_UI_Confirm_01.ogg","DA_DBA_SFX_UI_Confirm","presentation.dba.audio.ui.confirm@1","2D",False,1.0,0.0,0.0),
 ("UI","SW_DBA_UI_Cancel_01.ogg","DA_DBA_SFX_UI_Cancel","presentation.dba.audio.ui.cancel@1","2D",False,1.0,0.0,0.0),
 ("Spells","SW_DBA_Spell_Frost_Cast_01.ogg","DA_DBA_SFX_Spell_FrostCast","presentation.dba.audio.spell.frost.cast@1","World",False,0.85,0.0,0.0),
 ("Spells","SW_DBA_Spell_Frost_Impact_01.ogg","DA_DBA_SFX_Spell_FrostImpact","presentation.dba.audio.spell.frost.impact@1","World",False,0.9,0.0,0.0),
 ("Spells","SW_DBA_Spell_Fire_Impact_01.wav","DA_DBA_SFX_Spell_FireImpact","presentation.dba.audio.spell.fire.impact@1","World",False,0.85,0.0,0.0),
 ("Music","SW_DBA_Music_Login_01.ogg","DA_DBA_SFX_Music_Login","presentation.dba.audio.music.login@1","2D",True,0.45,1.8,1.8),
 ("Music","SW_DBA_Music_Village_01.mp3","DA_DBA_SFX_Music_Village","presentation.dba.audio.music.village@1","2D",True,0.40,1.8,1.8),
 ("Ambience","SW_DBA_Ambience_Forest_01.mp3","DA_DBA_SFX_Ambience_Forest","presentation.dba.audio.ambience.forest@1","2D",True,0.22,2.0,2.0)
]


def inspect():
    for folder, filename, definition, logical_id, space, loop, volume, fade_in, fade_out in AUDIO:
        source = SRC / folder / filename
        if not source.is_file():
            raise RuntimeError("缺失合法来源音频: " + str(source))
        if source.stat().st_size < 1000 or source.stat().st_size > 30000000:
            raise RuntimeError("异常音频文件大小: " + filename)
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        print("SSLM_AUDIO_SOURCE", filename, str(source.stat().st_size), digest)


def author():
    import unreal   # 仅UE Editor可加载的引擎Python模块
    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    cls = unreal.GamePlatformSFXDefinition
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)

    for folder, filename, definition_name, logical_id, space, loop, volume, fade_in, fade_out in AUDIO:
        stem = Path(filename).stem
        # 引擎SoundWave执行GP.Naming审查要求S_前缀；保留原始SourceArt文件名便于溯源。
        asset_stem = "S_" + stem[3:] if stem.startswith("SW_") else stem
        audio_dir = "/DBASFXPack_Core/Audio/" + folder + "/Waves"
        definition_dir = "/DBASFXPack_Core/Audio/" + folder + "/Definitions"
        sound_path = audio_dir + "/" + asset_stem
        old_sound_path = audio_dir + "/" + stem
        definition_path = definition_dir + "/" + definition_name
        # 对本次导入的旧命名资产通过UE编辑器原生命名重定向，避免直接修改二进制文件。
        if old_sound_path != sound_path and assets.does_asset_exist(old_sound_path):
            if assets.does_asset_exist(sound_path):
                raise RuntimeError("原资产与规范资产同时存在，拒绝自动覆盖: " + stem)
            if not assets.rename_asset(old_sound_path, sound_path):
                raise RuntimeError("UE SoundWave重命名失败: " + old_sound_path)
            print("SSLM_AUDIO_RENAMED", old_sound_path, sound_path)
        for directory in (audio_dir, definition_dir):
            if not assets.does_directory_exist(directory):
                assets.make_directory(directory)
        if assets.does_asset_exist(sound_path):
            sound = assets.load_asset(sound_path)
            if not isinstance(sound, unreal.SoundWave):
                raise TypeError("已有不兼容声音资产，停止覆盖: " + sound_path)
        else:
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", str(SRC / folder / filename))
            task.set_editor_property("destination_path", audio_dir)
            task.set_editor_property("destination_name", asset_stem)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", False)
            task.set_editor_property("save", False)
            tools.import_asset_tasks([task])
            sound = assets.load_asset(sound_path)
            if not isinstance(sound, unreal.SoundWave):
                raise RuntimeError("SoundWave导入失败: " + filename)
        sound.set_editor_property("looping", bool(loop))
        if not assets.save_loaded_asset(sound, only_if_is_dirty=False):
            raise RuntimeError("SoundWave保存失败: " + sound_path)

        if assets.does_asset_exist(definition_path):
            definition = assets.load_asset(definition_path)
            if not isinstance(definition, cls):
                raise TypeError("已有不兼容定义资产，停止覆盖: " + definition_path)
        else:
            definition = tools.create_asset(definition_name, definition_dir, cls, factory)
            if not isinstance(definition, cls):
                raise RuntimeError("SFX定义创建失败: " + definition_name)
        logical, version = logical_id.rsplit("@", 1)
        namespace, name = logical.rsplit(".", 1)
        ident = unreal.GamePlatformId()
        ident.set_editor_property("namespace", namespace)
        ident.set_editor_property("name", name)
        ident.set_editor_property("logical_version", int(version))
        definition.set_editor_property("logical_id", ident)
        definition.set_editor_property("sound", sound)
        definition.set_editor_property("playback_space",
            unreal.GamePlatformSFXPlaybackSpace.TWO_D if space == "2D"
            else unreal.GamePlatformSFXPlaybackSpace.WORLD)
        definition.set_editor_property("volume_multiplier", float(volume))
        definition.set_editor_property("fade_in_seconds", float(fade_in))
        definition.set_editor_property("fade_out_seconds", float(fade_out))
        if not assets.save_loaded_asset(definition, only_if_is_dirty=False):
            raise RuntimeError("定义保存失败: " + definition_name)
        if not isinstance(assets.load_asset(definition_path), cls):
            raise RuntimeError("定义回读失败: " + definition_path)
        print("SSLM_AUDIO_IMPORTED", sound_path, definition_path)
    print("SSLM_AUDIO_IMPORT_OK", len(AUDIO))


if __name__ == "__main__":
    inspect()
    if MODE == "inspect":
        print("SSLM_AUDIO_INSPECT_ONLY", len(AUDIO))
    elif MODE == "apply":
        author()
    else:
        raise ValueError("SSLM_AUDIO_IMPORT_MODE仅允许inspect或apply")
