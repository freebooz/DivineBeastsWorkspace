#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""天气P0源素材只读质量门禁：验证清单/指纹、透明图、无缝噪声、法线和WAV。

此测试不能代替UE纹理导入、Shader/Niagara真实资产编译、声音试听。
适用于工作区开发机与独立CI；不会删除或修改任何资源文件。
"""
from __future__ import annotations

import hashlib
import json
import wave
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "Tools/Unreal/Weather"


def checks() -> None:
    textures = json.loads((TOOLS / "WeatherSourceArtManifest.json").read_text(encoding="utf-8"))
    audio = json.loads((TOOLS / "WeatherAudioSourceManifest.json").read_text(encoding="utf-8"))
    assert textures["schemaVersion"] == 1 and len(textures["textures"]) == 9
    assert audio["schemaVersion"] == 1 and len(audio["audio"]) == 3
    names: set[str] = set()

    for entry in textures["textures"]:
        file = ROOT / entry["sourcePath"]
        assert file.is_file() and hashlib.sha256(file.read_bytes()).hexdigest() == entry["sha256"], file
        assert file.name not in names, file
        names.add(file.name)
        image = Image.open(file)
        assert list(image.size) == entry["size"] and image.mode == entry["mode"], file
        pixels = np.asarray(image, dtype=np.float32)
        if entry["mode"] == "RGBA":
            assert image.mode == "RGBA"
            a = pixels[:, :, 3]
            assert a.min() == 0 and a.max() > 90 and (a > 10).mean() > 0.01
            if "SnowflakeAtlas" in file.name:
                for row in range(4):
                    for col in range(4):
                        cell = a[row * 256:(row + 1) * 256, col * 256:(col + 1) * 256]
                        assert (cell > 10).sum() > 300, (file, row, col)
        if entry["owner"] == "Surface":
            # 周期遮罩与雪面采样不应存在显著跨边界阶跃。
            top = float(np.abs(pixels[0] - pixels[-1]).mean())
            left = float(np.abs(pixels[:, 0] - pixels[:, -1]).mean())
            assert top < 3.0 and left < 3.0, (file, top, left)
            if "SnowNormal" in file.name:
                assert pixels[:, :, 2].mean() > 240, "切线法线Z未朝外"
        print("SOURCE_TEXTURE_PASS", file.name)

    for entry in audio["audio"]:
        file = ROOT / entry["sourcePath"]
        assert file.is_file() and hashlib.sha256(file.read_bytes()).hexdigest() == entry["sha256"], file
        with wave.open(str(file), "rb") as reader:
            assert reader.getnchannels() == 2 and reader.getsampwidth() == 2
            assert reader.getframerate() == 48000 and reader.getnframes() == 576000
            data = np.frombuffer(reader.readframes(reader.getnframes()), dtype="<i2").reshape(-1, 2)
        assert not np.any(data == -32768)
        level = data.astype(np.float32) / 32767
        rms = float(np.sqrt(np.mean(level ** 2)))
        peak = float(np.max(np.abs(level)))
        assert 0.015 <= rms <= 0.18 and peak < 0.95, (file, rms, peak)
        assert float(np.max(np.abs(level[0] - level[-1]))) < .005, "循环边界存在较大突变"
        print("SOURCE_AUDIO_PASS", file.name)
    print("WEATHER_SOURCE_QA_PASS textures=9 audio=3")


if __name__ == "__main__":
    checks()
