# -*- coding: utf-8 -*-
"""SSLM 新增音频库：UE5.8 SoundWave/Definition 原生资产导入器。

默认为 inspect（只读审查SHA/授权）；SSLM_AUDIO_LIBRARY_MODE=apply 才真正制作uasset。
音效逻辑统一复用 GamePlatformSFX、GamePlatformData，不创建重复的播放器/资源目录。
全部资产位于客户端专用DBASFXPack_Core，不得放入共享的Village Server地图内容。
"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PACK = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBASFXPack_Core"
MANIFEST = PACK / "Docs/SSLM_AudioLibraryExpansion_20261018.json"
SOURCE = PACK / "SourceArt/Audio/Library"
MODE = os.environ.get("SSLM_AUDIO_LIBRARY_MODE", "inspect").lower().strip()
MOUNT = "/DBASFXPack_Core/Audio/Library"
POLICIES = "/DBASFXPack_Core/Audio/Shared/Policies"
GROUPS = {"Footsteps", "Water", "Ambience", "World", "Combat", "Creatures", "UI", "Spells"}

def read_sources():
    rows = json.loads(MANIFEST.read_text(encoding="utf-8"))
    entries = rows.get("entries", [])
    if rows.get("schemaVersion") != 1 or len(entries) != 32 or rows.get("mount") != MOUNT:
        raise ValueError("新声库清单结构/挂载点/数量不符")
    unique = set()
    for item in entries:
        if item["category"] not in GROUPS or item["playbackSpace"] not in ("World", "TwoD"):
            raise ValueError("声音分类/空间错误：" + item["filename"])
        if item["license"] != "CC0-1.0" or not item["sourcePage"].startswith("https://opengameart.org/"):
            raise ValueError("音频来源许可错误：" + item["filename"])
        source = SOURCE / item["category"] / item["filename"]
        if not source.is_file() or not source.resolve().is_relative_to(SOURCE.resolve()):
            raise FileNotFoundError(str(source))
        if hashlib.sha256(source.read_bytes()).hexdigest() != item["sha256"] or source.stat().st_size != item["bytes"]:
            raise RuntimeError("声音来源指纹不一致：" + item["filename"])
        if item["logicalId"] in unique:
            raise RuntimeError("重复逻辑身份：" + item["logicalId"])
        unique.add(item["logicalId"])
    return entries

def author(entries):
    import unreal
    lib = unreal.EditorAssetLibrary
    # PythonScript命令进程可能尚未扫描第三层纯内容包；先执行引擎注册表同步扫描。
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/DBASFXPack_Core"], True)
    print("SSLM_AUDIO_LIBRARY_REGISTRY_SCANNED", "/DBASFXPack_Core")
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    cls = unreal.GamePlatformSFXDefinition
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)

    def get(path, kind):
        asset = lib.load_asset(path)
        if not isinstance(asset, kind):
            raise RuntimeError("原生音频资源缺失或类型不对：" + path)
        return asset

    spell_att = get(POLICIES + "/S_DBA_SpellShort_Attenuation", unreal.SoundAttenuation)
    ui_con = get(POLICIES + "/S_DBA_UI_Click_Concurrency", unreal.SoundConcurrency)
    spell_con = get(POLICIES + "/S_DBA_Spell_Short_Concurrency", unreal.SoundConcurrency)

    def ensure_policy(name, kind, factory_cls, count=0, distance=None):
        path = POLICIES + "/" + name
        if lib.does_asset_exist(path):
            return get(path, kind)
        obj = tools.create_asset(name, POLICIES, kind, factory_cls())
        if not isinstance(obj, kind):
            raise RuntimeError("策略创建失败：" + name)
        if count:
            settings = obj.get_editor_property("concurrency")
            settings.set_editor_property("max_count", count)
            obj.set_editor_property("concurrency", settings)
        if distance:
            settings = obj.get_editor_property("attenuation")
            settings.set_editor_property("attenuate", True)
            settings.set_editor_property("spatialize", True)
            settings.set_editor_property("attenuation_shape_extents", unreal.Vector(distance[0], 0, 0))
            settings.set_editor_property("falloff_distance", distance[1])
            obj.set_editor_property("attenuation", settings)
        if not lib.save_loaded_asset(obj, only_if_is_dirty=False):
            raise RuntimeError("策略保存失败：" + name)
        print("SSLM_AUDIO_LIBRARY_POLICY_OK", name)
        return obj

    foot_con = ensure_policy("S_DBA_AudioLibrary_Footstep_Concurrency",
        unreal.SoundConcurrency, unreal.SoundConcurrencyFactory, count=8)
    creature_con = ensure_policy("S_DBA_AudioLibrary_Creature_Concurrency",
        unreal.SoundConcurrency, unreal.SoundConcurrencyFactory, count=8)
    ambient_con = ensure_policy("S_DBA_AudioLibrary_Ambience_Concurrency",
        unreal.SoundConcurrency, unreal.SoundConcurrencyFactory, count=6)
    ambient_att = ensure_policy("S_DBA_AudioLibrary_Ambient_Attenuation",
        unreal.SoundAttenuation, unreal.SoundAttenuationFactory, distance=(1200.0, 6000.0))

    for i, item in enumerate(entries):
        group, name = item["category"], item["id"]
        sound_name = Path(item["filename"]).stem
        def_name = "DA_DBA_SFX_" + group + "_" + name
        wave_dir = MOUNT + "/" + group + "/Waves"
        def_dir = MOUNT + "/" + group + "/Definitions"
        wave_path = wave_dir + "/" + sound_name
        def_path = def_dir + "/" + def_name
        for path in (wave_dir, def_dir):
            if not lib.does_directory_exist(path):
                lib.make_directory(path)
        if lib.does_asset_exist(wave_path):
            sound = get(wave_path, unreal.SoundWave)
        else:
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", str(SOURCE / group / item["filename"]))
            task.set_editor_property("destination_path", wave_dir)
            task.set_editor_property("destination_name", sound_name)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", False)
            task.set_editor_property("save", False)
            tools.import_asset_tasks([task])
            sound = get(wave_path, unreal.SoundWave)
        if bool(sound.get_editor_property("looping")) != bool(item["looping"]):
            sound.set_editor_property("looping", bool(item["looping"]))
        if not lib.save_loaded_asset(sound, only_if_is_dirty=False):
            raise RuntimeError("SoundWave保存失败：" + sound_name)
        if lib.does_asset_exist(def_path):
            definition = get(def_path, cls)
        else:
            definition = tools.create_asset(def_name, def_dir, cls, factory)
            if not isinstance(definition, cls):
                raise RuntimeError("音效定义创建失败：" + def_name)
        logical, version = item["logicalId"].rsplit("@", 1)
        ns, ident_name = logical.rsplit(".", 1)
        ident = unreal.GamePlatformId()
        ident.set_editor_property("namespace", ns)
        ident.set_editor_property("name", ident_name)
        ident.set_editor_property("logical_version", int(version))
        definition.set_editor_property("logical_id", ident)
        definition.set_editor_property("sound", sound)
        definition.set_editor_property("playback_space",
            unreal.GamePlatformSFXPlaybackSpace.WORLD if item["playbackSpace"] == "World"
            else unreal.GamePlatformSFXPlaybackSpace.TWO_D)
        definition.set_editor_property("volume_multiplier", float(item["volumeMultiplier"]))
        concurrency = (ui_con if group == "UI" else
                       foot_con if group == "Footsteps" else
                       creature_con if group == "Creatures" else
                       ambient_con if group == "Ambience" else spell_con)
        definition.set_editor_property("concurrency", concurrency)
        if item["playbackSpace"] == "World":
            definition.set_editor_property("attenuation", ambient_att if group == "Ambience" else spell_att)
        if item["looping"]:
            definition.set_editor_property("fade_in_seconds", 1.0)
            definition.set_editor_property("fade_out_seconds", 1.5)
        if not lib.save_loaded_asset(definition, only_if_is_dirty=False):
            raise RuntimeError("音效定义保存失败：" + def_name)
        if not isinstance(lib.load_asset(def_path), cls):
            raise RuntimeError("定义资产回读失败：" + def_path)
        print("SSLM_AUDIO_LIBRARY_ASSET_OK", group, name, str(i + 1))
    print("SSLM_AUDIO_LIBRARY_IMPORTED_OK", len(entries))

if __name__ == "__main__":
    entries = read_sources()
    for entry in entries:
        print("SSLM_AUDIO_LIBRARY_SHA_OK", entry["filename"], entry["sha256"])
    if MODE == "inspect":
        print("SSLM_AUDIO_LIBRARY_INSPECT_OK", len(entries))
    elif MODE == "apply":
        author(entries)
    else:
        raise ValueError("SSLM_AUDIO_LIBRARY_MODE仅允许inspect或apply")
