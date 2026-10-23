# -*- coding: utf-8 -*-
"""只读检查默认音乐的真实UE资产、共享引用和流式配置；不代替设备听音。"""
import json
from pathlib import Path
import unreal

WAVE_PATH = "/DBASFXPack_Core/Audio/Music/Waves/S_DBA_Music_GoldenAutumnPond"
DEFINITION_ROOT = "/DBASFXPack_Core/Audio/Music/Definitions/DA_DBA_SFX_Music_"


def validate():
    """验证完整曲目只导入一次，三个场景定义共同引用且均走音乐分类。"""
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/DBASFXPack_Core"], True)
    wave = unreal.load_asset(WAVE_PATH)
    assert isinstance(wave, unreal.SoundWave), "尚未导入Golden Autumn Pond真实SoundWave"
    assert abs(wave.get_editor_property("duration") - 517.32) < .1, "音乐时长不完整"
    assert wave.get_editor_property("num_channels") == 2, "必须保留原双声道"
    assert wave.get_editor_property("looping"), "默认背景音乐必须循环"
    assert wave.get_editor_property("virtualization_mode") == unreal.VirtualizationMode.PLAY_WHEN_SILENT, "音乐必须支持从静音即时恢复"
    assert wave.get_editor_property("loading_behavior") == unreal.SoundWaveLoadingBehavior.PRIME_ON_LOAD, "长音乐须使用原生流式预热"
    definitions = []
    for scene in ("Default", "Login", "Village"):
        definition = unreal.load_asset(DEFINITION_ROOT + scene)
        assert isinstance(definition, unreal.GamePlatformSFXDefinition), "缺少音乐定义：" + scene
        assert definition.get_editor_property("sound") == wave, "音乐定义未引用默认曲目：" + scene
        assert definition.get_editor_property("sound_group") == unreal.GamePlatformSFXSoundGroup.MUSIC, "错误的用户音量分类：" + scene
        assert definition.get_editor_property("playback_space") == unreal.GamePlatformSFXPlaybackSpace.TWO_D, "背景音乐不能使用空间衰减"
        assert definition.get_editor_property("concurrency"), "缺少原生音乐并发策略"
        assert definition.get_editor_property("fade_in_seconds") > 0, "缺少淡入"
        identity = definition.get_editor_property("logical_id")
        assert str(identity.get_editor_property("namespace")) == "presentation.dba.audio.music"
        assert str(identity.get_editor_property("name")) == scene.lower(), "音乐稳定身份冲突：" + scene
        definitions.append(definition.get_path_name())
    report = {"passed": True, "wave": wave.get_path_name(), "durationSeconds": wave.get_editor_property("duration"),
              "definitions": definitions, "streaming": "PrimeOnLoad", "audibleAcceptance": False}
    print("SSLM_DEFAULT_MUSIC_NATIVE_OK " + json.dumps(report))
    return report


if __name__ == "__main__":
    result = validate()
    output = Path(__file__).resolve().parents[3] / "Saved/Validation/DefaultMusic20261023/ColdReloadFinal.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2), encoding="utf-8")
