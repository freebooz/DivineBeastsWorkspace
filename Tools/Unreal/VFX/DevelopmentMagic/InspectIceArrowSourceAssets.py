#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""只读检查 SSLM 寒冰箭技术源文件，并生成通道诊断图与 JSON 清单。

本文件属于项目制作工具层，由美术审核人员在工作空间运行；依赖 Pillow 与 NumPy。
输入是公共表现包既有 PNG/OBJ，输出仅写 Saved/Validation；不修改源图或 UE 资产。
透明效果以 Alpha 灰度图检查，避免白色 RGB 在白底预览中被误判为空图。
通道、边界和几何统计不等于 UE 导入设置、着色器编译或人工视觉验收。
"""

from pathlib import Path
import hashlib
import json
import math

import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[4]
SOURCE = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/IceArrow"
OUTPUT = ROOT / "Saved/Validation/IceArrow/Production_20261016"


def inspect_texture(path):
    """读取单张源纹理；返回可序列化统计和通道预览，不回写原始像素。

    Alpha 范围与边界取 0—255 字节值；没有 Alpha 的技术图返回 null。
    接触图仅显示 Alpha 或原 RGB，不能作为材质颜色验收依据。
    """
    with Image.open(path) as opened:
        picture = opened.copy()
    has_alpha = "A" in picture.getbands()
    alpha = np.asarray(picture.getchannel("A")) if has_alpha else None
    border = np.concatenate((alpha[0], alpha[-1], alpha[:, 0], alpha[:, -1])) if has_alpha else None
    result = {
        "Name": path.name,
        "DimensionsPixels": list(picture.size),
        "Mode": picture.mode,
        "SHA256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "HasAlpha": has_alpha,
        "AlphaRange": [int(alpha.min()), int(alpha.max())] if has_alpha else None,
        "FullyTransparentFraction": float((alpha == 0).mean()) if has_alpha else None,
        "NonTransparentBorderPixels": int((border != 0).sum()) if has_alpha else None,
        "PreviewChannelZh": "Alpha灰度（白色表示不透明）" if has_alpha else "原RGB/灰度技术通道",
    }
    return result, (picture.getchannel("A").convert("RGB") if has_alpha else picture.convert("RGB"))


def inspect_mesh(path):
    """读取 OBJ 的厘米坐标、UV 和面索引数量；不替代引擎导入及法线检查。

    空网格或非法数值直接抛错，禁止用空包围盒生成成功报告。
    """
    lines = path.read_text(encoding="utf-8").splitlines()
    vertices = np.array([[float(value) for value in line.split()[1:4]] for line in lines if line.startswith("v ")])
    if vertices.size == 0 or not np.isfinite(vertices).all():
        raise ValueError("网格没有有效顶点：" + path.name)
    minimum, maximum = vertices.min(axis=0), vertices.max(axis=0)
    return {
        "Name": path.name,
        "SHA256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "VertexRecords": len(vertices),
        "FaceRecords": sum(line.startswith("f ") for line in lines),
        "UVRecords": sum(line.startswith("vt ") for line in lines),
        "BoundsMinCentimeters": minimum.tolist(),
        "BoundsMaxCentimeters": maximum.tolist(),
        "DimensionsCentimeters": (maximum - minimum).tolist(),
    }


def main():
    """只扫描技术源目录第一层，排除 Concepts 设计参考；异常保持非零退出。

    输出可重建诊断图和统计；相同输入可重复检查，不覆盖设计源图或正式资源。
    """
    textures = sorted(SOURCE.glob("*.png"))
    meshes = sorted(SOURCE.glob("*.obj"))
    if not textures or not meshes:
        raise FileNotFoundError("缺少寒冰箭技术源文件，拒绝生成空检查报告")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    cell_width, cell_height, columns = 300, 324, 4
    sheet = Image.new("RGB", (columns * cell_width, math.ceil(len(textures) / columns) * cell_height), (24, 28, 35))
    draw = ImageDraw.Draw(sheet)
    texture_rows = []
    for index, path in enumerate(textures):
        row, preview = inspect_texture(path)
        texture_rows.append(row)
        x, y = (index % columns) * cell_width, (index // columns) * cell_height
        preview.thumbnail((280, 280), Image.Resampling.LANCZOS)
        sheet.paste(preview, (x + 10, y + 10))
        draw.text((x + 10, y + 294), path.stem.removeprefix("T_DBA_IceArrow_") + (" [Alpha]" if row["HasAlpha"] else " [RGB]"), fill=(225, 233, 242))
    report = {
        "ScopeZh": "技术源通道和几何只读检查；未检查UE导入、材质或运行效果",
        "Textures": texture_rows,
        "Meshes": [inspect_mesh(path) for path in meshes],
        "EditorVerified": False,
        "ManualVisualAccepted": False,
    }
    sheet.save(OUTPUT / "SourceChannelReview.png")
    (OUTPUT / "SourceGeometryAndChannels.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"Textures": len(texture_rows), "Meshes": len(meshes), "TransparentTextures": sum(row["HasAlpha"] for row in texture_rows), "AlphaBorderErrors": [row["Name"] for row in texture_rows if row["NonTransparentBorderPixels"]]}, ensure_ascii=False))


if __name__ == "__main__":
    main()
