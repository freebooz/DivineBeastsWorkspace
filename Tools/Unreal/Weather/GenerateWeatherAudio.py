#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成雨/风三种48kHz 16-bit PCM可循环天气音源。

这些是原创合成声音，未使用第三方音频采样；可作项目P0声音基底与调试。
不伪装成野外实录、已发布AAA母带、已完成UE SoundWave/SFX Definition。
不覆盖已存在的人工作品。--verify只读对音源SHA256/PCM指标复核。
消费者：DBAWorldPack_Village（第三层项目声音内容）；运行期由GamePlatformSFX消费。
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
import math
import wave
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Weather/Audio"
MANIFEST = Path(__file__).with_name("WeatherAudioSourceManifest.json")
SAMPLE_RATE = 48000
DURATION = 12
SAMPLES = SAMPLE_RATE * DURATION
SEED = 20261010


def spectral_noise(rng: np.random.Generator, kind: str) -> np.ndarray:
    """带限、周期性的合成环境声基底；频谱相位随机且不引用外部音频。"""
    freqs = np.fft.rfftfreq(SAMPLES, 1 / SAMPLE_RATE)
    if kind == "wind":
        envelope = (freqs / (freqs + 24))**1.4 / (1 + (freqs / 520)**2.2)
    elif kind == "heavy":
        envelope = (freqs / (freqs + 180)) * np.exp(-(freqs / 12500)**2)
        envelope *= 0.5 + 0.5 * np.clip(freqs / 1300, 0, 1)
    else:
        envelope = (freqs / (freqs + 260))**1.3 * np.exp(-(freqs / 12000)**2)
        envelope *= 0.4 + 0.6 * np.clip(freqs / 3600, 0, 1)
    envelope[0] = 0.0
    phases = rng.uniform(-math.pi, math.pi, len(freqs))
    spectrum = envelope * (np.cos(phases) + 1j * np.sin(phases))
    wave_data = np.fft.irfft(spectrum, n=SAMPLES)
    wave_data = wave_data / max(1e-10, np.std(wave_data))
    return wave_data.astype(np.float32)


def raindrop_layer(rng: np.random.Generator, rate: float) -> tuple[np.ndarray, np.ndarray]:
    """稀疏冲击与细碎水声，多点随机位置，跨边界取模形成循环。"""
    out_l = np.zeros(SAMPLES, dtype=np.float32)
    out_r = np.zeros(SAMPLES, dtype=np.float32)
    count = int(rate * DURATION)
    for _ in range(count):
        start = int(rng.integers(0, SAMPLES))
        length = int(rng.integers(140, 900))
        k = np.arange(length, dtype=np.float32)
        decay = np.exp(-k / (length * rng.uniform(0.19, .38)))
        freq = float(rng.uniform(650, 6200))
        chirp = np.sin((2 * np.pi * freq / SAMPLE_RATE) * k + .1 * np.sin(k / 11))
        snap = (chirp * decay).astype(np.float32)
        energy = float(rng.uniform(.15, 1.0))**2
        pan = float(rng.uniform(-.85, .85))
        L = math.sqrt((1 - pan) * .5)
        R = math.sqrt((1 + pan) * .5)
        indices = (start + np.arange(length)) % SAMPLES
        out_l[indices] += snap * energy * L
        out_r[indices] += snap * energy * R
    return out_l, out_r


def create_audio(kind: str) -> np.ndarray:
    rng = np.random.default_rng(SEED + {"light": 1, "heavy": 2, "wind": 3}[kind])
    left = spectral_noise(rng, kind)
    right = spectral_noise(rng, kind)
    if kind == "wind":
        # 周期性低速包络，避免连续白噪声听感；周期整数确保循环时包络不突变。
        t = np.arange(SAMPLES, dtype=np.float32) / SAMPLE_RATE
        gust = 0.58 + .22 * np.sin(2 * np.pi * t / 4 + .4) + .12 * np.sin(2 * np.pi * t / 3 + 1.7)
        audio = np.stack((left * gust, right * (.90 * gust + .1)), axis=-1)
        target_rms = 10 ** (-24 / 20)
    else:
        droplets_l, droplets_r = raindrop_layer(rng, 22 if kind == "light" else 88)
        bed = .63 if kind == "light" else .85
        impact = .07 if kind == "light" else .12
        audio = np.stack((bed * left + impact * droplets_l,
                          bed * right + impact * droplets_r), axis=-1)
        target_rms = 10 ** ((-25 if kind == "light" else -19) / 20)
    # 防止边界波形因数值取样不完全一致产生循环咔哒，最后10ms沿首尾段连续调和。
    overlap = 480
    start = audio[:overlap].copy()
    end = audio[-overlap:].copy()
    ramp = np.linspace(0, 1, overlap, dtype=np.float32)[:, None]
    # 让最后的采样点对齐循环起点，10毫秒内平滑过渡，避免接缝咔哒。
    smooth = end * (1 - ramp) + start[::-1] * ramp
    audio[-overlap:] = smooth
    audio -= audio.mean(axis=0, keepdims=True)
    rms = float(np.sqrt(np.mean(audio.astype(np.float64)**2)))
    audio *= target_rms / max(rms, 1e-8)
    peak = float(np.max(np.abs(audio)))
    if peak > .91:
        audio *= .91 / peak
    return audio.astype(np.float32)


def save_bytes(audio: np.ndarray) -> bytes:
    pcm = np.round(np.clip(audio, -1, 1) * 32767).astype("<i2")
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(pcm.tobytes())
    return buf.getvalue()


def run(verify: bool, refresh_generated: bool = False) -> None:
    # 刷新时先检查旧Manifest与全部WAV指纹，不允许覆盖任意人工编辑。
    old_by_name = {}
    if refresh_generated:
        if not MANIFEST.is_file():
            raise FileNotFoundError("找不到前一次合成清单，拒绝刷新")
        old = json.loads(MANIFEST.read_text(encoding="utf-8"))
        if old.get("schemaVersion") != 1 or len(old.get("audio", [])) != 3:
            raise RuntimeError("既有音频清单不符合预期")
        for e in old["audio"]:
            fp = (ROOT / e["sourcePath"]).resolve()
            if not fp.is_relative_to(OUTPUT) or not fp.is_file():
                raise RuntimeError(f"既有音频路径非法：{fp}")
            if hashlib.sha256(fp.read_bytes()).hexdigest() != e["sha256"]:
                raise RuntimeError(f"既有音源经过人工修改，拒绝覆盖：{fp}")
            old_by_name[fp.name] = e
    entries = []
    for kind, file in (
        ("light", "SW_DBA_Weather_RainLight_Loop.wav"),
        ("heavy", "SW_DBA_Weather_RainHeavy_Loop.wav"),
        ("wind", "SW_DBA_Weather_Wind_Loop.wav"),
    ):
        audio = create_audio(kind)
        if not np.isfinite(audio).all():
            raise ValueError(f"NaN/Inf audio {file}")
        rms = float(np.sqrt(np.mean(audio.astype(np.float64)**2)))
        peak = float(np.max(np.abs(audio)))
        if not (0.015 <= rms <= 0.18 and peak < .95):
            raise ValueError(f"信号电平或峰值异常：{file} rms={rms} peak={peak}")
        blob = save_bytes(audio)
        digest = hashlib.sha256(blob).hexdigest()
        target = OUTPUT / file
        if verify:
            if not target.is_file() or hashlib.sha256(target.read_bytes()).hexdigest() != digest:
                raise RuntimeError(f"音频缺失或与清单不一致：{target}")
        else:
            old_digest = hashlib.sha256(target.read_bytes()).hexdigest() if target.exists() else None
            if old_digest != digest:
                if old_digest is not None and (not refresh_generated or file not in old_by_name):
                    raise FileExistsError(f"已有不同内容，拒绝覆盖原音效：{target}")
                OUTPUT.mkdir(parents=True, exist_ok=True)
                target.write_bytes(blob)
        entries.append({
            "sourcePath": target.relative_to(ROOT).as_posix(),
            "assetName": target.stem,
            "recommendedAssetPath": "/DBASFXPack_Core/Weather/Audio/" + target.stem,
            "destinationState": "客户端独立内容包尚未登记；当前Village SourceArt只存未烘焙WAV",
            "sha256": digest,
            "sampleRate": SAMPLE_RATE,
            "channels": 2,
            "sampleWidthBits": 16,
            "durationSeconds": DURATION,
            "peak": round(peak, 6),
            "rms": round(rms, 6),
            "origin": "synthetic-generated-in-workspace",
            "license": "workspace-original",
            "engineAssetCreated": False,
            "qualityGate": "人工试听/循环无爆音/MetaSound混音/真实场景响度测量",
        })
        print(f"{'VERIFY' if verify else 'SOURCE'} {file} 48kHz stereo 12s rms={rms:.4f} peak={peak:.4f} sha256={digest[:16]}")
    manifest = {
        "schemaVersion": 1,
        "generator": "Tools/Unreal/Weather/GenerateWeatherAudio.py",
        "note": "原创合成基底，不是第三方授权录音，需在UE中实际导入为SoundWave并经试听审核。",
        "audio": entries,
    }
    payload = json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"
    if verify:
        if not MANIFEST.is_file() or MANIFEST.read_text(encoding="utf-8") != payload:
            raise RuntimeError("音频Manifest不一致")
    else:
        if MANIFEST.exists() and MANIFEST.read_text(encoding="utf-8") != payload and not refresh_generated:
            raise FileExistsError(f"拒绝覆盖已修改的音频Manifest：{MANIFEST}")
        MANIFEST.write_text(payload, encoding="utf-8")
    print("PASS" if verify else "GENERATED", len(entries))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="生成/验证雨与风P0声音源WAV")
    parser.add_argument("--verify", action="store_true", help="仅验指纹不修改文件")
    parser.add_argument("--refresh-generated", action="store_true", help="仅在旧清单和3个源WAV指纹都匹配时刷新")
    args = parser.parse_args()
    if args.verify and args.refresh_generated:
        parser.error("--verify与--refresh-generated不能同时使用")
    run(args.verify, args.refresh_generated)
