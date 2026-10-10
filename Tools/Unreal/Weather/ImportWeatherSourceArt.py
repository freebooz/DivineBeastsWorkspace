#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""仅在锁定UE5.8编辑器的PythonScriptPlugin中运行。

职责：校验已生成天气SourceArt清单并真实导入Texture2D资产。
作用域：GamePlatformVFX与GamePlatformSurface两个平台表现插件；不修改原始资源。
安全：默认只读预检，须显式将环境变量 WEATHER_ASSET_IMPORT_MODE=apply 才导入。
禁止：不存在真实引擎时写空.uasset、重写同名已有资源、在Dedicated Server Cook中使用。
注意：本脚本未获得引擎端实际验收前仅属可执行导入方案，不宣称资产已生成。
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = Path(__file__).with_name("WeatherSourceArtManifest.json")
DESTINATION = {
    "VFX": "/GamePlatformVFX/Weather/Textures",
    "Surface": "/GamePlatformSurface/Textures/Weather",
}
EXPECTED_FORMAT = {"L", "RGB", "RGBA"}
MODE = os.environ.get("WEATHER_ASSET_IMPORT_MODE", "inspect").strip().lower()


def verify_sources() -> list[dict]:
    if not MANIFEST.exists():
        raise RuntimeError(f"找不到源纹理清单：{MANIFEST}")
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if data.get("schemaVersion") != 1 or len(data.get("textures", [])) != 9:
        raise RuntimeError("纹理清单SchemaVersion/记录数不符，禁止盲目导入")
    validated = []
    names = set()
    for entry in data["textures"]:
        rel = entry["sourcePath"]
        safe = (ROOT / rel).resolve()
        if not safe.is_relative_to(ROOT) or not safe.is_file() or safe.suffix.lower() != ".png":
            raise RuntimeError(f"非法或缺失源文件：{rel}")
        if entry["owner"] not in DESTINATION or entry["mode"] not in EXPECTED_FORMAT:
            raise RuntimeError(f"域或格式非法：{rel}")
        digest = hashlib.sha256(safe.read_bytes()).hexdigest()
        if digest != entry["sha256"]:
            raise RuntimeError(f"源文件指纹变化，必须重新人工审查：{rel}")
        name = safe.stem.removesuffix("_Source")
        if not name.isascii() or not name.startswith("T_GP_Weather_"):
            raise RuntimeError(f"导入资源名非法：{name}")
        asset_path = f"{DESTINATION[entry['owner']]}/{name}"
        if asset_path in names:
            raise RuntimeError(f"重复目标Asset路径：{asset_path}")
        names.add(asset_path)
        validated.append({"src": str(safe), "asset_path": asset_path,
                          "name": name, "entry": entry})
    return validated


def apply_ue_assets(items: list[dict]) -> None:
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("只能通过Unreal Editor的PythonScriptPlugin实际执行，禁止直接调用本地Python写.uasset") from exc

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    if not tools:
        raise RuntimeError("Unreal资产工具不可用，终止导入")
    tasks = []
    for item in items:
        if unreal.EditorAssetLibrary.does_asset_exist(item["asset_path"]):
            raise FileExistsError(f"目标引擎纹理已存在，拒绝覆盖：{item['asset_path']}")
        folder = item["asset_path"].rsplit("/", 1)[0]
        if not unreal.EditorAssetLibrary.does_directory_exist(folder):
            unreal.EditorAssetLibrary.make_directory(folder)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", item["src"])
        task.set_editor_property("destination_path", folder)
        task.set_editor_property("destination_name", item["name"])
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", False)
        tasks.append(task)
    tools.import_asset_tasks(tasks)

    settings_enum = {
        "Default": unreal.TextureCompressionSettings.TC_DEFAULT,
        "Masks": unreal.TextureCompressionSettings.TC_MASKS,
        "Normalmap": unreal.TextureCompressionSettings.TC_NORMALMAP,
    }
    for item, task in zip(items, tasks):
        paths = list(task.get_editor_property("imported_object_paths"))
        if not paths:
            raise RuntimeError(f"UE没有返回有效纹理资产：{item['asset_path']}")
        texture = unreal.EditorAssetLibrary.load_asset(item["asset_path"])
        if not texture or not isinstance(texture, unreal.Texture2D):
            raise RuntimeError(f"实际加载资产不是Texture2D：{item['asset_path']}")
        texture.set_editor_property("srgb", bool(item["entry"]["sourceSRGB"]))
        texture.set_editor_property(
            "compression_settings",
            settings_enum[item["entry"]["recommendedCompression"]]
        )
        # UE5.8的unreal.Texture2D Python代理没有post_edit_change方法；
        # set_editor_property后直接走EditorAssetLibrary保存并按类别回读。
        if not unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False):
            raise RuntimeError(f"UE未能保存真实Texture2D：{item['asset_path']}")
        reload_tex = unreal.EditorAssetLibrary.load_asset(item["asset_path"])
        if reload_tex is None or not isinstance(reload_tex, unreal.Texture2D):
            raise RuntimeError(f"保存后回读失败：{item['asset_path']}")
        print(f"UE_ASSET_OK {item['asset_path']}")
    print("UNREAL_WEATHER_TEXTURES_IMPORTED", len(items))


def main() -> None:
    items = verify_sources()
    for item in items:
        print(f"VERIFIED_SOURCE {item['asset_path']} <-- {item['src']}")
    if MODE == "inspect":
        print("PREVIEW_ONLY：通过了9张源PNG完整性验证，没有创建UE资产。")
        return
    if MODE != "apply":
        raise RuntimeError(f"WEATHER_ASSET_IMPORT_MODE只接受inspect或apply，实际为：{MODE}")
    apply_ue_assets(items)


if __name__ == "__main__":
    main()
