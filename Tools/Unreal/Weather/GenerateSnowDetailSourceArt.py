#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""跨游戏真实积雪增强的原创源素材生成器（只生成 PNG 与 SHA-256 清单）。

所属：Tools/Unreal/Weather 工程资源制作工具。
消费方：GamePlatformSurface 的雪覆盖/微高度、GamePlatformVFX 的地表风吹雪粉。
要求：NumPy、Pillow；普通 Python 可执行，不创建或覆盖 UE 引擎资产。
安全：禁止覆盖与确定性生成结果不一致的现存文件；--verify 只读复核。
所有高度仅为材质视觉微起伏，不作为碰撞或服务器权威地形高度。
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path

import numpy as np
from PIL import Image

from GenerateWeatherSourceTextures import periodic_noise

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = Path(__file__).with_name("SnowDetailSourceManifest.json")
SEED = 20261011
SURFACE = ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/SourceArt/Snow"
VFX = ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/Snow"


def snow_microheight(rng: np.random.Generator) -> Image.Image:
    """周期多尺度雪粒起伏；0..255对应视觉微高度，不代表厘米位移。"""
    n = 2048
    broad = periodic_noise(n, rng, (8, 16, 32, 64, 128, 256))
    fine = periodic_noise(n, rng, (32, 64, 128, 256, 512, 1024))
    field = (0.72 * broad + 0.28 * fine)
    data = np.uint8(np.rint(np.clip((field - 0.20) / 0.58, 0, 1) * 255))
    return Image.fromarray(data, mode="L")


def snow_coverage(rng: np.random.Generator) -> Image.Image:
    """周期宏观遮罩，降低网格重复感并打散雪/土的边缘。"""
    n = 2048
    field = periodic_noise(n, rng, (2, 4, 8, 16, 32, 64))
    data = np.uint8(np.rint(np.clip((field - 0.10) / 0.78, 0, 1) * 255))
    return Image.fromarray(data, mode="L")


def ground_puff() -> Image.Image:
    """地表雪粉 Sprite：椭圆高斯消散与流线细节，允许 GPU sprite alpha 淡出。"""
    w, h = 512, 512
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    u = (xx + 0.5 - w * 0.5) / (w * 0.5)
    v = (yy + 0.5 - h * 0.5) / (h * 0.5)
    # 垂直压扁以模拟贴地雪粉云，边缘为透明且不能产生矩形边界。
    core = np.exp(-3.4 * (u * u + 6.2 * v * v))
    trail = np.exp(-7.1 * ((u - .24) ** 2 + 9.5 * (v + .025 * u) ** 2))
    detail = 0.93 + 0.07 * np.sin(u * 31 + v * 19) * np.cos(v * 45 - u * 11)
    alpha = np.uint8(np.rint(np.clip((0.65 * core + 0.23 * trail) * detail, 0, 1) * 200))
    rgb = np.empty((h, w, 4), dtype=np.uint8)
    rgb[..., 0] = 224
    rgb[..., 1] = 238
    rgb[..., 2] = 247
    rgb[..., 3] = alpha
    return Image.fromarray(rgb, mode="RGBA")


def expected_art() -> list[tuple[Path, Image.Image, str, bool, str]]:
    """按确定的顺序调用同一 RNG，保证重复生成获得完全相同字节。"""
    rng = np.random.default_rng(SEED)
    return [
        (SURFACE / "T_GP_Snow_MicroHeight_Source.png", snow_microheight(rng),
         "2048雪面微高度与光学遮罩", False, "Masks"),
        (SURFACE / "T_GP_Snow_CoverageNoise_Source.png", snow_coverage(rng),
         "2048世界空间积雪覆盖噪声", False, "Masks"),
        (VFX / "T_GP_Snow_GroundPuff_Source.png", ground_puff(),
         "512贴地风吹雪粉半透明Sprite", True, "Default"),
    ]


def main() -> None:
    parser = argparse.ArgumentParser(description="积雪源素材生成与完整性核验")
    parser.add_argument("--verify", action="store_true", help="只校验现有文件与清单，不修改任何文件")
    options = parser.parse_args()

    planned = []
    content = []
    for target, image, usage, srgb, compression in expected_art():
        output = io.BytesIO()
        image.save(output, "PNG", optimize=False, compress_level=6)
        raw = output.getvalue()
        sha = hashlib.sha256(raw).hexdigest()
        entry = {
            "sourcePath": target.relative_to(ROOT).as_posix(),
            "assetPath": ("/GamePlatformSurface/Textures/Snow/" if target.parent == SURFACE
                          else "/GamePlatformVFX/Weather/Textures/") +
                         target.stem.removesuffix("_Source"),
            "usageZh": usage,
            "sha256": sha,
            "size": list(image.size),
            "mode": image.mode,
            "sourceSRGB": srgb,
            "recommendedCompression": compression,
            "originalArt": "procedural-workspace-original",
            "engineAssetCreated": False,
        }
        planned.append(entry)
        content.append((target, raw))

    manifest = {
        "schemaVersion": 1,
        "generator": "Tools/Unreal/Weather/GenerateSnowDetailSourceArt.py",
        "seed": SEED,
        "noteZh": "仅为原始PNG；Texture2D、材质和Niagara需通过真实UE5.8创建、回读和编译",
        "textures": planned,
    }
    encoded = (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode("utf-8")

    # 所有目标先完成预检，避免半覆盖已有文件；非生成器所有权的资产绝不修改。
    for target, raw in [*content, (MANIFEST, encoded)]:
        if options.verify and not target.is_file():
            raise FileNotFoundError("源素材或清单缺失：" + str(target))
        if target.exists() and target.read_bytes() != raw:
            raise RuntimeError("现存文件与固定种子/清单不符，拒绝覆盖：" + str(target))
    if not options.verify:
        for target, raw in [*content, (MANIFEST, encoded)]:
            if not target.exists():
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(raw)
                print("SNOW_SOURCE_CREATED", target.resolve().relative_to(ROOT).as_posix())
    for entry in planned:
        print("SNOW_SOURCE_VERIFIED", entry["assetPath"], entry["sha256"], entry["size"])
    print("SNOW_SOURCE_CHECK_OK count=3 mode=" + ("verify" if options.verify else "generate"))


if __name__ == "__main__":
    main()
