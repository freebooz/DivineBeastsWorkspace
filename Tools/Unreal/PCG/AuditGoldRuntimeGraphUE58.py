#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""只读分析SSLM平台Gold三份已保存Realized Graph的UE5.8原生节点状态。
不创建Graph、不修改默认值、不保存地图。失败保留真实Python异常而非模拟通过。
本工具仅说明Graph结构，不能替代PCGGenerate/StaticMeshSpawner实例验收。
"""
from __future__ import annotations

import json
from pathlib import Path

GRAPHS = ("Canopy", "Rock", "Crop")
ROOT = "/Game/Development/Foundation/PCG/Realized/PCG_Gold_"
SETTINGS_PROPS = (
    "grid_extents", "cell_size", "point_position", "coordinate_space",
    "b_cull_points_outside_volume", "default_layer_name", "density_scale",
    "uniform_scale", "mesh_set_id", "subject_priority", "exclude_threshold",
    "required_attributes", "b_enabled", "b_is_template",
    "mesh_selector_type", "mesh_selector_parameters", "out_attribute_name",
)


def main() -> None:
    import unreal as ue  # type: ignore

    project = Path(str(ue.Paths.get_project_file_path()))
    if not str(project).replace("\\", "/").endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("真实UE5.8编辑器工程不属于SSLM")
    result = []
    assets = ue.EditorAssetLibrary
    for suffix in GRAPHS:
        path = ROOT + suffix
        if not assets.does_asset_exist(path):
            raise FileNotFoundError("金标准真实生成图缺失：" + path)
        graph = assets.load_asset(path)
        if not isinstance(graph, ue.PCGGraph):
            raise TypeError("资产不是UPCGGraph：" + path)
        nodes = graph.get_editor_property("nodes")
        data = {"path": path, "nodes": [], "edgesCount": len(graph.get_all_edges())}
        for node in nodes:
            settings = node.get_settings()
            if settings is None:
                raise RuntimeError("金标准图包含空节点：" + path)
            kind = settings.get_class().get_name()
            node_data = {"class": kind, "properties": {}}
            for prop in SETTINGS_PROPS:
                try:
                    value = settings.get_editor_property(prop)
                except Exception:
                    continue
                if prop == "mesh_selector_parameters" and value is not None:
                    selector = value
                    node_data["properties"]["meshSelectorClass"] = selector.get_class().get_name()
                    try:
                        entries = selector.get_editor_property("mesh_entries")
                        node_data["properties"]["meshEntriesCount"] = len(entries)
                        node_data["properties"]["meshEntries"] = []
                        for entry in entries:
                            descriptor = entry.get_editor_property("descriptor")
                            mesh = descriptor.get_editor_property("static_mesh")
                            node_data["properties"]["meshEntries"].append({
                                "mesh": str(mesh),
                                "weight": str(entry.get_editor_property("weight"))
                            })
                    except Exception as error:
                        node_data["properties"]["meshInspectError"] = type(error).__name__ + ": " + str(error)
                elif prop in ("required_attributes",):
                    node_data["properties"][prop] = [str(x) for x in value]
                elif prop == "mesh_selector_parameters":
                    continue
                else:
                    node_data["properties"][prop] = str(value)
            data["nodes"].append(node_data)
        result.append(data)
        print("SSLM_GOLD_GRAPH_RUNTIME_INSPECTED", suffix, "nodes", len(nodes), "edges", data["edgesCount"])
        for node in data["nodes"]:
            print("SSLM_GOLD_GRAPH_NODE", suffix, node["class"], json.dumps(node["properties"], ensure_ascii=False))
    report = project.parent.parent / "Saved/Validation/GamePlatformPCG/GoldGraphRuntimeAudit.json"
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps({"status": "NativeGraphReadOnly", "graphs": result}, ensure_ascii=False, indent=2), encoding="utf-8")
    print("SSLM_GOLD_GRAPH_AUDIT_PASS", len(result))


if __name__ == "__main__":
    main()
