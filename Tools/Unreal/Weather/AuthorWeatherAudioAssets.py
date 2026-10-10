#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""真实UE5.8项目天气音频导入与SFX定义制作，禁止放入Village共享Cook目录。

前置：
1. 实际创建并登记带真实.uasset的客户端纯内容包DBASFXPack_Core，并使Editor/Client挂载；
2. GamePlatformSFXClient和GamePlatformData模块加载成功；
3. WeatherAudioSourceManifest.json内三段WAV源文件SHA256已复核。
默认WEATHER_SFX_AUTHOR_MODE=inspect仅报告前置条件；apply才导入SoundWave、
设置Looping并创建两个UGamePlatformSFXDefinition，保存/回读。
MetaSound雨量混合图需要后续在Monolith或音频编辑器真实连线，绝不创建空同名资源。
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = Path(__file__).with_name("WeatherAudioSourceManifest.json")
MODE = os.getenv("WEATHER_SFX_AUTHOR_MODE", "inspect").strip().lower()
ROOT_MOUNT = "/DBASFXPack_Core"
AUDIO_DIR = ROOT_MOUNT + "/Weather/Audio"
DEFINE_DIR = ROOT_MOUNT + "/Weather/Definitions"
PACK_DESCRIPTOR = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBASFXPack_Core/DBASFXPack_Core.uplugin"
PACK_REGISTRY = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/ContentPackRegistry.json"
NAMES = {
    "SW_DBA_Weather_RainHeavy_Loop": (
        "DA_DBA_SFX_Weather_Rain", "presentation.dba.weather.rain@1"),
    "SW_DBA_Weather_Wind_Loop": (
        "DA_DBA_SFX_Weather_Snow", "presentation.dba.weather.snow@1"),
}


def sources() -> list[dict]:
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if data.get("schemaVersion") != 1 or len(data.get("audio", [])) != 3:
        raise ValueError("天气声音Manifest的Schema/条目数不符")
    rows = []
    for entry in data["audio"]:
        target = (ROOT / entry["sourcePath"]).resolve()
        if not target.is_relative_to(ROOT) or not target.is_file():
            raise FileNotFoundError("丢失音频源：" + str(target))
        if hashlib.sha256(target.read_bytes()).hexdigest() != entry["sha256"]:
            raise RuntimeError("音频源指纹变化：" + str(target))
        rows.append({"name": entry["assetName"], "path": str(target),
                     "asset": AUDIO_DIR + "/" + entry["assetName"]})
    return rows


def main() -> None:
    rows = sources()
    for row in rows:
        print("SFX_TARGET", row["asset"], "SOURCE", row["path"])
    for name, logical_id in NAMES.values():
        print("SFX_DEFINITION_TARGET", DEFINE_DIR + "/" + name, logical_id)
    if MODE == "inspect":
        print("WEATHER_SFX_INSPECT_ONLY：无真实插件/挂载时不导入SoundWave和Definition")
        return
    if MODE != "apply":
        raise ValueError("WEATHER_SFX_AUTHOR_MODE只接受inspect或apply")
    if not PACK_DESCRIPTOR.is_file():
        raise RuntimeError("客户端独立声音内容包DBASFXPack_Core尚未创建；禁止在VillageServer的AlwaysCook目录偷放SoundWave")
    registry = json.loads(PACK_REGISTRY.read_text(encoding="utf-8"))
    entry_names = [e["Name"] for e in registry["ContentPacks"]]
    if entry_names.count("DBASFXPack_Core") != 1:
        raise RuntimeError("DBASFXPack_Core尚未正式登记；不得绕过内容包基线")
    descriptor = json.loads(PACK_DESCRIPTOR.read_text(encoding="utf-8"))
    if descriptor.get("CanContainContent") is not True or descriptor.get("Modules"):
        raise ValueError("DBASFXPack_Core必须是允许真实内容的纯内容插件")

    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("UE5.8编辑器未运行，不能伪造SoundWave .uasset") from exc

    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for row in rows:
        if assets.does_asset_exist(row["asset"]):
            raise FileExistsError("已有音源，拒绝覆盖：" + row["asset"])
    for name, _ in NAMES.values():
        if assets.does_asset_exist(DEFINE_DIR + "/" + name):
            raise FileExistsError("已有SFX Definition，拒绝覆盖：" + name)

    for directory in (AUDIO_DIR, DEFINE_DIR):
        if not assets.does_directory_exist(directory):
            assets.make_directory(directory)
    tasks = []
    for row in rows:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", row["path"])
        task.set_editor_property("destination_path", AUDIO_DIR)
        task.set_editor_property("destination_name", row["name"])
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", False)
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    for row, task in zip(rows, tasks):
        imported = task.get_editor_property("imported_object_paths")
        if not imported:
            raise RuntimeError("SoundWave导入未返回有效资源：" + row["name"])
        sound = assets.load_asset(row["asset"])
        if not isinstance(sound, unreal.SoundWave):
            raise TypeError("导入音频非SoundWave：" + row["asset"])
        sound.set_editor_property("looping", True)
        if not assets.save_loaded_asset(sound, only_if_is_dirty=False):
            raise RuntimeError("循环SoundWave保存失败：" + row["name"])
        print("UE_WEATHER_SOUND_SAVED", row["asset"])

    cls = unreal.GamePlatformSFXDefinition
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    for sound_name, (name, logical_id) in NAMES.items():
        sound = assets.load_asset(AUDIO_DIR + "/" + sound_name)
        definition = tools.create_asset(name, DEFINE_DIR, cls, factory)
        if not isinstance(definition, cls):
            raise RuntimeError("天气SFX定义创建失败：" + name)
        base_id, version = logical_id.rsplit("@", 1)
        namespace, identifier = base_id.rsplit(".", 1)
        ident = unreal.GamePlatformId()
        ident.set_editor_property("namespace", namespace)
        ident.set_editor_property("name", identifier)
        ident.set_editor_property("logical_version", int(version))
        definition.set_editor_property("logical_id", ident)
        revision = unreal.GamePlatformDataVersion()
        # 使用C++默认结构版本及修订=1；EditDefaultsOnly不接受实例写入。
        definition.set_editor_property("data_version", revision)
        definition.set_editor_property("sound", sound)
        definition.set_editor_property("allowed_float_parameters", [unreal.Name("WeatherIntensity")])
        definition.set_editor_property("fade_in_seconds", 1.0)
        definition.set_editor_property("fade_out_seconds", 1.5)
        # Audio播放2D，世界退出/重连由GamePlatformSFX的唯一World服务回收。
        definition.set_editor_property(
            "playback_space", unreal.GamePlatformSFXPlaybackSpace.TWO_D)
        if not assets.save_loaded_asset(definition, only_if_is_dirty=False):
            raise RuntimeError("天气SFX定义保存失败：" + name)
        if not isinstance(assets.load_asset(DEFINE_DIR + "/" + name), cls):
            raise RuntimeError("天气SFX定义回读失败：" + name)
        print("UE_WEATHER_SFX_DEFINITION_SAVED", DEFINE_DIR + "/" + name)


if __name__ == "__main__":
    main()
