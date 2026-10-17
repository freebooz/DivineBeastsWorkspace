#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM Gold农田图原生样条来源受控迁移（仅UE5.8编辑器）。

作用域仅为/Game/Development/Foundation/PCG/Realized/PCG_Gold_Crop；
修正Actor复合输入无法被SplineSampler识别导致的零点输出。
不触及正式Village/MainArena地图、客户端运行时、碰撞/网络权威或其他生成图。
inspect和verify只读；apply前须由PowerShell入口执行哈希备份、排他检查。
"""

from __future__ import annotations

import os

PACKAGE = "/Game/Development/Foundation/PCG/Realized/PCG_Gold_Crop"
MODE = os.environ.get("SSLM_GOLD_CROP_SPLINE_MODE", "inspect").strip().lower()


def class_name(obj):
    return obj.get_class().get_name() if obj else ""


def require_enum(ue, enum_name: str, member: str):
    enum_type = getattr(ue, enum_name, None)
    value = getattr(enum_type, member, None) if enum_type else None
    if value is None:
        raise RuntimeError(f"当前UE5.8缺少枚举：{enum_name}.{member}")
    return value


def inspect_graph(ue, graph):
    """读取真实UE图节点/边，拒绝重复或含糊来源，不猜测未保存的连接。"""
    nodes = list(graph.get_editor_property("nodes"))
    sampler_nodes = [n for n in nodes if class_name(n.get_settings()) == "PCGSplineSamplerSettings"]
    source_nodes = [n for n in nodes if class_name(n.get_settings()) == "PCGDataFromActorSettings"]
    if len(sampler_nodes) != 1 or len(source_nodes) > 1:
        raise RuntimeError("Gold Crop必须恰有1个SplineSampler，且最多1个GetActorData")
    sampler = sampler_nodes[0]
    edges = list(graph.get_all_edges())
    # 迁移仅接受当前已审核的Gold农田图：先前已接入密度0过滤器。
    # 输入漂移、缺失Spawner、增删其他节点时失败关闭，不能把未知图当已批准图修复。
    expected = {
        "GamePlatformPCGSpatialCarveSettings": 1,
        "PCGDensityFilterSettings": 1,
        "PCGStaticMeshSpawnerSettings": 1,
        "PCGSplineSamplerSettings": 1,
    }
    for kind, count in expected.items():
        if sum(class_name(n.get_settings()) == kind for n in nodes) != count:
            raise RuntimeError("Gold Crop节点合同漂移：" + kind)
    if len(nodes) != 8 + len(source_nodes) or len(edges) != 9:
        raise RuntimeError("Gold Crop节点/边数量超出已批准的密度过滤后拓扑")
    incoming = [e for e in edges if e.get_output_node() == sampler and
                str(e.get_output_pin_label()) == "Spline"]
    if len(incoming) != 1:
        raise RuntimeError("SplineSampler.Spline输入必须恰有一条真实UPCGEdge")
    return sampler, source_nodes, incoming[0], nodes, edges


def assert_source(ue, source, sampler, edges):
    settings = source.get_settings()
    if class_name(settings) != "PCGDataFromActorSettings":
        raise RuntimeError("已保存的PCG样条源类别不是GetActorData")
    actor_filter = settings.get_editor_property("actor_selector").get_editor_property("actor_filter")
    selector = settings.get_editor_property("component_selector")
    select_mode = selector.get_editor_property("component_selection")
    select_class = selector.get_editor_property("component_selection_class")
    mode = settings.get_editor_property("mode")
    if actor_filter != require_enum(ue, "PCGActorFilter", "SELF"):
        raise RuntimeError("样条源必须只查询Self Actor")
    if select_mode != require_enum(ue, "PCGComponentSelection", "BY_CLASS") or \
            select_class != ue.SplineComponent.static_class():
        raise RuntimeError("样条源必须仅解析SplineComponent类型")
    # UE反射枚举字符串可能带类型和数字，只接受明确ParseActorComponents身份。
    if "PARSE_ACTOR_COMPONENTS" not in str(mode).upper():
        raise RuntimeError("样条源不得切换到PCGComponent数据或其他演员引用模式")
    if not settings.get_editor_property("ignore_pcg_generated_components"):
        raise RuntimeError("必须忽略PCG生成的组件")
    if settings.get_editor_property("always_requery_actors"):
        raise RuntimeError("不允许图执行中重新查询外部Actor")
    if settings.get_editor_property("actor_selector").get_editor_property("include_children"):
        raise RuntimeError("样条源不得扫描子Actor")
    connections = [e for e in edges if e.get_output_node() == sampler and
                   str(e.get_output_pin_label()) == "Spline"]
    if len(connections) != 1 or connections[0].get_input_node() != source or \
            str(connections[0].get_input_pin_label()) != "Out":
        raise RuntimeError("GetActorData.Out必须是SplineSampler.Spline唯一上游")
    return True


def main():
    if MODE not in ("inspect", "apply", "verify"):
        raise ValueError("仅允许inspect/apply/verify三种执行模式")
    print("SSLM_GOLD_CROP_SPLINE_START", MODE, flush=True)
    import unreal as ue  # type: ignore
    project = str(ue.Paths.get_project_file_path()).replace("\\", "/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("当前UE编辑器不是SSLM唯一正式工程")
    if "-SSLMGoldCropSplineMigration" not in str(ue.SystemLibrary.get_command_line()):
        raise RuntimeError("缺少本次迁移的显式开发模式标记")

    graph = ue.EditorAssetLibrary.load_asset(PACKAGE)
    if not isinstance(graph, ue.PCGGraph):
        raise RuntimeError("独立Gold Crop原生图不存在或反射类型错误")

    sampler, sources, edge, nodes, edges = inspect_graph(ue, graph)
    print("SSLM_GOLD_CROP_SPLINE_INPUT", len(nodes), "edges", len(edges),
          "existingSources", len(sources), flush=True)

    if MODE == "inspect":
        print("SSLM_GOLD_CROP_SPLINE_INSPECT_PASS", flush=True)
        return
    if MODE == "verify":
        if len(sources) != 1 or not assert_source(ue, sources[0], sampler, edges):
            raise RuntimeError("Crop实际样条来源未经批准")
        print("SSLM_GOLD_CROP_SPLINE_VERIFY_PASS", flush=True)
        return

    # 修复只能发生在尚未迁移的已保存图；不重复创建节点，也不改写已加载的业务地图。
    if sources:
        raise FileExistsError("Gold Crop已有GetActorData；请改用verify，不可重复执行apply")
    prior_source = edge.get_input_node()
    from_pin = edge.get_input_pin_label()
    to_pin = edge.get_output_pin_label()
    if prior_source is None or prior_source in nodes or str(to_pin) != "Spline":
        raise RuntimeError("旧图必须是Graph Input直接连接SplineSampler；拒绝切断其他处理节点")
    if not graph.remove_edge(prior_source, from_pin, sampler, to_pin):
        raise RuntimeError("无法安全移除Crop旧复合输入连接")
    raw = graph.add_node_of_type(ue.PCGDataFromActorSettings)
    source, settings = raw if isinstance(raw, tuple) else (raw, None)
    if not isinstance(source, ue.PCGNode) or not isinstance(settings, ue.PCGDataFromActorSettings):
        raise RuntimeError("官方GetActorData节点创建失败")
    actor_selector = settings.get_editor_property("actor_selector")
    actor_selector.set_editor_property("actor_filter", require_enum(ue, "PCGActorFilter", "SELF"))
    actor_selector.set_editor_property("include_children", False)
    settings.set_editor_property("actor_selector", actor_selector)
    components = settings.get_editor_property("component_selector")
    components.set_editor_property(
        "component_selection", require_enum(ue, "PCGComponentSelection", "BY_CLASS"))
    components.set_editor_property("component_selection_class", ue.SplineComponent)
    settings.set_editor_property("component_selector", components)
    settings.set_editor_property("ignore_pcg_generated_components", True)
    settings.set_editor_property("always_requery_actors", False)

    graph.add_edge(source, ue.Name("Out"), sampler, ue.Name("Spline"))
    new_edges = list(graph.get_all_edges())
    assert_source(ue, source, sampler, new_edges)
    # 一条旧输入边断开、一条受限GetActorData边新增，最终总边数必须保持不变。
    if len(new_edges) != len(edges):
        raise RuntimeError("Gold Crop迁移后图连接数量不符合替换预期")
    if not ue.EditorAssetLibrary.save_loaded_asset(graph, only_if_is_dirty=False):
        raise RuntimeError("无法保存Gold Crop真实PCG图")
    print("SSLM_GOLD_CROP_SPLINE_REPAIR_PASS", flush=True)
    print("DEVELOPMENT_GOLD_ONLY_NO_VILLAGE_MAP_MUTATION", flush=True)


if __name__ == "__main__":
    main()
