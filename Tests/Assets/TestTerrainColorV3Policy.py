#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V3.0十层地形配色源PNG、来源完整性、可回退资产身份的离线阻断门禁。

本脚本只读：对已有Poly Haven CC0扫描40图检查完整哈希，保证Normal/ORM/Height
没有因调色而被覆盖；确认新候选10张基础色遵循权威语义及PNG2048尺寸、颜色方向
和人工审批待完成标记。可选--ue-files只核对真实资产磁盘存在，
真正Texture2D类型、函数连线/Shader、编辑器地图必须由UE5.8独立进程回读。
不修改原图、.uasset、.umap或运行时配置。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAPPING = ROOT / "Tools/Unreal/Terrain/TerrainV3ColorMapping.json"
REPORT = ROOT / "Saved/ArtReview/TerrainColorV3/GenerationManifest.json"
LAYERS = {"Grass", "ForestSoil", "Dirt", "Rock", "Scree", "Sand",
          "Gravel", "RoadDirt", "FarmSoil", "ShoreMud"}


def ensure(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError("SSLM V3地形配色不合规：" + message)


def sha256(path: Path) -> str:
    dig = hashlib.sha256()
    with path.open("rb") as file:
        while chunk := file.read(1024 * 1024):
            dig.update(chunk)
    return dig.hexdigest()


def png_size(path: Path) -> tuple[int, int]:
    with path.open("rb") as f:
        data = f.read(24)
    ensure(data[:8] == b"\x89PNG\r\n\x1a\n" and data[12:16] == b"IHDR",
           "不是原生PNG图像：" + str(path))
    return struct.unpack(">II", data[16:24])


def color(hexvalue: str) -> tuple[int, int, int]:
    ensure(len(hexvalue) == 7 and hexvalue.startswith("#") and
           hexvalue[1:].upper() == hexvalue[1:],
           "色值不是约定的HEX表示")
    return tuple(int(hexvalue[i:i + 2], 16) for i in (1, 3, 5))


def color_distance(a: str, b: str) -> float:
    aa, bb = color(a), color(b)
    return sum((x-y)**2 for x,y in zip(aa,bb)) ** .5


def check(native: bool = False) -> None:
    cfg = json.loads(MAPPING.read_text(encoding="utf-8"))
    registry_file = ROOT / cfg["referencePalette"]
    registry = json.loads(registry_file.read_text(encoding="utf-8"))
    production_file = ROOT / cfg["baselinePBRManifest"]
    production = json.loads(production_file.read_text(encoding="utf-8"))
    report = json.loads(REPORT.read_text(encoding="utf-8-sig"))
    ensure(len(registry["items"]) == 41, "全局参考颜色索引必须含41色")
    ensure(len(production["files"]) == 40 and production["license"] == "CC0 1.0",
           "正式扫描PBR源40图或授权声明不完整")
    ensure(len(cfg["layers"]) == 10 and {x["layer"] for x in cfg["layers"]} == LAYERS,
           "配置层缺失或重复")
    ensure(report["paletteSha256"] == sha256(registry_file),
           "已派生色板不是当前全局真源")
    ensure(report["productionManifestSha256"] == sha256(production_file),
           "既有PBR来源版本漂移")
    ensure(len(report["layers"]) == 10 and {x["layer"] for x in report["layers"]} == LAYERS,
           "派生基础色十层不全")
    ensure(report["humanReviewStatusZh"] == "待审核",
           "没有独立人工签收证据，不得擅自标为已审核")
    catalog = {x["semanticId"]: x for x in registry["items"]}
    rows = {(x["layer"], x["role"]): x for x in production["files"]}
    generated = {x["layer"]: x for x in report["layers"]}
    for layer in cfg["layers"]:
        name = layer["layer"]
        key = layer["semanticId"]
        ensure(key in catalog and 0 < float(layer["strength"]) <= 1,
               "语义ID或校准强度非法：" + name)
        generated_row = generated[name]
        ensure(generated_row["semanticId"] == key and
               generated_row["targetSRGB"] == catalog[key]["referenceSRGBHex"],
               "没有从全局主索引派生：" + name)
        before, after = generated_row["meanBeforeAfter"].split("|")
        ensure(color_distance(after, generated_row["targetSRGB"]) <
               color_distance(before, generated_row["targetSRGB"]),
               "颜色校正未使基础色更接近设计目标：" + name)
        if name == "Grass":
            r, g, b = color(after)
            ensure(g > r > b, "草地不再是自然绿色主调")
        if name == "RoadDirt":
            ensure(sum(color(after)) > sum(color(before)) + 100,
                   "通行道路仍过暗")
        source = rows[(name, "BaseColor")]
        ensure(generated_row["originalSha256"] == source["sha256"] and
               generated_row["originalSourcePath"] == source["sourcePath"],
               "覆盖或绕过原始PBR来源：" + name)
        dst = ROOT / generated_row["derivedSourcePath"]
        ensure(dst.is_file() and png_size(dst) == (2048, 2048) and
               sha256(dst) == generated_row["derivedSha256"],
               "V3新基础色纹理不存在/尺寸或哈希异常：" + name)
        ensure(generated_row["futureUnrealAsset"].startswith(
                   "/DBAPresentationPack_Core/Materials/Terrain/TexturesV3/"),
               "新基础色不在客户端项目表现包：" + name)
    # 同时确认10组原始BaseColor/Normal/ORM/Height均与既有源指纹一致。
    for spec in production["files"]:
        source = ROOT / spec["sourcePath"]
        ensure(source.is_file() and sha256(source) == spec["sha256"],
               "源PBR贴图已改动：" + spec["layer"] + "/" + spec["role"])
        if spec["role"] == "BaseColor":
            ensure(spec["sRGB"] and spec["compression"] == "Default",
                   "原始基础色贴图颜色空间配置错误")
        else:
            ensure(not spec["sRGB"], "Normal/ORM/Height不得启用sRGB")
    # 未获人工批准的V3高精客户端材质，不得写入共享权威新手村地图。
    village = ROOT / ("Game/Plugins/DivineBeasts/ContentPacks/Worlds/"
                      "DBAWorldPack_Village/Content/Maps/L_Village_Start.umap")
    ensure(village.is_file() and
           b"M_DBA_Terrain_Production10_V3" not in village.read_bytes() and
           b"MI_DBA_Terrain_Production10_V3" not in village.read_bytes(),
           "V3审核候选被直接硬引用进服务器共用正式Village地图")
    if native:
        base = ROOT / ("Game/Plugins/DivineBeasts/ContentPacks/Presentation/"
                       "DBAPresentationPack_Core/Content/Materials/Terrain")
        for row in report["layers"]:
            asset = base / ("TexturesV3/T_DBA_Terrain_" + row["layer"] +
                            "_BaseColor_V3.uasset")
            ensure(asset.is_file() and asset.stat().st_size > 2048,
                   "未找到真实UE纹理文件：" + str(asset))
        for name in ["M_DBA_Terrain_Production10_V3"] + [
            "MI_DBA_Terrain_Production10_V3_" + x for x in
            ("Review", "High", "Medium", "Low")
        ]:
            obj = base / (name + ".uasset")
            ensure(obj.is_file() and obj.stat().st_size > 2048,
                   "未找到真实UE材质或实例：" + name)
    print("SSLM_TERRAIN_COLOR_V3_POLICY_STATIC_PASS",
          "layers=10 original_PBR=40",
          "derived_color=10", "native_files_checked=" + str(native),
          "approval=pending")


def test_negative_paths() -> None:
    ensure(color_distance("#AABBCC", "#AABBCC") == 0, "颜色距离算法错误")
    try:
        color("AAAAAA")
    except AssertionError:
        pass
    else:
        raise AssertionError("错误HEX没有被拒绝")
    print("SSLM_TERRAIN_V3_POLICY_NEGATIVE_SELF_TEST_PASS")


def main() -> None:
    parser = argparse.ArgumentParser(description="SSLM十层地形V3配色只读门禁")
    parser.add_argument("--ue-files", action="store_true", help="同时检查十五份实际UE资产文件是否存在")
    args = parser.parse_args()
    check(native=args.ue_files)
    test_negative_paths()


if __name__ == "__main__":
    main()
