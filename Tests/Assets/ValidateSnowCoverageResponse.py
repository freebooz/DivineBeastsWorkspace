#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""写实积雪遮罩曲线数值回归，不代替UE Shader编译及真人视觉验收。

所属：Tests/Assets，读取跨项目GamePlatformSurface原创宏观遮罩PNG源图。
前置：NumPy、Pillow在单独工程辅助Python环境中安装，禁止修改UE引擎依赖。
目标：小雪时仍有足够可辨的斑驳覆盖，大雪近连续，零雪严格为0。
坡度与高度在真实Shader及Review地图中验证；本检查先对水平单位遮罩核验。
"""
from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
TEXTURE = (
    ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/"
    "SourceArt/Snow/T_GP_Snow_CoverageNoise_Source.png"
)


def coverage(noise: np.ndarray, snow_amount: float, softness: float = .18) -> np.ndarray:
    """对应生产UE材质遮罩公式；Slope/Height均按水平标准测试表面1计算。"""
    if not 0.0 <= snow_amount <= 1.0:
        raise ValueError("雪量必须限制在0到1")
    return snow_amount * np.clip(
        (noise - (0.5 - snow_amount * 0.4)) / softness, 0.0, 1.0
    )


def main() -> None:
    if not TEXTURE.is_file():
        raise FileNotFoundError("雪覆盖噪声源图不存在：" + str(TEXTURE))
    with Image.open(TEXTURE) as img:
        if img.size != (2048, 2048) or img.mode != "L":
            raise RuntimeError("原始覆盖遮罩必须为2048灰度PNG")
        noise = np.asarray(img, dtype=np.float32) / 255.0

    samples = {x: coverage(noise, x) for x in (0.0, 0.23, 0.5, 0.85, 1.0)}
    assert np.all(samples[0.0] == 0), "零雪量仍显示雪，视觉状态合同失效"
    assert 0.07 < float(samples[0.23].mean()) < 0.22, "小雪薄覆强度错误"
    assert 0.55 < float(samples[0.85].mean()) < 0.90, "大雪连续程度错误"
    assert 0.0 < float((samples[0.23] > 0.01).mean()) < 0.90, "小雪必须保留部分露地"
    assert float((samples[0.85] > 0.10).mean()) > .97, "大雪须覆盖绝大多数水平表面"
    for left, right in zip((0.0, 0.23, 0.5, 0.85), (0.23, 0.5, 0.85, 1.0)):
        if not bool(np.all(samples[right] >= samples[left] - 1e-6)):
            raise RuntimeError("雪量增加却导致局部积雪减少，曲线不单调")
    for amount, array in samples.items():
        print("SNOW_COVERAGE_AMOUNT", f"{amount:.2f}", "MEAN", round(float(array.mean()), 4))
    print("SNOW_COVERAGE_NUMERICAL_PASS samples=5")


if __name__ == "__main__":
    main()
