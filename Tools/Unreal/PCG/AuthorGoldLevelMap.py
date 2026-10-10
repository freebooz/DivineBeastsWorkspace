#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 PCG Gold Level（程序化内容生成金标准关卡）真实编辑器创作脚本。

默认仅检查目标清单。仅在 PCG_GOLD_MAP_MODE=apply 且正式 UE5.8 Editor 的
PythonScriptPlugin（Python编辑器脚本插件）可用时创建真实 .umap。
它不会修改 Village 正式地图；不绕过平台 WorldDirector 的注册/Schema校验。
场景创建不代表 G01～G16 的实例化/碰撞/导航/双Cook 验收通过。
"""
from __future__ import annotations

import os

MODE = os.environ.get("PCG_GOLD_MAP_MODE", "inspect").strip().lower()
MAP = "/Game/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1"
BASE = "/Game/Development/Foundation/PCG"
BLUEPRINT = "/DBAWorldPack_Village/PCG/Blueprints"
TEMPLATE = BASE + "/Templates"
REALIZED = BASE + "/Realized"
PROFILES = BASE + "/Profiles"
REALIZED_FOR = {"PCG_Canopy": "Canopy", "PCG_Rock": "Rock", "PCG_Crops": "Crop"}
REQUIRED_DEFINITIONS = (
    "DA_PCGGold_Exec",
    "DA_PCGGold_Priority",
    "DA_PCGGold_MeshCanopy",
    "DA_PCGGold_MeshCrop",
    "DA_PCGGold_MeshFence",
    "DA_PCGGold_PolicyForest",
    "DA_PCGGold_PolicyCrop",
    "DA_PCGGold_LayerCanopy",
    "DA_PCGGold_LayerFloor",
    "DA_PCGGold_LayerCrop",
    "DA_PCGGold_Biome",
    "DA_PCGGold_Exclusion",
    "DA_PCGGold_Road",
    "DA_PCGGold_Enclosure",
    "DA_PCGGold_Parcel",
    "DA_PCGGold_Crop",
    "DA_PCGGold_Connector",
)
SPECIFICATIONS = (
    # 蓝图资源名称、放置标签、初始坐标、模板、领域、阶段
    ("BP_PCG_Forest", "PCG_Canopy", (0, 0, 0), "TPL_ScatterSurface", "Forest.Canopy", "SCATTER"),
    ("BP_PCG_Rock", "PCG_Rock", (1100, 700, 0), "TPL_ScatterSurface", "Rock.Scatter", "SCATTER"),
    ("BP_PCG_Resource", "PCG_Resource", (800, -700, 0), "TPL_Base", "Play.Resource", "GAMEPLAY_ANCHORS"),
    ("BP_PCG_Exclusion", "PCG_ManualLock", (-20, 0, 0), "TPL_Base", "Gameplay.Exclusion", "FIELD_READ"),
    ("BP_PCG_Road", "PCG_MajorRoad", (0, 0, 0), "TPL_LinearDresser", "Road.Network", "NETWORKS"),
    ("BP_PCG_Road", "PCG_MinorPath", (0, -500, 0), "TPL_LinearDresser", "Path.Trail", "NETWORKS"),
    ("BP_PCG_Road", "PCG_FieldFence", (650, 300, 0), "TPL_Enclosure", "Encl.FieldFence", "ENCLOSURES"),
    ("BP_PCG_Field", "PCG_Parcel", (700, 300, 0), "TPL_ParcelFill", "Agri.Parcel", "PARCELS"),
    ("BP_PCG_Crops", "PCG_Crops", (700, 320, 0), "TPL_CropField", "Agri.Crop", "SCATTER"),
    ("BP_PCG_Bridge", "PCG_Gate", (650, -150, 0), "TPL_GateInsert", "Gate.Farm", "CONNECTORS"),
    ("BP_PCG_Bridge", "PCG_HandPlacedBridge", (-1100, 0, 0), "TPL_Connector", "Bridge.Span", "CONNECTORS"),
)


def inspect_plan() -> None:
    print("PCG_GOLD_LEVEL_TARGET", MAP)
    print("PCG_GOLD_BLUEPRINT_COUNT", len({row[0] for row in SPECIFICATIONS}))
    for row in SPECIFICATIONS:
        graph = REALIZED + "/PCG_Gold_" + REALIZED_FOR[row[1]] if row[1] in REALIZED_FOR else TEMPLATE + "/" + row[3]
        print("GOLD_ACTOR", row[1], BLUEPRINT + "/" + row[0], graph)
    for name in REQUIRED_DEFINITIONS:
        print("GOLD_REQUIRED_DEFINITION", BASE + "/Definitions/" + name)


def load_dependencies(unreal):
    library = unreal.EditorAssetLibrary
    if library.does_asset_exist(MAP):
        raise FileExistsError("PCG Gold Level地图已存在，不得覆盖：" + MAP)
    missing = []
    required_blueprints = sorted({BLUEPRINT + "/" + row[0] for row in SPECIFICATIONS})
    required_templates = sorted({TEMPLATE + "/" + row[3] for row in SPECIFICATIONS})
    required_definitions = [BASE + "/Definitions/" + name for name in REQUIRED_DEFINITIONS]
    required_realized = [REALIZED + "/PCG_Gold_" + name for name in REALIZED_FOR.values()]
    required_profiles = [PROFILES + "/DA_PCGGold_" + name for name in REALIZED_FOR.values()]
    for asset in required_blueprints + required_templates + required_definitions + required_realized + required_profiles:
        if not library.does_asset_exist(asset):
            missing.append(asset)
    if missing:
        raise FileNotFoundError("PCG真实GoldLevel前置资产尚未通过编辑器创建：" + "，".join(missing))
    cls = getattr(unreal, "GamePlatformPCGWorldDirector", None)
    if cls is None:
        raise RuntimeError("PCG WorldDirector实际编辑器模块未加载，禁止写入不完整.umap")
    return {name: library.load_blueprint_class(BLUEPRINT + "/" + name.split("/")[-1])
            for name in {row[0] for row in SPECIFICATIONS}}


def main():
    inspect_plan()
    if MODE == "inspect":
        print("PCG_GOLD_MAP_INSPECT_ONLY：没有生成或覆盖任何真实UE地图")
        return
    if MODE != "apply":
        raise ValueError("PCG_GOLD_MAP_MODE仅允许inspect或apply")

    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("只能由真实UE5.8编辑器生成.umap，普通Python不能伪造") from exc

    assets = unreal.EditorAssetLibrary
    levels = unreal.EditorLevelLibrary
    blueprint_classes = load_dependencies(unreal)
    # 在new_level写盘之前检查实际PCG阶段枚举和原生Graph绑定接口。
    stage_type = getattr(unreal, "GamePlatformPCGWorldStage", None)
    missing_stages = [stage for *_, stage in SPECIFICATIONS if stage_type is None or getattr(stage_type, stage, None) is None]
    if missing_stages:
        raise RuntimeError("UE5.8未反射下列PCG固定阶段：" + ", ".join(sorted(set(missing_stages))))
    binder = getattr(unreal, "GamePlatformPCGEditorLibrary", None)
    if not callable(getattr(binder, "bind_placed_actor_graph", None)):
        raise RuntimeError("PCG编辑器缺少图与官方组件同步接口；拒绝创建半成品关卡")
    if not callable(getattr(binder, "configure_realized_graph_spatial_masks", None)):
        raise RuntimeError("PCG编辑器缺少空间Mask注入接口；拒绝创建无真实排除的关卡")


    for name, cls in blueprint_classes.items():
        if cls is None:
            raise RuntimeError("已登记的PCG蓝图未产生有效生成类：" + name)

    map_dir = MAP.rsplit("/", 1)[0]
    if not assets.does_directory_exist(map_dir):
        assets.make_directory(map_dir)
    if not levels.new_level(MAP):
        raise RuntimeError("UE无法创建独立PCG GoldLevel地图")
    world = levels.get_editor_world()
    if world is None:
        raise RuntimeError("编辑器无法回读GoldLevel世界")

    def spawn(cls, label, xyz, rotation=None):
        obj = levels.spawn_actor_from_class(
            cls,
            unreal.Vector(*xyz),
            rotation or unreal.Rotator(0.0, 0.0, 0.0),
        )
        if obj is None:
            raise RuntimeError("未能在真实UE世界放置PCG Actor：" + label)
        obj.set_actor_label(label)
        return obj

    director = spawn(unreal.GamePlatformPCGWorldDirector, "PCG_WorldDirector", (0, 0, 0))

    shapes = {
        "PCG_MajorRoad": [(-1900, 0, 0), (1900, 0, 0)],
        "PCG_MinorPath": [(-1800, -500, 0), (1700, -500, 0)],
        "PCG_FieldFence": [(400, 50, 0), (1050, 50, 0), (1050, 850, 0), (400, 850, 0)],
        "PCG_Parcel": [(400, 50, 0), (1050, 50, 0), (1050, 850, 0), (400, 850, 0)],
        "PCG_Crops": [(450, 100, 0), (1000, 100, 0), (1000, 800, 0), (450, 800, 0)],
    }
    actors = []
    for bp, label, position, template, domain, stage_name in SPECIFICATIONS:
        actor = spawn(blueprint_classes[bp], label, position)
        actor.set_editor_property("domain_id", domain)
        stage_type = getattr(unreal, "GamePlatformPCGWorldStage", None)
        stage = getattr(stage_type, stage_name, None) if stage_type else None
        if stage is None:
            raise RuntimeError("UE5.8没有反射对应PCG阶段枚举：" + stage_name)
        actor.set_editor_property("world_stage", stage)
        graph_path = REALIZED + "/PCG_Gold_" + REALIZED_FOR[label] if label in REALIZED_FOR else TEMPLATE + "/" + template
        graph = assets.load_asset(graph_path)
        if graph is None:
            raise RuntimeError("已保存的PCG Graph真实载入失败：" + graph_path)

        # 必须同时更新平台放置器的Graph字段和官方PCGComponent.GraphInstance；
        # 不能只设置SoftObjectPtr，导致地图上存在图路径却没有真正可生成的组件。
        binder = getattr(unreal, "GamePlatformPCGEditorLibrary", None)
        bind_func = getattr(binder, "bind_placed_actor_graph", None) if binder else None
        if not callable(bind_func):
            raise RuntimeError("PCG编辑器模块缺少官方组件安全Graph绑定入口")
        bound = bind_func(actor, graph)
        if (bound[0] if isinstance(bound, tuple) else bound) is not True:
            raise RuntimeError("放置器Graph与PCGComponent绑定失败：" + label + " " + str(bound))
        # 使用实际UE的BoxComponent体积确定候选空间，森林必须与两条道路/农田相交，
        # 否则G01/G02/G03即使PCG节点正确也无法测试空间挖洞，不能用默认小盒子冒充森林。
        volume_sizes = {
            "PCG_Canopy": (1550.0, 950.0, 260.0),
            "PCG_Rock": (420.0, 380.0, 200.0),
            "PCG_Resource": (300.0, 300.0, 160.0),
            "PCG_ManualLock": (230.0, 210.0, 120.0),
        }
        if label in volume_sizes:
            box = actor.get_editor_property("bounds")
            if box is None or not hasattr(box, "set_box_extent"):
                raise RuntimeError("金标准PCG体积缺少可写UBoxComponent：" + label)
            box.set_box_extent(unreal.Vector(*volume_sizes[label]), False)

        if label in shapes:
            key = "boundary" if label in ("PCG_Parcel", "PCG_Crops") else "spline"
            spline = actor.get_editor_property(key)
            if spline is None:
                raise RuntimeError("PCG样条/地块Actor缺少必需原生组件：" + label)
            spline.clear_spline_points(False)
            for point in shapes[label]:
                spline.add_spline_point(
                    unreal.Vector(*point), unreal.SplineCoordinateSpace.WORLD, False
                )
            spline.set_closed_loop(label in ("PCG_Parcel", "PCG_Crops", "PCG_FieldFence"), True)
            spline.update_spline()

        if label == "PCG_ManualLock":
            actor.set_editor_property("carve_priority", 100)
            actor.set_editor_property("strength", 1.0)
        elif label == "PCG_MajorRoad":
            actor.set_editor_property("carve_priority", 70)
            actor.set_editor_property("carve_half_width_cm", 240.0)
        elif label == "PCG_MinorPath":
            actor.set_editor_property("carve_priority", 40)
            actor.set_editor_property("carve_half_width_cm", 125.0)
        elif label == "PCG_FieldFence":
            actor.set_editor_property("carve_priority", 48)
        elif label == "PCG_Parcel":
            actor.set_editor_property("carve_priority", 50)

        if not director.register_participant(actor):
            raise RuntimeError("WorldDirector拒绝注册参与者：" + label)
        actors.append(actor)

    # P2空间来源和注册集合在保存前验证；不在本工具启动官方PCG并假装G01–G16通过。
    checked = director.validate_participant_set()
    valid = checked[0] if isinstance(checked, tuple) else checked
    if not valid:
        raise RuntimeError("PCG WorldDirector注册集合未通过Schema/来源校验：" + str(checked))
    # 只有完整注册且空间边界无非法输入后，才能写入本次独占的三张真实Graph Instance。
    # Forest=30、Rock=15、Crop=20与优先级表对应：道路70可切全部，农田50只切树不切作物。
    # P4地块与P1作物自身共享位置，不允许地块Mask把所有作物删除；项目层只能忽略这两类自掩码。
    spatial = getattr(unreal.GamePlatformPCGEditorLibrary, "configure_realized_graph_spatial_masks", None)
    if not callable(spatial):
        raise RuntimeError("PCG编辑器模块没有真实SpatialMask绑定函数")
    for domain_name, prio, ignored in (
        ("Canopy", 30, []),
        ("Rock", 15, []),
        ("Crop", 20, ["Agri.Parcel", "Agri.Crop"]),
    ):
        path = REALIZED + "/PCG_Gold_" + domain_name
        graph = assets.load_asset(path)
        if graph is None:
            raise RuntimeError("GoldLevel图实例尚未真实保存：" + path)
        result = spatial(graph, director, prio, ignored)
        if (result[0] if isinstance(result, tuple) else result) is not True:
            raise RuntimeError("PCG空间优先级/掩码注入失败：" + path + " " + str(result))
        if not assets.save_loaded_asset(graph, only_if_is_dirty=False):
            raise RuntimeError("PCG真实图实例无法保存空间规则：" + path)
        print("PCG_GOLD_SPATIAL_MASKS_SAVED", path, "priority", prio, "ignored", ignored)

    # 仅展示测试物体用于人工查看，不替代真正Landscape、桥梁碰撞、林业资源和NavMesh。
    plane = assets.load_asset("/Engine/BasicShapes/Plane.Plane")
    if plane is None:
        raise RuntimeError("引擎平面基础网格不可用")
    ground = spawn(unreal.StaticMeshActor, "PCG_GoldLevel_TestGround", (0, 0, -50))
    ground.get_editor_property("static_mesh_component").set_static_mesh(plane)
    ground.set_actor_scale3d(unreal.Vector(40, 40, 1))
    spawn(unreal.DirectionalLight, "PCG_ReviewSun", (0, 0, 1100))
    spawn(unreal.SkyLight, "PCG_ReviewSky", (0, 0, 1050))
    spawn(unreal.PlayerStart, "PCG_ReviewSpawn", (-1600, -1600, 120))

    if not levels.save_current_level():
        raise RuntimeError("真实GoldLevel地图UE保存失败")
    saved = assets.load_asset(MAP)
    if not isinstance(saved, unreal.World):
        raise RuntimeError("GoldLevel保存后无法按UWorld从AssetRegistry回读")
    print("UE_PCG_GOLD_LEVEL_MAP_SAVED", MAP, "registered", len(actors))


if __name__ == "__main__":
    main()
