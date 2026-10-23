# -*- coding: utf-8 -*-
"""在本工程UE编辑器内导入用户指定的默认音乐；只能通过真实引擎资产工具执行。"""
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[3]
PACK = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBASFXPack_Core"
WAVE_ROOT = "/DBASFXPack_Core/Audio/Music/Waves"
NAME = "S_DBA_Music_GoldenAutumnPond"
DEFINITION_ROOT = "/DBASFXPack_Core/Audio/Music/Definitions"


def author():
    """只拥有本次新曲目和三个音乐定义；保留既有定义身份、旧音频与其它未保存资产。"""
    assert Path(unreal.Paths.get_project_file_path()).resolve() == (ROOT / "Game/DivineBeastsArena.uproject").resolve()
    source = PACK / "SourceArt/Audio/Music/GoldenAutumnPond.wav"
    assert source.is_file(), "缺少已提取的原采样率PCM"
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.EditorAssetLibrary
    wave = lib.load_asset(WAVE_ROOT + "/" + NAME) if lib.does_asset_exist(WAVE_ROOT + "/" + NAME) else None
    if wave is None:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", WAVE_ROOT)
        task.set_editor_property("destination_name", NAME)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", False)
        tools.import_asset_tasks([task])
        wave = lib.load_asset(WAVE_ROOT + "/" + NAME)
    assert isinstance(wave, unreal.SoundWave), "实际SoundWave导入失败"
    wave.set_editor_property("looping", True)
    wave.set_editor_property("sound_group", unreal.SoundGroup.SOUNDGROUP_MUSIC)
    wave.set_editor_property("loading_behavior", unreal.SoundWaveLoadingBehavior.PRIME_ON_LOAD)
    # 沿用既有音乐合同：从默认静音调高音量时不重启或丢失循环声音。
    wave.set_editor_property("virtualization_mode", unreal.VirtualizationMode.PLAY_WHEN_SILENT)
    wave.set_editor_property("compression_quality", 80)
    wave.set_editor_property("volume", 1.0)
    wave.set_editor_property("pitch", 1.0)
    assert lib.save_loaded_asset(wave, only_if_is_dirty=False), "音乐波形保存失败"
    default_path = DEFINITION_ROOT + "/DA_DBA_SFX_Music_Default"
    if not lib.does_asset_exist(default_path):
        assert lib.duplicate_asset(DEFINITION_ROOT + "/DA_DBA_SFX_Music_Village", default_path), "创建项目默认定义失败"
    definitions = []
    for scene in ("Default", "Login", "Village"):
        definition = lib.load_asset(DEFINITION_ROOT + "/DA_DBA_SFX_Music_" + scene)
        assert isinstance(definition, unreal.GamePlatformSFXDefinition), "音乐定义类型错误"
        if scene == "Default":
            identity = unreal.GamePlatformId()
            identity.set_editor_property("namespace", "presentation.dba.audio.music")
            identity.set_editor_property("name", "default")
            identity.set_editor_property("logical_version", 1)
            definition.set_editor_property("logical_id", identity)
        definition.set_editor_property("sound", wave)
        definition.set_editor_property("sound_group", unreal.GamePlatformSFXSoundGroup.MUSIC)
        definition.set_editor_property("playback_space", unreal.GamePlatformSFXPlaybackSpace.TWO_D)
        assert lib.save_loaded_asset(definition, only_if_is_dirty=False), "音乐定义保存失败"
        definitions.append(definition.get_path_name())
    report = {"project": unreal.Paths.get_project_file_path(), "wave": wave.get_path_name(),
              "definitions": definitions, "durationSeconds": wave.get_editor_property("duration")}
    output = ROOT / "Saved/Validation/DefaultMusic20261023/Import.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print("SSLM_DEFAULT_MUSIC_IMPORTED " + json.dumps(report))


if __name__ == "__main__":
    author()
