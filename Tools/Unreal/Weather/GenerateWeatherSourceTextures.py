#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""为游戏平台Weather技术美术生成确定性源纹理；只生成PNG，不伪造UE .uasset。

所属层：Tools（工程工具），消费方：GamePlatformVFX / GamePlatformSurface。
运行期：人工显式执行，禁止覆盖已有的人工美术修改；--verify只读验证。
资产入UE、色彩空间、Compression及Shader必须由UE编辑器独立验证。
依赖：Python 3、NumPy、Pillow；随机种子固定，输出字节/清单可复核。
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import math
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[3]
VFX = ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/Weather"
SURFACE = ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/SourceArt/Weather"
SEED = 20261010


@dataclass(frozen=True)
class TextureSource:
    file: str
    domain: str
    usage: str
    srgb: bool
    compression: str
    alpha: bool


SOURCES = (
    TextureSource("T_GP_Weather_RainStreak_Source.png", "VFX", "雨线Sprite透明度", True, "Default", True),
    TextureSource("T_GP_Weather_RainRipple_Source.png", "VFX", "局部水面环纹透明度", True, "Default", True),
    TextureSource("T_GP_Weather_RainSplashFlipbook_Source.png", "VFX", "4x4水花序列帧，按行依次播放", True, "Default", True),
    TextureSource("T_GP_Weather_SnowflakeAtlas_Source.png", "VFX", "4x4形态不同的雪花图集", True, "Default", True),
    TextureSource("T_GP_Weather_WetnessNoise_Source.png", "Surface", "无缝湿润宏观噪声", False, "Masks", False),
    TextureSource("T_GP_Weather_PuddleMask_Source.png", "Surface", "无缝积水辅助遮罩，不能独自替代地形坡度/高度规则", False, "Masks", False),
    TextureSource("T_GP_Weather_SnowAlbedo_Source.png", "Surface", "雪色PBR基础颜色源贴图", True, "Default", False),
    TextureSource("T_GP_Weather_SnowNormal_Source.png", "Surface", "雪面切线空间Normal源贴图", False, "Normalmap", False),
    TextureSource("T_GP_Weather_SnowORM_Source.png", "Surface", "R=AO,G=Roughness,B=Metallic线性打包", False, "Masks", False),
)


def periodic_noise(n: int, rng: np.random.Generator, octaves=(4, 8, 16, 32, 64, 128)) -> np.ndarray:
    """周期双线性插值与五次平滑，靠取模得到水平/垂直无缝纹理。"""
    ax = np.arange(n, dtype=np.float32)
    result = np.zeros((n, n), dtype=np.float32)
    weights = (0.35, 0.27, 0.18, 0.11, 0.06, 0.03)
    for cells, weight in zip(octaves, weights):
        values = rng.random((cells, cells), dtype=np.float32)
        sample = ax * cells / n
        xi = np.floor(sample).astype(np.int32)
        f = sample - xi
        f = f * f * f * (f * (f * 6 - 15) + 10)
        x0, x1 = xi % cells, (xi + 1) % cells
        y0, y1 = x0, x1
        fx, fy = f[None, :], f[:, None]
        a = values[y0[:, None], x0[None, :]]
        b = values[y0[:, None], x1[None, :]]
        c = values[y1[:, None], x0[None, :]]
        d = values[y1[:, None], x1[None, :]]
        result += weight * ((a * (1 - fx) + b * fx) * (1 - fy) +
                            (c * (1 - fx) + d * fx) * fy)
    return result


def to_luma(noise: np.ndarray, dark=0.05, light=0.95) -> np.ndarray:
    norm = np.clip((noise - 0.20) / 0.60, 0, 1)
    return np.uint8(np.round(np.clip(dark + (light - dark) * norm, 0, 1) * 255))


def rain_streak() -> Image.Image:
    """用于Sprite的单条雨线；亮度集中在中央，并沿雨线首尾渐隐。"""
    w, h = 512, 1024
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    t = y / (h - 1)
    center = 256 + (0.5 - t) * 42
    width = 1.7 + 3.5 * np.sin(np.pi * t) ** 2
    core = np.exp(-0.5 * ((x - center) / width) ** 2)
    halo = np.exp(-0.5 * ((x - center) / (width * 4.5)) ** 2)
    fade = np.maximum(0, np.sin(np.pi * t)) ** 1.8
    a = np.clip((0.66 * core + 0.14 * halo) * fade, 0, 1)
    rgba = np.stack((np.full_like(a, 218), np.full_like(a, 237),
                     np.full_like(a, 250), a * 255), axis=-1)
    return Image.fromarray(np.uint8(np.round(rgba)))


def ripple() -> Image.Image:
    n = 512
    yy, xx = np.mgrid[-1:1:n*1j, -1:1:n*1j]
    r = np.sqrt(xx * xx + yy * yy)
    theta = np.arctan2(yy, xx)
    modulation = 1.0 + 0.02 * np.sin(8 * theta) + 0.015 * np.cos(11 * theta)
    ring = np.exp(-0.5 * ((r - 0.58 * modulation) / 0.018) ** 2)
    inner = 0.21 * np.exp(-0.5 * ((r - 0.37) / 0.026) ** 2)
    alpha = np.clip((ring + inner) * (1 - np.clip((r - 0.90) * 10, 0, 1)), 0, 1)
    return Image.fromarray(np.uint8(np.stack((
        np.full_like(alpha, 207), np.full_like(alpha, 229),
        np.full_like(alpha, 242), alpha * 255), axis=-1)))


def splash_flipbook() -> Image.Image:
    """16帧，先出冲击中心，再水环扩大衰减；不依赖引擎之外的粒子碰撞。"""
    full = Image.new("RGBA", (1024, 1024))
    rng = np.random.default_rng(SEED + 17)
    for frame in range(16):
        tile = Image.new("RGBA", (256, 256))
        draw = ImageDraw.Draw(tile, "RGBA")
        t = frame / 15
        cx, cy = 128, 149
        radius = 13 + 83 * (t ** 0.65)
        opacity = int(220 * (1 - t) ** 1.5)
        box = (cx - radius, cy - radius * .38, cx + radius, cy + radius * .38)
        if opacity > 0:
            draw.arc(box, 3, 175, fill=(225, 238, 248, opacity), width=max(1, round(4 * (1 - t) + 1)))
            draw.arc(box, 186, 353, fill=(225, 238, 248, round(opacity * .6)), width=max(1, round(3 * (1 - t) + 1)))
        if 0 < frame < 12:
            for droplet in range(7):
                angle = (droplet / 7 + .08 * rng.random()) * math.tau
                rr = (24 + 110 * t) * (0.8 + .3 * rng.random())
                dx = cx + rr * math.cos(angle)
                dy = cy - 20 - 60 * t - 18 * math.sin(angle)
                size = 1.5 + (1 - t) * 3
                draw.ellipse((dx - size, dy - size, dx + size, dy + size),
                             fill=(226, 241, 250, int(opacity * 0.5)))
        glow = tile.filter(ImageFilter.GaussianBlur(2.0))
        tile = Image.alpha_composite(glow, tile)
        full.alpha_composite(tile, dest=((frame % 4) * 256, (frame // 4) * 256))
    return full


def snowflake_atlas() -> Image.Image:
    """16种六重对称晶体图样，柔边RGBA。每一格可独立采样，不拼成四季场景插画。"""
    full = Image.new("RGBA", (1024, 1024))
    rng = np.random.default_rng(SEED + 23)
    for idx in range(16):
        tile = Image.new("RGBA", (256, 256))
        d = ImageDraw.Draw(tile, "RGBA")
        cx, cy = 128, 128
        radius = float(rng.uniform(55, 103))
        branches = int(rng.integers(2, 5))
        phase = float(rng.uniform(0, 0.3))
        strength = int(rng.integers(155, 225))
        d.ellipse((cx - 4, cy - 4, cx + 4, cy + 4), fill=(243, 249, 255, strength))
        for arm in range(6):
            a = phase + arm * math.tau / 6
            tip = (cx + radius * math.cos(a), cy + radius * math.sin(a))
            d.line(((cx, cy), tip), fill=(244, 249, 255, strength), width=int(rng.integers(2, 5)))
            for node in range(1, branches + 1):
                t = 0.28 + .58 * node / (branches + 1)
                px, py = cx + radius * t * math.cos(a), cy + radius * t * math.sin(a)
                branch_len = radius * (0.16 + 0.07 * (1 - t))
                for side in (-1, 1):
                    b = a + side * math.pi / 3
                    d.line(((px, py),
                            (px - branch_len * math.cos(b),
                             py - branch_len * math.sin(b))),
                           fill=(245, 250, 255, int(strength * 0.78)), width=2)
        soft = tile.filter(ImageFilter.GaussianBlur(1.4))
        full.alpha_composite(Image.alpha_composite(soft, tile),
                             dest=(idx % 4 * 256, idx // 4 * 256))
    return full


def surface_images() -> dict[str, Image.Image]:
    rng = np.random.default_rng(SEED)
    noise = periodic_noise(1024, rng)
    fine = periodic_noise(1024, rng, octaves=(16, 32, 64, 128, 256, 512))
    wet = Image.fromarray(to_luma(noise, .03, .97))
    puddle = np.clip((noise - 0.50) * 8 + (fine - .5) * 0.8, 0, 1)
    puddle = Image.fromarray(np.uint8(np.round(puddle * 255)))

    # 纹理微结构只给雪本身，地面积雪的空间坡度/高度/温度由材质函数负责。
    grain = np.clip(0.56 * noise + 0.44 * fine, 0, 1)
    s = np.uint8(np.clip(np.round(233 + (grain - .5) * 26), 0, 255))
    rgb = np.stack((np.clip(s.astype(np.int16) - 2, 0, 255),
                    s.astype(np.int16),
                    np.clip(s.astype(np.int16) + 3, 0, 255)), -1).astype(np.uint8)
    albedo = Image.fromarray(rgb)
    height = fine * 0.84 + noise * 0.16
    gx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 3.1
    gy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 3.1
    normal = np.stack((-gx, -gy, np.ones_like(height)), axis=-1)
    normal = normal / np.maximum(np.linalg.norm(normal, axis=-1, keepdims=True), 1e-6)
    normal = Image.fromarray(np.uint8(np.round(np.clip((normal + 1) / 2 * 255, 0, 255))))
    rough = np.uint8(np.round(np.clip(0.83 + (grain - .5) * .12, 0, 1) * 255))
    orm = Image.fromarray(np.stack((np.full_like(rough, 248), rough,
                                   np.zeros_like(rough)), axis=-1))
    return {
        "T_GP_Weather_WetnessNoise_Source.png": wet,
        "T_GP_Weather_PuddleMask_Source.png": puddle,
        "T_GP_Weather_SnowAlbedo_Source.png": albedo,
        "T_GP_Weather_SnowNormal_Source.png": normal,
        "T_GP_Weather_SnowORM_Source.png": orm,
    }


def build() -> dict[str, Image.Image]:
    generated = {
        "T_GP_Weather_RainStreak_Source.png": rain_streak(),
        "T_GP_Weather_RainRipple_Source.png": ripple(),
        "T_GP_Weather_RainSplashFlipbook_Source.png": splash_flipbook(),
        "T_GP_Weather_SnowflakeAtlas_Source.png": snowflake_atlas(),
    }
    generated.update(surface_images())
    return generated


def execute(verify: bool) -> None:
    generated = build()
    entries = []
    expected_names = {s.file for s in SOURCES}
    assert set(generated) == expected_names, "纹理清单与生成器不一致"
    for src in SOURCES:
        image = generated[src.file]
        outdir = VFX if src.domain == "VFX" else SURFACE
        outpath = outdir / src.file
        data = io.BytesIO()
        image.save(data, "PNG", compress_level=9, optimize=False)
        blob = data.getvalue()
        digest = hashlib.sha256(blob).hexdigest()
        if verify:
            if not outpath.exists() or hashlib.sha256(outpath.read_bytes()).hexdigest() != digest:
                raise RuntimeError(f"资源缺失或人工修改，校验失败：{outpath}")
        else:
            if outpath.exists():
                if hashlib.sha256(outpath.read_bytes()).hexdigest() != digest:
                    raise FileExistsError(f"已有不同内容，拒绝覆盖：{outpath}")
            else:
                outdir.mkdir(parents=True, exist_ok=True)
                outpath.write_bytes(blob)
        if src.alpha:
            aa = np.asarray(image.getchannel("A"))
            if not (aa.min() == 0 and aa.max() > 90):
                raise AssertionError(f"Alpha不合规：{src.file}")
        if src.file.endswith("SnowNormal_Source.png"):
            zz = np.asarray(image)[:, :, 2].mean()
            if zz < 230:
                raise AssertionError("切线法线Z不符合近竖直朝向预期")
        entries.append({
            "sourcePath": outpath.relative_to(ROOT).as_posix(),
            "owner": src.domain,
            "use": src.usage,
            "sha256": digest,
            "size": list(image.size),
            "mode": image.mode,
            "sourceSRGB": src.srgb,
            "recommendedCompression": src.compression,
            "originalArt": "procedural-self-generated",
            "license": "workspace-original",
            "engineAssetCreated": False,
        })
        print(f"{'VERIFY' if verify else 'SOURCE'} {src.file} {image.width}x{image.height} {image.mode} sha256={digest[:16]}")
    manifest = {
        "schemaVersion": 1,
        "generator": "Tools/Unreal/Weather/GenerateWeatherSourceTextures.py",
        "seed": SEED,
        "notes": "仅为可重复的天气源纹理，必须经UE编辑器导入/设置材质/编译才能成为可运行资源；不声称生成Niagara或.uasset。",
        "textures": entries,
    }
    manifest_path = Path(__file__).with_name("WeatherSourceArtManifest.json")
    payload = json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"
    if verify:
        if not manifest_path.exists() or manifest_path.read_text(encoding="utf-8") != payload:
            raise AssertionError("资源清单不一致，人工美术变更必须显式重新审核版本")
    else:
        if manifest_path.exists() and manifest_path.read_text(encoding="utf-8") != payload:
            raise FileExistsError(f"资源清单已有不同内容，拒绝覆盖：{manifest_path}")
        manifest_path.write_text(payload, encoding="utf-8")
    print("PASS" if verify else "GENERATED", len(entries))


def main() -> None:
    parser = argparse.ArgumentParser(description="天气P0源纹理生成与SHA256验证（中文说明见脚本头）")
    parser.add_argument("--verify", action="store_true", help="仅验证，不写文件")
    args = parser.parse_args()
    execute(verify=args.verify)


if __name__ == "__main__":
    main()
