#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Monolith Editor 实际导入《神兽联盟》主题源PNG为真正UTexture2D。

必须由 Monolith MCP editor.run_python(mode="execute_file") 运行。
禁止使用外部脚本独自生成或修改.uasset；此文件只是引擎操作指令。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.get_project_file_path()).resolve().parents[1]
SOURCE = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/SourceArt/UI/Theme"
MOUNT = "/DBAUIPack_Core/UI/Textures/Theme"
MANIFEST = SOURCE / "UIThemeSourceArtManifest.json"

def main():
    if not MANIFEST.is_file():
        print("ERROR: cannot find original UI theme art manifest")
        return
    spec = json.loads(MANIFEST.read_text(encoding="utf-8"))
    tasks = []
    checked = []
    for item in spec["Files"]:
        path = ROOT / item["Source"]
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != item["SHA256"]:
            print("ERROR SOURCE OR HASH",str(path))
            return
        dest = item["Destination"]
        if not dest.startswith(MOUNT+"/"):
            print("ERROR DESTINATION IS OUTSIDE DBAUIPACK",dest)
            return
        checked.append((dest,item))
        if unreal.EditorAssetLibrary.does_asset_exist(dest):
            print("ALREADY_IMPORTED",dest)
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename",str(path))
        task.set_editor_property("destination_path",MOUNT)
        task.set_editor_property("destination_name",dest.rsplit("/",1)[-1])
        task.set_editor_property("automated",True)
        task.set_editor_property("replace_existing",False)
        task.set_editor_property("save",True)
        tasks.append(task)
    print("SOURCE_PNG_VERIFIED",len(checked))
    print("NEED_TEXTURE_IMPORT",len(tasks))
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    verified = 0
    for dest,item in checked:
        texture = unreal.EditorAssetLibrary.load_asset(dest)
        if texture is None or texture.get_class().get_name() != "Texture2D":
            print("ERROR_TEXTURE_MISSING",dest)
            continue
        verified += 1
        try:
            texture.set_editor_property("lod_group",unreal.TextureGroup.TEXTUREGROUP_UI)
            texture.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            unreal.EditorAssetLibrary.save_asset(dest,only_if_is_dirty=False)
        except Exception as exc:
            print("TEXTURE_SETTING_WARNING",dest,str(exc)[:260])
        print("UE_TEXTURE2D",dest)
    print("THEME_TEXTURE2D_COUNT",verified,"/",len(checked))
    print("AUTOMATION_TESTS=NOT_RUN")

main()
