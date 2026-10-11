#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成跨游戏真实降雪用柔边雪片RGBA Sprite，替代过大星芒形雪花的项目增强版引用。

职责：GamePlatformVFX平台纯SourceArt生产，按固定数学公式生成可复现源PNG和SHA256；
不修改原T_GP_Weather_SnowflakeAtlas或NS_GP_Weather_Snow（它们作为回退资产）。
新图单Sprite、无星形骨架、全透明边缘、低粒度变化，适用于真实降雪/手机及PC渲染。
消费者是独立的MI_GP_VFX_Weather_SnowSoft材质实例与NS_GP_Weather_Snow_Detailed近/远景。
运行：需要NumPy/Pillow的Python，--verify只校验已有文件，拒绝覆盖不同源内容。
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
SOURCE = (ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/"
          "SourceArt/Snow/T_GP_Snow_SoftFlake_Source.png")
MANIFEST = Path(__file__).with_name("SnowSoftFlakeSourceManifest.json")
ASSET = "/GamePlatformVFX/Weather/Textures/T_GP_Snow_SoftFlake"


def generate() -> Image.Image:
    """蓝白柔边颗粒+少量光散射晕，严格保留0 alpha外边界，拒绝星芒与锐线。"""
    size = 512
    y, x = np.mgrid[0:size, 0:size].astype(np.float32)
    u = (x + .5 - size * .5) / (size * .5)
    v = (y + .5 - size * .5) / (size * .5)
    angle = np.arctan2(v, u)
    radius = np.sqrt((u * .95) ** 2 + (v * 1.07) ** 2)
    # 低振幅的轮廓变化，避免数学完美圆，但禁止冰晶星芒和可见像素颗粒。
    soft_radius = radius * (1 + .021 * np.sin(5 * angle) + .014 * np.cos(7 * angle))
    core = np.exp(-2.35 * (soft_radius / .50) ** 2)
    halo = np.exp(-1.9 * (radius / .72) ** 2)
    rim = np.clip((1.0 - radius) / .16, 0, 1)
    falloff = rim * rim * (3.0 - 2.0 * rim)
    opacity = np.clip((.65 * core + .075 * halo) * falloff, 0, .73)
    rgba = np.zeros((size, size, 4), dtype=np.uint8)
    rgba[..., 0], rgba[..., 1], rgba[..., 2] = 232, 243, 250
    rgba[..., 3] = np.uint8(np.rint(opacity * 255))
    # 小像素块完全透明，避免雪花卡片形成半透明边框或纹理接缝。
    rgba[:2, :, 3] = 0
    rgba[-2:, :, 3] = 0
    rgba[:, :2, 3] = 0
    rgba[:, -2:, 3] = 0
    return Image.fromarray(rgba, mode="RGBA")


def main() -> None:
    parser = argparse.ArgumentParser(description="稳定生产柔边雪粒贴图、SHA256并校验Alpha")
    parser.add_argument("--verify", action="store_true")
    opts = parser.parse_args()
    im = generate()
    buf = io.BytesIO()
    im.save(buf, format="PNG", optimize=False, compress_level=6)
    payload = buf.getvalue()
    digest = hashlib.sha256(payload).hexdigest()
    meta = {
        "schemaVersion": 1,
        "generator": "Tools/Unreal/Weather/GenerateSnowSoftFlake.py",
        "sourcePath": SOURCE.relative_to(ROOT).as_posix(),
        "textureAsset": ASSET,
        "textureSize": [512, 512],
        "mode": "RGBA",
        "sha256": digest,
        "sourceSRGB": True,
        "descriptionZh": "平台通用自然柔边小雪粒，替代星芒卡片，不更改原雪晶Atlas",
        "createdUEAsset": False,
    }
    manifest_bytes = (json.dumps(meta, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    for path, raw in ((SOURCE, payload), (MANIFEST, manifest_bytes)):
        if opts.verify and not path.is_file():
            raise FileNotFoundError("源文件或清单不存在：" + str(path))
        if path.exists() and path.read_bytes() != raw:
            raise RuntimeError("发现不同素材，禁止覆盖：" + str(path))
    if not opts.verify:
        for path, raw in ((SOURCE, payload), (MANIFEST, manifest_bytes)):
            if not path.exists():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(raw)
                print("SNOW_SOFT_SOURCE_CREATED", path.resolve().relative_to(ROOT).as_posix())
    # 原图不存在的锐边/星芒由低频径向连续Alpha限定；按边框/中心像素检测。
    a = np.asarray(im)[..., 3]
    if (np.any(a[[0, -1], :]) or np.any(a[:, [0, -1]]) or
            a[256, 256] < 145 or len(payload) < 12000):
        raise RuntimeError("雪花透明边界、中心透明度或实际PNG大小不满足合同")
    print("SNOW_SOFT_SOURCE_QA_PASS", ASSET, digest, im.size, "max_alpha", int(a.max()))


if __name__ == "__main__":
    main()
