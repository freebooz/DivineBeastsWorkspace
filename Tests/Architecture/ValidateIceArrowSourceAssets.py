#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM寒冰箭源纹理与网格静态门禁（仅使用Python标准库，不依赖外部图像库）。

只读取本项目受控SourceArt，不加载UE、不修改纹理或二进制资产。
PNG校验覆盖IHDR、CRC、真实IDAT解码、边界Alpha和法线向量，所有失败明确返回1。
通过仅证明源纹理合同，不能取代Niagara Shader、运行画面、Cook与人工视觉验收。
"""
from __future__ import annotations

import importlib.util
import math
import struct
import sys
import tempfile
import types
import zlib
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/IceArrow"
SPRITES = ("RuneOuter", "RuneInner", "CrystalShard", "FrostMist",
           "TrailStrip", "IceFlake", "ImpactFlash", "FrozenGround")
PNG_SIGNATURE = bytes.fromhex("89504e470d0a1a0a")
COLOR_CHANNELS = {0: 1, 2: 3, 6: 4}
MAX_TEXTURE_DIM = 4096
MAX_COMPRESSED_BYTES = 64 * 1024 * 1024


def _paeth(left: int, above: int, upper_left: int) -> int:
    """复原PNG Paeth预测值，不依赖图像库的可变像素缓存。"""
    predicted = left + above - upper_left
    distances = (abs(predicted - left), abs(predicted - above),
                 abs(predicted - upper_left))
    if distances[0] <= distances[1] and distances[0] <= distances[2]:
        return left
    return above if distances[1] <= distances[2] else upper_left


def decode_png_channels(path: Path, selected: tuple[int, ...]) -> tuple[int, int, int, tuple[bytearray, ...]]:
    """按实际PNG过滤器只重建所需颜色通道，避免加载完整RGBA与外部NumPy。

    仅支持受控源资产约定：非交错、8位灰度/RGB/RGBA。
    CRC、块边界、尺寸、数据长度或过滤器不合法一律抛ValueError，禁止吞错算通过。
    """
    data = path.read_bytes()
    if len(data) < 45 or not data.startswith(PNG_SIGNATURE):
        raise ValueError("PNG签名或长度非法")
    position = len(PNG_SIGNATURE)
    image_header = None
    compressed_chunks: list[bytes] = []
    compressed_size = 0
    saw_end = False
    while position + 12 <= len(data):
        length = int.from_bytes(data[position:position + 4], "big")
        kind = data[position + 4:position + 8]
        end = position + 12 + length
        if length > MAX_COMPRESSED_BYTES or end > len(data):
            raise ValueError("PNG数据块越界或大小异常")
        payload = data[position + 8:position + 8 + length]
        expected_crc = int.from_bytes(data[position + 8 + length:end], "big")
        if zlib.crc32(kind + payload) & 0xffffffff != expected_crc:
            raise ValueError("PNG数据块CRC不匹配")
        if kind == b"IHDR":
            if image_header is not None or length != 13 or position != 8:
                raise ValueError("PNG头部重复或不是第一个数据块")
            width, height, depth, color_type, compression, filtration, interlace = struct.unpack(">IIBBBBB", payload)
            if (depth != 8 or color_type not in COLOR_CHANNELS or
                    compression != 0 or filtration != 0 or interlace != 0 or
                    not (1 <= width <= MAX_TEXTURE_DIM) or
                    not (1 <= height <= MAX_TEXTURE_DIM)):
                raise ValueError("PNG色彩模式、尺寸或交错方式不符合源图合同")
            image_header = (width, height, color_type)
        elif kind == b"IDAT":
            if image_header is None:
                raise ValueError("PNG图像数据出现在头部之前")
            compressed_size += length
            if compressed_size > MAX_COMPRESSED_BYTES:
                raise ValueError("PNG压缩数据超过安全上限")
            compressed_chunks.append(payload)
        elif kind == b"IEND":
            if length != 0 or image_header is None or not compressed_chunks:
                raise ValueError("PNG结束块不合法")
            saw_end = True
            if end != len(data):
                raise ValueError("PNG结束块后有多余数据")
            break
        position = end
    if not saw_end or image_header is None:
        raise ValueError("PNG缺少完整头部、图像或结束标记")

    width, height, color_type = image_header
    channels = COLOR_CHANNELS[color_type]
    if len(set(selected)) != len(selected) or any(c < 0 or c >= channels for c in selected):
        raise ValueError("请求的PNG通道不存在或重复")
    row_bytes = width * channels
    expected_bytes = height * (row_bytes + 1)
    decompressor = zlib.decompressobj()
    raw = decompressor.decompress(b"".join(compressed_chunks), expected_bytes + 1)
    if len(raw) != expected_bytes or not decompressor.eof or decompressor.unconsumed_tail or decompressor.unused_data:
        raise ValueError("PNG解压长度或压缩终态不符合IHDR")
    decoded = tuple(bytearray(width * height) for _ in selected)
    for y in range(height):
        row_start = y * (row_bytes + 1)
        filter_type = raw[row_start]
        if filter_type > 4:
            raise ValueError("PNG包含不合法的扫描行过滤器")
        source_start = row_start + 1
        output_start = y * width
        previous_start = output_start - width
        for x in range(width):
            index = output_start + x
            prior_index = previous_start + x
            for output_channel, original_channel in zip(decoded, selected):
                left = output_channel[index - 1] if x > 0 else 0
                above = output_channel[prior_index] if y > 0 else 0
                upper_left = output_channel[prior_index - 1] if x > 0 and y > 0 else 0
                if filter_type == 0:
                    predicted = 0
                elif filter_type == 1:
                    predicted = left
                elif filter_type == 2:
                    predicted = above
                elif filter_type == 3:
                    predicted = (left + above) // 2
                else:
                    predicted = _paeth(left, above, upper_left)
                output_channel[index] = (raw[source_start + x * channels + original_channel] + predicted) & 255
    return width, height, color_type, decoded


def inspect_sources() -> list[str]:
    """检查实际源纹理边界和法线数学；每一缺项逐项记录，不把无法解码误记为通过。"""
    failures: list[str] = []
    for name in SPRITES:
        path = SOURCE / ("T_DBA_IceArrow_" + name + ".png")
        if not path.is_file():
            failures.append("缺少独立透明纹理：" + name)
            continue
        try:
            width, height, color_type, outputs = decode_png_channels(path, (3,))
            if color_type != 6 or width < 512 or height < 512:
                failures.append("尺寸/透明格式不合格：" + name)
                continue
            alpha = outputs[0]
            border_opaque = any(alpha[:width]) or any(alpha[-width:])
            border_opaque = border_opaque or any(
                alpha[y * width] or alpha[y * width + width - 1] for y in range(height)
            )
            if border_opaque or max(alpha) < 100 or alpha.count(0) / len(alpha) < .03:
                failures.append("透明通道或留边不合格：" + name)
        except (OSError, ValueError, zlib.error, struct.error) as error:
            failures.append("独立透明纹理无法安全解码：" + name + " (" + type(error).__name__ + ")")
    for suffix in ("CrackMask", "IceNormal", "IceORM"):
        path = SOURCE / ("T_DBA_IceArrow_" + suffix + ".png")
        if not path.is_file():
            failures.append("缺少技术贴图：" + suffix)
            continue
        try:
            selected = (0, 1, 2) if suffix == "IceNormal" else ()
            width, height, color_type, outputs = decode_png_channels(path, selected)
            if width < 512 or height < 512:
                failures.append("技术贴图分辨率不足：" + suffix)
            if suffix == "IceNormal":
                if color_type != 2:
                    failures.append("切线空间法线必须是RGB源图")
                    continue
                red, green, blue = outputs
                error_sum = 0.0
                min_z = 1.0
                for r, g, b in zip(red, green, blue):
                    nx, ny, nz = r / 127.5 - 1, g / 127.5 - 1, b / 127.5 - 1
                    error_sum += abs(math.sqrt(nx * nx + ny * ny + nz * nz) - 1)
                    min_z = min(min_z, nz)
                if error_sum / (width * height) > .04 or min_z < .3:
                    failures.append("切线空间法线未归一化或朝向异常")
        except (OSError, ValueError, zlib.error, struct.error) as error:
            failures.append("技术贴图无法安全解码：" + suffix + " (" + type(error).__name__ + ")")
    for name in ("CrystalLance", "CrystalShard", "IceCrown", "TrailRibbon"):
        path = SOURCE / ("SM_DBA_IceArrow_" + name + ".obj")
        if not path.is_file():
            failures.append("缺少网格源：" + name)
            continue
        try:
            source = path.read_text("utf-8")
            if (source.count("\nv ") < 6 or source.count("\nf ") < 8 or
                    "\nvt " not in source or "\nvn " not in source):
                failures.append("网格三角面/UV/法线不足：" + name)
        except (OSError, UnicodeError):
            failures.append("网格源编码或读取失败：" + name)
    return failures


def inspect_sequence_isolation():
    """用纯Python检查每轮截图独占目录；不启动UE、不执行Niagara，也不触及正式资产。"""
    problems = []
    script = ROOT / 'Tools/Unreal/VFX/DevelopmentMagic/ReviewIceArrowSequenceA.py'
    if not script.is_file():
        return ['缺少寒冰箭逐帧审核脚本']
    # Unreal模块只在定义真实审核类时被引用；隔离目录函数可在普通Python中独立验证。
    with patch.dict(sys.modules, {'unreal': types.ModuleType('unreal')}):
        spec = importlib.util.spec_from_file_location('ice_arrow_sequence_isolation_test', script)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
    with tempfile.TemporaryDirectory(prefix='sslm_ice_arrow_review_') as temp:
        folder = Path(temp)
        previous = folder / 'SequenceA'
        previous.mkdir()
        original = previous / 'Frame_0030.png'
        original.write_bytes(b'previous-review-evidence')
        first_id, first = module.reserve_sequence_directory(folder / 'SequenceRuns')
        first_report = first / 'SequenceReport.json'
        first_report.write_text('{"round":1}', encoding='utf-8')
        second_id, second = module.reserve_sequence_directory(folder / 'SequenceRuns')
        if first_id == second_id or first == second or not first.is_dir() or not second.is_dir():
            problems.append('连续两次审查没有获得独立目录')
        if not first_report.is_file() or first_report.read_text(encoding='utf-8') != '{"round":1}':
            problems.append('第二轮审查覆盖第一轮报告')
        if original.read_bytes() != b'previous-review-evidence':
            problems.append('新版审查修改了历史SequenceA帧')
    return problems


if __name__ == '__main__':
    problems = inspect_sources() + inspect_sequence_isolation()
    for problem in problems: print('FAIL', problem)
    print('ICE_ARROW_SOURCE_GATE', 'FAIL' if problems else 'PASS', 'issues=', len(problems))
    sys.exit(1 if problems else 0)
