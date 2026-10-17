#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 Gold Realized图中SpatialCarve后的零密度点安全过滤修复。

仅允许修改三份Development/Foundation/PCG/Realized测试资产，
在命令行Mode=apply且原有图节点拓扑得到精确验证后插入官方DensityFilter。
UE5.8的WeightedMeshSelector不会读取点密度来决定是否生成：必须真实删除
SpatialCarve已置零的点。图外资源、正式村庄地图和网络权威均不受此操作影响。
外部脚本应在编辑器启动前备份三份.uasset，并在保存后通过独立UE进程验证。
"""
from __future__ import annotations

import os
from pathlib import Path

ROOT = "/Game/Development/Foundation/PCG/Realized"
NAMES = ("Canopy", "Rock", "Crop")
MODE = os.environ.get("SSLM_GOLD_DENSITY_REPAIR_MODE", "inspect").strip().lower()


def expected_graphs(ue):
    """先全部读取并审查，阻止任意图、重复节点或结构漂移进入修改阶段。"""
    assets = ue.EditorAssetLibrary
    result = []
    for name in NAMES:
        path = ROOT + "/PCG_Gold_" + name
        graph = assets.load_asset(path)
        if not isinstance(graph, ue.PCGGraph):
            raise RuntimeError("Gold真实图缺失或类型非法：" + path)
        nodes = list(graph.get_editor_property("nodes"))
        carvings = [x for x in nodes if x.get_settings().get_class().get_name() ==
                    "GamePlatformPCGSpatialCarveSettings"]
        filters = [x for x in nodes if x.get_settings().get_class().get_name() ==
                   "PCGDensityFilterSettings"]
        spawners = [x for x in nodes if x.get_settings().get_class().get_name() ==
                    "PCGStaticMeshSpawnerSettings"]
        if len(carvings) != 1 or len(spawners) != 1 or len(filters) > 1:
            raise RuntimeError("Gold密度过滤修复不接受非标准节点集合：" + path)
        edges = list(graph.get_all_edges())
        outgoing = [e for e in edges if e.get_input_node() == carvings[0]]
        if len(outgoing) != 1:
            raise RuntimeError("Gold SpatialCarve必须恰有一条下游节点边：" + path)
        to_node = outgoing[0].get_output_node()
        if to_node is None:
            raise RuntimeError("Gold SpatialCarve后继无效：" + path)
        edge = outgoing[0]
        result.append({
            "name": name, "path": path, "graph": graph, "carve": carvings[0],
            "target": to_node, "from_pin": edge.get_input_pin_label(),
            "to_pin": edge.get_output_pin_label(),
            "filter": filters[0] if filters else None,
            "original_nodes": len(nodes), "original_edges": len(edges),
        })
    return result


def filter_valid(node) -> bool:
    """确认过滤器会删除密度0点，且不对剩余点重新归一化。"""
    if node is None:
        return False
    s = node.get_settings()
    return (
        s.get_class().get_name() == "PCGDensityFilterSettings" and
        abs(float(s.get_editor_property("lower_bound")) - 0.001) < 0.000001 and
        abs(float(s.get_editor_property("upper_bound")) - 1.0) < 0.000001 and
        not s.get_editor_property("invert_filter") and
        not s.get_editor_property("normalize_output_density") and
        not s.get_editor_property("keep_zero_density_points")
    )


def main() -> None:
    print("SSLM_GOLD_DENSITY_REPAIR_START", MODE, flush=True)
    if MODE not in ("inspect", "apply", "verify"):
        raise ValueError("Gold修复模式只允许inspect/apply/verify")
    import unreal as ue  # type: ignore
    if not str(ue.Paths.get_project_file_path()).replace("\\", "/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("当前UE5.8编辑器非SSLM唯一工程")
    if "-SSLMGoldDensityRepair" not in ue.SystemLibrary.get_command_line():
        raise RuntimeError("未提供独立开发修复批准标识")
    graphs = expected_graphs(ue)
    if MODE == "inspect":
        for entry in graphs:
            print("SSLM_GOLD_DENSITY_INSPECT", entry["name"],
                  "nodes", entry["original_nodes"],
                  "hasFilter", entry["filter"] is not None, flush=True)
        print("SSLM_GOLD_DENSITY_INSPECT_PASS", len(graphs), flush=True)
        return
    if MODE == "verify":
        for entry in graphs:
            if entry["filter"] is None or not filter_valid(entry["filter"]):
                raise RuntimeError("Gold图没有合格的零密度点过滤器：" + entry["path"])
            # 拒绝将SpatialCarve绕过过滤器直接接到后续Spawner链。
            edges = list(entry["graph"].get_all_edges())
            upstream = [e for e in edges if e.get_input_node() == entry["carve"]]
            if len(upstream) != 1 or upstream[0].get_output_node() != entry["filter"]:
                raise RuntimeError("Gold空间掩码未真正接入DensityFilter：" + entry["path"])
            print("SSLM_GOLD_DENSITY_GRAPH_OK", entry["name"], flush=True)
        print("SSLM_GOLD_DENSITY_VERIFY_PASS", len(graphs), flush=True)
        return
    if any(entry["filter"] is not None for entry in graphs):
        raise FileExistsError("Gold过滤器已存在或只有部分已存在，禁止重复增量改写")
    for entry in graphs:
        g = entry["graph"]
        start = entry["carve"]
        target = entry["target"]
        if not g.remove_edge(start, entry["from_pin"], target, entry["to_pin"]):
            raise RuntimeError("Gold源图的旧引脚断连失败：" + entry["path"])
        result = g.add_node_of_type(ue.PCGDensityFilterSettings)
        node, settings = result if isinstance(result, tuple) else (result, None)
        if not isinstance(node, ue.PCGNode) or not isinstance(settings, ue.PCGDensityFilterSettings):
            raise RuntimeError("原生DensityFilter节点创建失败：" + entry["path"])
        settings.set_editor_property("lower_bound", 0.001)
        settings.set_editor_property("upper_bound", 1.)
        settings.set_editor_property("invert_filter", False)
        settings.set_editor_property("normalize_output_density", False)
        settings.set_editor_property("keep_zero_density_points", False)
        if not filter_valid(node):
            raise RuntimeError("UE5.8过滤器数值配置异常：" + entry["path"])
        g.add_edge(start, entry["from_pin"], node, ue.Name("In"))
        g.add_edge(node, ue.Name("Out"), target, entry["to_pin"])
        edges = list(g.get_all_edges())
        actual = [e for e in edges if e.get_input_node() == start]
        if (len(actual) != 1 or actual[0].get_output_node() != node or
                not any(e.get_input_node() == node and e.get_output_node() == target for e in edges)):
            raise RuntimeError("UE5.8新节点未建立实际双向UPCGEdge连线：" + entry["path"])
        if not ue.EditorAssetLibrary.save_loaded_asset(g, only_if_is_dirty=False):
            raise RuntimeError("Gold修复资产保存失败：" + entry["path"])
        print("SSLM_GOLD_DENSITY_GRAPH_REPAIRED", entry["name"], flush=True)
    print("SSLM_GOLD_DENSITY_REPAIR_PASS", len(graphs), flush=True)
    print("GOLD_DEVELOPMENT_ONLY_NO_VILLAGE_WORLD_MUTATION", flush=True)


if __name__ == "__main__":
    main()
