#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8真实材质及Niagara节点链接回归，防止只验收资产存在而未接到运行图。

归属：Tests/Assets（独立只读验收），只访问同一开发机的Monolith MCP服务。
执行：python Tests/Assets/VerifySnowNativeGraphsMonolith.py --port 9316
前置：正确DivineBeastsArena UE编辑器及Monolith已启动；端口范围仅本机回环。
影响：只读查询材质图、Niagara发射率、参数链接和编译诊断，不更改资源。
限制：图/编译校验不等于客户端/服务器Cook和人工可见的真实积雪画面。
"""
from __future__ import annotations

import argparse
import json
from urllib.error import URLError
from urllib.request import Request, urlopen

SURFACE = "/GamePlatformSurface/Materials/M_GP_SnowCover_Detailed"
NIAGARA = "/GamePlatformVFX/Weather/Niagara/NS_GP_Weather_Snow_Detailed"


def mcp_call(port: int, domain: str, action: str, params: dict) -> dict:
    """通过本机真实MCP服务读取引擎资产；失败时保留错误，不猜测结果。"""
    endpoint = f"http://127.0.0.1:{port}/mcp"
    request = Request(
        endpoint,
        data=json.dumps({
            "jsonrpc": "2.0",
            "id": 1,
            "method": "tools/call",
            "params": {
                "name": f"{domain}_query",
                "arguments": {"action": action, "params": params},
            },
        }).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urlopen(request, timeout=30) as response:
            returned = json.load(response)
    except URLError as exc:
        raise RuntimeError(f"Monolith服务不可达：{endpoint}") from exc

    if returned.get("error"):
        raise RuntimeError(f"{domain}:{action} JSON-RPC失败：{returned['error']}")
    result = returned.get("result", {})
    if result.get("isError"):
        raise RuntimeError(f"{domain}:{action} 引擎拒绝处理：{result}")
    chunks = result.get("content", [])
    if not chunks or "text" not in chunks[0]:
        raise RuntimeError(f"{domain}:{action} 缺少可解码结果")
    decoded = json.loads(chunks[0]["text"])
    if decoded.get("error") or decoded.get("success") is False:
        raise RuntimeError(f"{domain}:{action} 内部错误：{decoded}")
    return decoded


def verify_material(port: int) -> None:
    graph = mcp_call(
        port, "material", "export_material_graph",
        {"asset_path": SURFACE, "include_properties": True, "include_positions": False}
    )
    nodes = {n["id"]: n for n in graph["nodes"]}
    edges = {(edge["from"], edge["to"], edge["to_pin"]) for edge in graph["connections"]}
    if len(nodes) < 56:
        raise RuntimeError("UE雪母材质没有完整56节点升级拓扑")
    constant = nodes.get("MaterialExpressionConstant_0")
    if constant is None or abs(float(constant["props"]["R"]) - 0.5) > 1e-6:
        raise RuntimeError("SnowCoverage基础阈值不是0.5")
    has_shift = any(
        node.get("class") == "ScalarParameter"
        and node["props"].get("ParameterName") == "SnowCoverageShiftScale"
        and abs(float(node["props"].get("DefaultValue", "0.4")) - 0.4) < 1e-4
        for node in nodes.values()
    )
    if not has_shift:
        raise RuntimeError("找不到SnowCoverageShiftScale=0.4")
    for required in (
        ("MaterialExpressionMultiply_6", "MaterialExpressionSubtract_1", "B"),
        ("MaterialExpressionMultiply_7", "MaterialExpressionSaturate_3", "Input"),
        ("MaterialExpressionCollectionParameter_1", "MaterialExpressionMultiply_7", "B"),
    ):
        if required not in edges:
            raise RuntimeError("生产积雪材质缺少计算节点连线：" + str(required))
    print("SNOW_MATERIAL_MASK_GRAPH_PASS", len(nodes), "coverage_factor=SnowAmount")


def verify_niagara(port: int) -> None:
    for emitter, expected in (
        ("NE_GP_Snow_Near", "User.WeatherNearSpawnRate"),
        ("NE_GP_Snow_Far", "User.WeatherFarSpawnRate"),
    ):
        value = mcp_call(
            port, "niagara", "get_module_input_value",
            {"asset_path": NIAGARA, "emitter": emitter, "module_node": "SpawnRate",
             "input": "SpawnRate"},
        )
        if not value.get("is_linked") or value.get("linked_parameter") != expected:
            raise RuntimeError(f"{emitter} 未绑定天气强度参数{expected}")
        print("SNOW_WEATHER_RATE_LINK_OK", emitter, expected)

    drift = "NE_GP_Snow_GroundDrift"
    space = mcp_call(
        port, "niagara", "get_emitter_property",
        {"asset_path": NIAGARA, "emitter": drift, "property": "bLocalSpace"},
    )
    if space.get("value") is not False:
        raise RuntimeError("地面雪粉仍是局部空间，角色移动时可能整片跟随")
    tree = mcp_call(
        port, "niagara", "get_dynamic_input_tree",
        {"asset_path": NIAGARA, "emitter": drift, "module_node": "SpawnRate",
         "max_depth": 4},
    )
    roots = [entry for entry in tree.get("inputs", []) if entry.get("name") == "SpawnRate"]
    if len(roots) != 1 or roots[0].get("source") != "dynamic_input":
        raise RuntimeError("地面雪粉SpawnRate缺少动态输入")
    dyn = roots[0].get("dynamic_input", {})
    if dyn.get("function_name") != "Multiply_Float":
        raise RuntimeError("地面雪粉不是Multiply_Float强度缩放节点")
    inputs = {item["name"]: item for item in dyn.get("inputs", [])}
    if inputs.get("A", {}).get("source") != "linked":
        raise RuntimeError("Multiply_Float未链接天气强度参数")
    if abs(float(inputs.get("B", {}).get("value", "NaN")) - 18.0) > 0.001:
        raise RuntimeError("地面雪粉原始发射率倍率不是18")
    compiled = mcp_call(
        port, "niagara", "get_system_diagnostics",
        {"asset_path": NIAGARA, "compile_first": False},
    )
    if compiled.get("error_count") != 0 or compiled.get("warning_count") != 0:
        raise RuntimeError("雪系统存在编译错误或警告")
    print("SNOW_GROUND_DRIFT_INTENSITY_PASS world_space=true rate=User.WeatherIntensity*18")


def main() -> None:
    parser = argparse.ArgumentParser(description="Snow UE材质与Niagara原生连线验收")
    parser.add_argument("--port", type=int, default=9316, help="本机Monolith监听端口")
    options = parser.parse_args()
    if not (1024 <= options.port <= 65535):
        raise ValueError("Monolith端口范围不合法")
    verify_material(options.port)
    verify_niagara(options.port)
    print("SNOW_NATIVE_GRAPH_VALIDATION_PASS")


if __name__ == "__main__":
    main()
