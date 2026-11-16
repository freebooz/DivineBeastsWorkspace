# -*- coding: utf-8 -*-
"""UE5.8新增32项SoundWave/Definition冷重载只读验收。

只验证真实资产与配置内容，不等于真实扬声器播放、Server Cook或人工音质验收。
"""
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[3]
manifest = root / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBASFXPack_Core/Docs/SSLM_AudioLibraryExpansion_20261018.json"
data = json.loads(manifest.read_text(encoding="utf-8"))
if len(data["entries"]) != 32:
    raise RuntimeError("新增声音数不为32")
reg = unreal.AssetRegistryHelpers.get_asset_registry()
reg.scan_paths_synchronous(["/DBASFXPack_Core"], True)
lib = unreal.EditorAssetLibrary
mount = data["mount"]
completed = 0
for item in data["entries"]:
    category, ident = item["category"], item["id"]
    wave = mount + "/" + category + "/Waves/" + Path(item["filename"]).stem
    definition = mount + "/" + category + "/Definitions/DA_DBA_SFX_" + category + "_" + ident
    sound = lib.load_asset(wave)
    df = lib.load_asset(definition)
    if not isinstance(sound, unreal.SoundWave) or not isinstance(df, unreal.GamePlatformSFXDefinition):
        raise RuntimeError("声音或定义冷重载失败：" + wave + " | " + definition)
    if bool(sound.get_editor_property("looping")) != bool(item["looping"]):
        raise RuntimeError("声音循环策略错误：" + wave)
    if not df.get_editor_property("sound") or not df.get_editor_property("concurrency"):
        raise RuntimeError("定义缺原生声音/并发：" + definition)
    if item["playbackSpace"] == "World" and not df.get_editor_property("attenuation"):
        raise RuntimeError("三维声音缺衰减：" + definition)
    ident_val = df.get_editor_property("logical_id")
    expected = item["logicalId"].rsplit("@", 1)
    ns, name = expected[0].rsplit(".", 1)
    if str(ident_val.get_editor_property("namespace")) != ns or str(ident_val.get_editor_property("name")) != name:
        raise RuntimeError("Definition逻辑身份未匹配：" + definition)
    completed += 1
    print("SSLM_AUDIO_LIBRARY_COLD_OK", category, ident)
for name, kind in [
    ("S_DBA_AudioLibrary_Footstep_Concurrency", unreal.SoundConcurrency),
    ("S_DBA_AudioLibrary_Creature_Concurrency", unreal.SoundConcurrency),
    ("S_DBA_AudioLibrary_Ambience_Concurrency", unreal.SoundConcurrency),
    ("S_DBA_AudioLibrary_Ambient_Attenuation", unreal.SoundAttenuation),
]:
    obj = lib.load_asset("/DBASFXPack_Core/Audio/Shared/Policies/" + name)
    if not isinstance(obj, kind):
        raise RuntimeError("新增声音策略资产失效：" + name)
print("SSLM_AUDIO_LIBRARY_COLD_VERIFY_OK", completed, 4)
