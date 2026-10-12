#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Gaea/Landscape/PCG 第二版地貌语义回归。

V1既有校准资产不能被隐式改写；V2通用排除不应生成道路材质，
明确独立RoadOverlay才允许生成RoadDirt，且源类型仍为 calibration_only_not_gaea。
这里验证实际生成的10张PNG+1024个PCG Schema点及Manifest SHA-256；
不宣称已导入UE、不代替地形导航和Cook。
"""
from __future__ import annotations

import hashlib
import json
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/Unreal/Terrain"))
from TerrainMaskRules import (LAYERS, MASK_NAMES, project_landscape_weights,
                              sample_pcg_schema_v1)
from ValidateAndProjectGaeaExports import parse_source

SOURCE = ROOT / "Tools/Unreal/Terrain/TerrainSourceManifest_v1.json"
DERIVED = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Terrain/CalibrationDerivedV2"
REQUIRED_KEYS = {
    "Pcg.Biome.Id", "Pcg.Biome.Weight", "Pcg.Biome.Priority",
    "Pcg.Layer.Name", "Pcg.Layer.Index", "Pcg.Rule.Slope",
    "Pcg.Rule.Height", "Pcg.Rule.Wetness", "Pcg.Exclude.Mask",
    "Pcg.Exclude.Source", "Pcg.Exec.Seed",
}


def must_raise(action, label: str) -> None:
    try:
        action()
    except (ValueError, TypeError, RuntimeError):
        return
    raise AssertionError("无效投影被接受：" + label)


def verify() -> None:
    doc = json.loads(SOURCE.read_text(encoding="utf-8"))
    assert doc["sourceKind"] == "procedural_calibration_not_gaea"
    height, masks, world, kind = parse_source(doc, calibration=True)
    assert kind == "calibration_only_not_gaea" and height.shape == (1009, 1009)
    meta_path = DERIVED / "TerrainProjectionManifest_v1.json"
    meta = json.loads(meta_path.read_text(encoding="utf-8"))
    assert meta["schemaVersion"] == 1 and meta["projectionVersion"] == 2
    assert meta["sourceKind"] == "calibration_only_not_gaea"
    assert meta["roadOverlaySHA256"] is None
    assert meta["pcgSchemaVersion"] == "1.0"
    assert meta["sourceSHA256"] == hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    assert len(meta["generatedFiles"]) == 11

    # Manifest每一条都是独立新建的真实源文件，不以清单项代替内容。
    for item in meta["generatedFiles"]:
        relative = Path(item["relativePath"])
        assert not relative.is_absolute() and ".." not in relative.parts
        source = (DERIVED / relative).resolve()
        assert source.is_relative_to(DERIVED.resolve()) and source.is_file()
        assert hashlib.sha256(source.read_bytes()).hexdigest() == item["sha256"]

    weights = project_landscape_weights(masks, projection_version=2)
    loaded: dict[str, np.ndarray] = {}
    for layer in LAYERS:
        with Image.open(DERIVED / "Weights" / f"LS_{layer}_Import.png") as im:
            assert im.format == "PNG" and im.mode == "L"
            loaded[layer] = np.asarray(im, dtype=np.uint8)
        assert np.array_equal(loaded[layer], weights[layer]), "投影权重内容不同步：" + layer

    stacks = np.stack([loaded[name].astype(np.uint16) for name in LAYERS])
    assert int(stacks.sum(axis=0).min()) == int(stacks.sum(axis=0).max()) == 255
    assert np.count_nonzero(loaded["RoadDirt"]) == 0, (
        "没有独立RoadOverlay时不能将Gameplay.Exclusion强行生产道路材质"
    )
    assert np.count_nonzero(masks["Exclusion"] > .5) > 0

    points = sample_pcg_schema_v1(
        masks, weights, height, world, grid_step=32, projection_version=2)
    points_path = DERIVED / "PCG/PCG_SchemaV1_TerrainCandidates.json"
    persisted = json.loads(points_path.read_text(encoding="utf-8"))
    assert persisted["projectionVersion"] == 2 and persisted["schemaVersion"] == 1
    assert persisted["sourceKind"] == "calibration_only_not_gaea"
    assert persisted["pointCount"] == len(points) == 1024
    assert persisted["points"] == points
    exclusions = 0
    for point in points:
        assert set(point).issuperset({"Grid", "WorldPositionCm"})
        assert {field for field in point if field.startswith("Pcg.")} == REQUIRED_KEYS
        assert math.isfinite(point["Pcg.Rule.Height"])
        assert all(math.isfinite(value) for value in point["WorldPositionCm"])
        source = point["Pcg.Exclude.Source"]
        if point["Pcg.Exclude.Mask"] >= .5:
            assert source == "Gameplay.Exclusion"
            exclusions += 1
        else:
            assert source == "None"
        assert source != "Road.Network", "通用排除区不能隐式获得道路来源身份"
    assert exclusions > 0, "真实校准掩码未覆盖任何需要测试的高排除区域"

    # 独立道路仅通过有证据的项目覆盖图启用，不能改变通用Exclusion.Source。
    synthetic = {name: np.zeros((32, 32), dtype=np.float32) for name in MASK_NAMES}
    synthetic["Exclusion"][:, :] = .75
    road = np.zeros((32, 32), dtype=np.float32)
    road[:, 10:14] = 1.0
    plain = project_landscape_weights(synthetic, projection_version=2)
    marked = project_landscape_weights(synthetic, projection_version=2, road_overlay=road)
    assert np.count_nonzero(plain["RoadDirt"]) == 0
    assert int(marked["RoadDirt"][0, 11]) > 100
    assert int(marked["RoadDirt"][0, 5]) == 0
    assert np.all(np.stack([marked[x].astype(np.uint16) for x in LAYERS]).sum(axis=0) == 255)
    height32 = np.zeros((32, 32), dtype=np.uint16)
    pts = sample_pcg_schema_v1(
        synthetic, marked, height32, world, grid_step=8, projection_version=2)
    assert all(p["Pcg.Exclude.Source"] == "Gameplay.Exclusion" for p in pts)
    must_raise(lambda: project_landscape_weights(synthetic, projection_version=3), "不支持的新版投影")
    must_raise(lambda: project_landscape_weights(synthetic, projection_version=1, road_overlay=road),
               "旧版使用新道路覆盖图")
    must_raise(lambda: project_landscape_weights(synthetic, projection_version=2, road_overlay=road[:16]),
               "道路覆盖图宽高不匹配")
    bad_road = road.copy()
    bad_road[0, 0] = float("nan")
    must_raise(lambda: project_landscape_weights(synthetic, projection_version=2, road_overlay=bad_road),
               "道路覆盖图含非法数值")
    print("TERRAIN_PCG_V2_PROJECTION_INDEPENDENT_ROAD_AND_EXCLUSION_PASS")
    print("PNG_LAYERS=10 PCG_CANDIDATES=1024 EXCLUSION_POINTS=", exclusions)
    print("SOURCE=calibration_only_not_gaea UE_ASSETS_NOT_GENERATED=TRUE")


if __name__ == "__main__":
    verify()
