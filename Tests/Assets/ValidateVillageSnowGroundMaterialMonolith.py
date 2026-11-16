#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""只读求值真实Village材质的雪覆盖连线，验证大雪遮住底色、小雪露地及坡度响应。

调用方为本机资产审核；输入来自Monolith导出的当前原生图及原覆盖PNG。
不是Shader/GPU或人工验收，不创建材质、不加载地图、不改变服务器天气。
"""
import argparse
import importlib.util
import json
import math
import re
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("snow_native_query", Path(__file__).with_name("VerifySnowNativeGraphsMonolith.py"))
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)


def evaluate(graph, noise, amount, slope_degrees=0):
    """沿实际BaseColor最终混合Alpha求值，仅支持覆盖支路真实使用的纯数学节点。"""
    nodes = {n["id"]: n for n in graph["nodes"]}
    links = {(e["to"], e["to_pin"]): e["from"] for e in graph["connections"]}
    output = next(o["from"] for o in graph["outputs"] if o["to_property"] == "BaseColor")
    alpha = links[(output, "Alpha")]
    cache = {}

    def value(identity):
        if identity in cache:
            return cache[identity]
        node = nodes[identity]
        kind, props = node["class"], node.get("props", {})

        def arg(pin, default=0):
            key = links.get((identity, pin))
            return value(key) if key else float(props.get("Const" + pin, default))

        if kind == "ScalarParameter":
            result = amount if props["ParameterName"] == "VillageSnowAmount" else float(props["DefaultValue"])
        elif kind == "Constant":
            result = float(props["R"])
        elif kind == "TextureSampleParameter2D" and props["ParameterName"] == "VillageSnowCoverageTexture":
            result = noise
        elif kind == "DotProduct":
            # 当前覆盖支路的实际点积必须为几何法线与向上向量。
            assert nodes[links[(identity, "A")]]["class"] == "VertexNormalWS"
            up = nodes[links[(identity, "B")]]["props"]["Constant"]
            assert "B=1.000000" in up and "R=0.000000" in up
            result = math.cos(math.radians(slope_degrees))
        elif kind == "ComponentMask" and props.get("B") == "True":
            assert nodes[links[(identity, "Input")]]["class"] == "WorldPosition"
            result = 0.0  # 标准海拔水平测试面，不能将海拔误作厚度。
        elif kind == "Multiply":
            result = arg("A", 1) * arg("B", 1)
        elif kind == "Add":
            result = arg("A") + arg("B")
        elif kind == "Subtract":
            result = arg("A", 1) - arg("B", 1)
        elif kind == "Divide":
            result = arg("A", 1) / arg("B", 2)
        elif kind == "Power":
            result = np.maximum(arg("Base"), 0) ** arg("Exponent", 2)
        elif kind == "Saturate":
            result = np.clip(arg("Input"), 0, 1)
        else:
            raise AssertionError("覆盖支路存在未审核的节点: " + kind + " " + identity)
        cache[identity] = result
        return result

    return np.broadcast_to(value(alpha), noise.shape)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=9316)
    parser.add_argument("--report", type=Path)
    opts = parser.parse_args()
    graph = native.mcp_call(opts.port, "material", "export_material_graph", {
        "asset_path": "/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge",
        "include_properties": True, "include_positions": False})
    source = ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/SourceArt/Snow/T_GP_Snow_CoverageNoise_Source.png"
    with Image.open(source) as img:
        noise = np.asarray(img, dtype=np.float32)[::4, ::4] / 255.0
    amounts = [0, .23, .5, .85, 1]
    masks = [evaluate(graph, noise, a) for a in amounts]
    assert np.all(masks[0] == 0), "零雪量必须完全恢复底层地表"
    assert .10 < masks[1].mean() < .40, "小雪需要薄覆盖并保留露地"
    assert masks[3].mean() > .96, "大雪覆盖仍过于透明，底层草色透出"
    assert (masks[3] > .95).mean() > .90, "大雪平地需要大面积实覆"
    for a, b in zip(masks, masks[1:]):
        assert np.all(b >= a - 1e-6), "雪量增加不得局部退雪"
    assert evaluate(graph, noise, .85, 25).mean() > evaluate(graph, noise, .85, 50).mean()
    assert evaluate(graph, noise, .85, 90).max() < 1e-5, "垂直面不能均匀挂雪"
    report = {"nativeAsset": graph["asset_path"], "means": dict(zip(map(str, amounts), [float(m.mean()) for m in masks])),
              "passed": True, "boundary": "原生连线数值验收，不等于GPU或人工视觉验收"}
    if opts.report:
        opts.report.parent.mkdir(parents=True, exist_ok=True)
        opts.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print("VILLAGE_SNOW_NATIVE_COVERAGE_PASS", json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
