#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""积雪细节源纹理形态/真实性/无缝性质量门禁。

归属：Tests/Architecture；适用 Python3+Pillow+NumPy 环境。
只读：验证 Manifest、尺寸、颜色通道、SHA256、雪粉Alpha、无缝边缘。
不调用UE、不冒充Shader/Niagara/客户端/服务器运行及人工视觉验收。
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "Tools/Unreal/Weather/SnowDetailSourceManifest.json"


def main() -> None:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    assert manifest["schemaVersion"] == 1 and len(manifest["textures"]) == 3, "资源记录数或版本不正确"
    targets = set()
    for entry in manifest["textures"]:
        path = ROOT / entry["sourcePath"]
        assert path.is_file(), f"缺失的真实PNG源纹理: {path}"
        assert hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"], f"源PNG被意外修改: {path}"
        with Image.open(path) as img:
            img.load()
            assert img.format == "PNG", f"来源并非真实PNG: {path}"
            assert list(img.size) == entry["size"] and img.mode == entry["mode"], f"贴图尺寸/通道错误: {path}"
            ar = np.asarray(img)
            if img.mode == "L":
                # 周期噪声左右/上下边缘的采样差必须处于可见接缝阈值以下。
                horizontal = np.abs(ar[:, 0].astype(np.int16) - ar[:, -1].astype(np.int16))
                vertical = np.abs(ar[0, :].astype(np.int16) - ar[-1, :].astype(np.int16))
                assert np.percentile(horizontal, 99) < 14, f"水平方向存在明显重复边缘: {path}"
                assert np.percentile(vertical, 99) < 14, f"垂直方向存在明显重复边缘: {path}"
                assert int(ar.max()) - int(ar.min()) >= 28, f"纹理几乎没有微观变化: {path}"
            elif img.mode == "RGBA":
                alpha = ar[:, :, 3]
                assert int(alpha.min()) == 0 and int(alpha.max()) > 80, f"雪粉不具备正确透明度: {path}"
                assert np.all(alpha[0, :] == 0) and np.all(alpha[-1, :] == 0), f"雪粉上下边界未消隐: {path}"
        assert entry["assetPath"] not in targets and entry["assetPath"].isascii(), "资产身份重复或格式错误"
        targets.add(entry["assetPath"])
        print("SNOW_QA_ASSET_OK", entry["assetPath"], entry["sha256"])
    print("SNOW_SOURCE_QA_PASS textures=3")


if __name__ == "__main__":
    main()
