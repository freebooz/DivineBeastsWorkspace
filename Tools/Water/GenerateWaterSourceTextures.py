#!/usr/bin/env python3
# SSLM水体通用贴图源生成器：制作数学上无缝平铺的法线、泡沫、流向及焦散RGBA原稿。
# 仅为编辑器导入前的图像制作，不产生或修改Unreal *.uasset、材质和地图。
# 输入：--size 256/512/1024/2048；输出：Docs/Implementation/Water/SourceTextures/ 中PNG和通道清单。
# 无--overwrite时绝不覆盖文件，便于保护人工修订版本；依赖开发环境numpy、Pillow。
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image


def periodic_noise(size: int, seed: int, center_frequency: float) -> np.ndarray:
    """生成天然平铺的二维带限随机高程；固定种子，不使用非确定性GPU求解。"""
    rng = np.random.default_rng(seed)
    signal = rng.normal(size=(size, size)).astype(np.float32)
    freq_y = np.fft.fftfreq(size)[:, None] * size
    freq_x = np.fft.rfftfreq(size)[None, :] * size
    distance = np.sqrt(freq_x * freq_x + freq_y * freq_y)
    spectral = np.exp(-0.5 * np.square((distance - center_frequency) / max(2.0, center_frequency * .72)))
    spectral[0, 0] = 0.0
    noise = np.fft.irfft2(np.fft.rfft2(signal) * spectral, s=signal.shape).astype(np.float32)
    deviation = max(float(noise.std()), 1e-6)
    return noise / deviation


def clamp_byte(values: np.ndarray) -> np.ndarray:
    """将浮点通道映射为8bit非线性展示数据；法线、流向、遮罩正式导入必须关闭sRGB。"""
    return np.uint8(np.rint(np.clip(values, 0.0, 1.0) * 255.0))


def rgba(red: np.ndarray, green: np.ndarray, blue: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    """打包同分辨率RGBA通道，便于未来作为UE BC5/BC7压缩素材源。"""
    return np.stack([clamp_byte(x) for x in (red, green, blue, alpha)], axis=-1)


def render_tiles(size: int) -> dict[str, np.ndarray]:
    """制作无缝水纹原稿；不依赖项目特定地图，位移/强度由水材质实例控制。"""
    coarse = periodic_noise(size, 17011, 9.0)
    detail = periodic_noise(size, 17012, 45.0)
    foam_base = periodic_noise(size, 17013, 18.0)
    foam_detail = periodic_noise(size, 17014, 88.0)
    flow = periodic_noise(size, 17015, 16.0)
    caustic_warp_x = periodic_noise(size, 17016, 5.0)
    caustic_warp_y = periodic_noise(size, 17017, 6.0)

    height = 0.74 * coarse + 0.26 * detail
    normal_d_x = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * (size / 1024.) * 0.8
    normal_d_y = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * (size / 1024.) * 0.8
    nz = 1.0 / np.sqrt(1.0 + normal_d_x * normal_d_x + normal_d_y * normal_d_y)
    nx = -normal_d_x * nz
    ny = -normal_d_y * nz
    normal = rgba((nx + 1.) * .5, (ny + 1.) * .5, (nz + 1.) * .5, np.ones_like(nx))

    # 岸线白沫Alpha采用平滑阈值，避免硬边阈值在动态UV下闪烁；RGB维持中性值便于着色。
    mask_signal = 0.78 * foam_base + 0.22 * foam_detail
    foam_opacity = np.clip((mask_signal - 0.02) * 0.42 + 0.33, 0., 1.)
    foam = rgba(np.ones_like(nx) * .92, np.ones_like(nx) * .96,
                np.ones_like(nx), foam_opacity)

    # 流向RG以0.5为静止流；x方向轻微弯曲，y方向保持主要向下，数值来自平铺噪声导数。
    flow_x = (np.roll(flow, -1, axis=1) - np.roll(flow, 1, axis=1))
    flow_y = (np.roll(flow, -1, axis=0) - np.roll(flow, 1, axis=0))
    field_x = .5 + .14 * np.tanh(flow_x)
    field_y = .5 + .32 * np.tanh(-.8 + .5 * flow_y)
    flow_map = rgba(field_x, field_y, np.clip(.5 + .2 * detail, 0., 1.),
                    np.ones_like(nx))

    # 近景焦散改为连续、平铺的扭曲光线网络，避免高频微分噪声出现满屏椒盐颗粒。
    # 周期整数相位保证四边首尾相接；低频周期扰动让光线柔和弯曲。
    unit = np.arange(size, dtype=np.float32) / size
    u, v = np.meshgrid(unit, unit)
    phase_a = 2. * np.pi * (9. * u + 7. * v + .035 * caustic_warp_x)
    phase_b = 2. * np.pi * (7. * u - 11. * v + .035 * caustic_warp_y)
    phase_c = 2. * np.pi * (13. * u + 5. * v + .022 * caustic_warp_x)
    ridge_a = np.exp(-np.square(np.sin(phase_a) / .17))
    ridge_b = np.exp(-np.square(np.sin(phase_b) / .16))
    ridge_c = np.exp(-np.square(np.sin(phase_c) / .16))
    bright = np.clip(.47 * ridge_a + .45 * ridge_b + .20 * ridge_c +
                     .16 * ridge_a * ridge_b, 0., 1.)
    caustic_map = rgba(bright, bright, bright, np.ones_like(nx))
    return {
        "T_GP_Water_TilingNormal_Source.png": normal,
        "T_GP_Water_ShoreFoam_Source.png": foam,
        "T_GP_Water_WaterfallFlow_Source.png": flow_map,
        "T_GP_Water_Caustics_Source.png": caustic_map,
    }


def generate(output_dir: Path, size: int, overwrite: bool) -> list[dict[str, object]]:
    """保存真正存在的PNG源文件和SHA256证据；失败不覆盖原有纹理资产。"""
    output_dir.mkdir(parents=True, exist_ok=True)
    outputs: list[dict[str, object]] = []
    tiles = render_tiles(size)
    registry = output_dir / "WaterTextureSourceManifest.json"
    # 一次性检查整批目标，任何同名已存在都不留下半批新纹理。
    existing = [path for path in [registry] + [output_dir / filename for filename in tiles]
                if path.exists()]
    if existing and not overwrite:
        raise FileExistsError(f"纹理原稿或清单已存在，拒绝部分覆盖：{existing}")
    for filename, image in tiles.items():
        target = output_dir / filename
        Image.fromarray(image).save(target, format="PNG", optimize=True)
        digest = hashlib.sha256(target.read_bytes()).hexdigest()
        kind = ("NormalMap（切线法线）" if "Normal" in filename else
                "FoamMask（岸线泡沫，Alpha有效）" if "Foam" in filename else
                "FlowMap（瀑布流向，RG以0.5为零速度）" if "Flow" in filename else
                "Caustics（焦散灰度，RGB同值）")
        outputs.append({
            "file": filename, "kind": kind, "width": size, "height": size,
            "format": "RGBA8", "sha256": digest,
            "sRGB": False,
            "说明": "真实PNG像素源图，非UE原生Texture2D；需由Monolith正式导入并重新审核压缩与像素通道",
        })
    registry.write_text(json.dumps({"version": 1, "items": outputs}, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8")
    return outputs


def main() -> int:
    parser = argparse.ArgumentParser(description="SSLM程序化水纹理真实PNG原稿生成器")
    parser.add_argument("--size", type=int, choices=(256, 512, 1024, 2048), default=1024)
    parser.add_argument("--overwrite", action="store_true",
                        help="仅明确要求覆盖本生成器以前的同名原稿时使用，不触碰UE .uasset")
    parser.add_argument("--output", type=Path, default=(
        Path(__file__).resolve().parents[2] / "Docs/Implementation/Water/SourceTextures"))
    args = parser.parse_args()
    files = generate(args.output, args.size, args.overwrite)
    print(f"已生成 {len(files)} 份可平铺水纹理PNG原稿，边长 {args.size}，保存目录 {args.output}")
    for entry in files:
        print(entry["file"], entry["sha256"][:16])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
