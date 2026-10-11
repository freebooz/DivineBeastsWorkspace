#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""第三层Village共享中性地形母材质所需极小默认纹理；只生成PNG，不生成.uasset。

共享地图禁止硬引用GamePlatformSurface纯客户端纹理，因此给Landscape母材质放入三张
WorldPack自有中性默认贴图；客户端再通过GamePlatformData软加载高分辨率真实雪贴图替换MID参数。
默认贴图随共享地图服务器Cook，但不含平台纯表现插件依赖；它们属于最低限度的基础地形渲染资源。
--verify只读；不同内容绝不覆盖，固定生成并记录SHA256，异常退出不谎报成功。
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/LandscapeBridge"
MANIFEST = Path(__file__).with_name("VillageLandscapeBridgeSourceManifest.json")

FILES = (
    ("T_DBA_Village_SnowCoverageDefault_Source.png", "L", (64, 64), 135,
     False, "Masks", "用于服务器共享地形母材质的微型默认雪覆盖遮罩"),
    ("T_DBA_Village_SnowAlbedoDefault_Source.png", "RGB", (16, 16), (220, 225, 230),
     True, "Default", "共享地形母材质默认雪色；客户端异步替换为平台高清雪色"),
    ("T_DBA_Village_SnowNormalDefault_Source.png", "RGB", (16, 16), (128, 128, 255),
     False, "Normalmap", "默认平面雪法线；客户端异步替换为平台雪法线"),
)

def main() -> None:
    parser = argparse.ArgumentParser(description="可重现的新手村共享地形中性占位纹理")
    parser.add_argument("--verify", action="store_true")
    options = parser.parse_args()
    inputs = []
    manifest = []
    for filename, mode, dims, fill, srgb, compression, desc in FILES:
        im = Image.new(mode, dims, fill)
        tmp = io.BytesIO()
        im.save(tmp, format="PNG", compress_level=6)
        raw = tmp.getvalue()
        path = OUTPUT / filename
        manifest.append({
            "sourcePath": path.relative_to(ROOT).as_posix(),
            "assetPath": "/DBAWorldPack_Village/Materials/Landscape/Defaults/" +
                         path.stem.removesuffix("_Source"),
            "size": list(dims), "mode": mode, "sourceSRGB": srgb,
            "recommendedCompression": compression,
            "sha256": hashlib.sha256(raw).hexdigest(),
            "descriptionZh": desc,
            "serverBaselineSafe": True
        })
        inputs.append((path, raw))
    doc = {"schemaVersion": 1, "generator": "Tools/Unreal/Weather/GenerateVillageLandscapeBridgeSources.py",
           "notesZh": "共享WorldPack自有的小尺寸基础纹理，不含任何平台纯客户端依赖",
           "textures": manifest}
    outputs = [*inputs, (MANIFEST, (json.dumps(doc, indent=2, ensure_ascii=False)+"\n").encode("utf-8"))]
    for path, raw in outputs:
        if options.verify and not path.is_file():
            raise FileNotFoundError("验证模式下不存在：" + str(path))
        if path.exists() and path.read_bytes() != raw:
            raise RuntimeError("已存在不同的用户内容，禁止覆盖：" + str(path))
    if not options.verify:
        for path, raw in outputs:
            if not path.exists():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(raw)
                print("VILLAGE_BRIDGE_SOURCE_CREATED", path.relative_to(ROOT))
    for item in manifest:
        print("VILLAGE_BRIDGE_SOURCE_VALIDATED", item["assetPath"], item["sha256"])
    print("VILLAGE_BRIDGE_SOURCE_QA_PASS count=3")


if __name__ == "__main__":
    main()
