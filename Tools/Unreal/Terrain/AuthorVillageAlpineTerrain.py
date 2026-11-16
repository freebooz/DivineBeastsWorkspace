"""从 Monolith 执行的现有 Village 地形高程着色扩展。"""

import unreal


MAP = "/DBAWorldPack_Village/Maps/L_Village_Start"
MASTER = "/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge"
LAYOUT = MASTER + "_Layout"
MARKER = "Village alpine elevation masks"
STRENGTH = "VillageAutoAlpineStrength"

# cm 为正式地图当前湖面与最高 Landscape bounds。其余值是相对高度比例。
PARAMETERS = {
    "VillageMountainFootHeightCm": 10500.0,
    "VillageMountainSummitHeightCm": 42963.55,
    "VillageMountainCliffStartFraction": 0.46,
    "VillageMountainCliffFullFraction": 0.54,
    "VillageMountainSnowStartFraction": 0.80,
    "VillageMountainSnowFullFraction": 0.88,
    STRENGTH: 0.0,
}

CONTRACT = """
float h = saturate((HeightCm - FootCm) / max(SummitCm - FootCm, 1.0));
float roadProtection = saturate(max(max(RoadLayer, FarmLayer), RoadMask));
float influence = saturate(Strength) * (1.0 - roadProtection);
float cliff = smoothstep(CliffStart, max(CliffFull, CliffStart + 0.001), h);
float summit = smoothstep(SnowStart, max(SnowFull, SnowStart + 0.001), h);
float retention = smoothstep(0.35, 0.78, saturate(UpDot));
return float2(cliff, summit * retention) * influence;
""".strip()

# 复用当前权威材质的四条岩石 PBR 分支，保留原低处自动和手涂输出。
PBR = (
    ("MaterialExpressionCustom_35", "MaterialExpressionCustom_27", "", "MaterialExpressionStaticSwitchParameter_3"),
    ("MaterialExpressionCustom_36", "MaterialExpressionCustom_28", "", "MaterialExpressionStaticSwitchParameter_4"),
    ("MaterialExpressionCustom_37", "MaterialExpressionCustom_29", "", "MaterialExpressionStaticSwitchParameter_5"),
    ("MaterialExpressionCustom_38", "MaterialExpressionTextureSampleParameter2D_27", "R", "MaterialExpressionStaticSwitchParameter_6"),
)


def inspect_binding():
    """校验实际关卡、地形实例与材质所有权，拒绝在其他地图上写入。"""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world is None or world.get_path_name().split(".")[0] != MAP:
        raise RuntimeError("请先用 Monolith 打开当前正式 Village 地图。")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    landscapes = [actor for actor in actors if actor.get_class().get_name() == "Landscape"]
    if len(landscapes) != 1:
        raise RuntimeError("正式 Village 主 Landscape 身份不唯一。")
    landscape = landscapes[0]
    layout = landscape.get_editor_property("landscape_material")
    if layout is None or layout.get_path_name().split(".")[0] != LAYOUT:
        raise RuntimeError("地形实例已变化，停止图表写入。")
    material = layout.get_editor_property("parent")
    if material is None or material.get_path_name().split(".")[0] != MASTER:
        raise RuntimeError("地形的现有父材质身份不符。")
    if not landscape.get_editor_property("use_dynamic_material_instance"):
        raise RuntimeError("正式地形的天气动态材质合同不符。")
    center, extent = landscape.get_actor_bounds(False)
    if abs(center.z + extent.z - PARAMETERS["VillageMountainSummitHeightCm"]) > 100:
        raise RuntimeError("Landscape 标高已经改动，须重新校准山顶参考。")
    return material, layout


def author():
    """仅扩展原材质图和正式地图实例，保留其它世界、涂层与资源身份。"""
    lib = unreal.MaterialEditingLibrary
    material, layout = inspect_binding()

    def existing(name):
        node = unreal.find_object(material, name)
        if node is None:
            raise RuntimeError("缺少已复核的材质节点 " + name)
        return node

    def create(kind, x, y, label):
        node = lib.create_material_expression(material, kind, x, y)
        if node is None:
            raise RuntimeError("无法创建表达式 " + label)
        node.set_editor_property("desc", label)
        return node

    def connect(source, output, target, pin):
        if lib.connect_material_expressions(source, output, target, pin):
            return
        # UE 的单输出和单输入节点图表可显示标签，而 Python API 使用空名称寻址。
        if output in ("", "XYZ") and pin == "Input" and lib.connect_material_expressions(source, "", target, ""):
            return
        raise RuntimeError(f"无法连接 {source.get_name()} 到 {target.get_name()} 的 {pin}。")

    if STRENGTH in [str(name) for name in lib.get_scalar_parameter_names(material)]:
        return {"material": MASTER, "layout": LAYOUT, "changed": False,
                "enable": lib.get_material_instance_scalar_parameter_value(layout, STRENGTH)}

    dry_paths = existing("MaterialExpressionTextureSampleParameter2D_13")
    road = existing("MaterialExpressionLandscapeLayerSample_3")
    farm = existing("MaterialExpressionLandscapeLayerSample_4")
    for node, name in ((dry_paths, "ForestPathMask"), (road, "LS_RoadDirt"), (farm, "LS_FarmSoil")):
        if str(node.get_editor_property("parameter_name")) != name:
            raise RuntimeError("原有道路与耕地保护合同不符。")
    slope = existing("MaterialExpressionDotProduct_0")
    weather_raw = existing("MaterialExpressionMultiply_5")
    weather_coverage = existing("MaterialExpressionSaturate_3")
    surfaces = [(existing(blend), existing(rock), channel, existing(switch))
                for blend, rock, channel, switch in PBR]

    with unreal.ScopedEditorTransaction("Apply Village alpine terrain material"):
        world_position = create(unreal.MaterialExpressionWorldPosition, 2100, -5600, "Alpine fixed world position")
        world_position.set_editor_property(
            "world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS
        )
        height = create(unreal.MaterialExpressionComponentMask, 2320, -5600, "Alpine world height cm")
        for channel in ("r", "g", "b", "a"):
            height.set_editor_property(channel, channel == "b")
        connect(world_position, "XYZ", height, "Input")

        params = {}
        for index, (name, value) in enumerate(PARAMETERS.items()):
            param = create(unreal.MaterialExpressionScalarParameter, 2100, -5300 + index * 105, name)
            param.set_editor_property("parameter_name", unreal.Name(name))
            param.set_editor_property("default_value", value)
            param.set_editor_property("group", unreal.Name("Village Alpine"))
            params[name] = param
        source_by_pin = {
            "HeightCm": (height, ""),
            "FootCm": (params["VillageMountainFootHeightCm"], ""),
            "SummitCm": (params["VillageMountainSummitHeightCm"], ""),
            "CliffStart": (params["VillageMountainCliffStartFraction"], ""),
            "CliffFull": (params["VillageMountainCliffFullFraction"], ""),
            "SnowStart": (params["VillageMountainSnowStartFraction"], ""),
            "SnowFull": (params["VillageMountainSnowFullFraction"], ""),
            "Strength": (params[STRENGTH], ""),
            "UpDot": (slope, ""),
            "RoadLayer": (road, ""),
            "FarmLayer": (farm, ""),
            "RoadMask": (dry_paths, "R"),
        }
        alpine = create(unreal.MaterialExpressionCustom, 2600, -5300, MARKER)
        alpine.set_editor_property("description", MARKER)
        alpine.set_editor_property("code", CONTRACT)
        alpine.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
        pins = []
        for pin in source_by_pin:
            value = unreal.CustomInput()
            value.set_editor_property("input_name", unreal.Name(pin))
            pins.append(value)
        alpine.set_editor_property("inputs", pins)
        for pin, (node, output) in source_by_pin.items():
            connect(node, output, alpine, pin)

        masks = []
        for index, label in enumerate(("Upper hillside cliff", "Summit snow")):
            mask = create(unreal.MaterialExpressionComponentMask, 2920, -5300 + index * 170, label)
            for channel in ("r", "g", "b", "a"):
                mask.set_editor_property(channel, channel == ("r" if index == 0 else "g"))
            connect(alpine, "", mask, "Input")
            masks.append(mask)
        cliff, snow = masks
        for index, (blend, rock, channel, switch) in enumerate(surfaces):
            weight = create(unreal.MaterialExpressionLinearInterpolate, 3160, -5250 + index * 140, ("Alpine rock BaseColor", "Alpine rock Normal", "Alpine rock Roughness", "Alpine rock AO")[index])
            connect(blend, "", weight, "A")
            connect(rock, channel, weight, "B")
            connect(cliff, "", weight, "Alpha")
            connect(weight, "", switch, "True")

        combined = create(unreal.MaterialExpressionMax, 3160, -4610, "Summit snow and existing weather snow")
        connect(weather_raw, "", combined, "A")
        connect(snow, "", combined, "B")
        connect(combined, "", weather_coverage, "Input")

        lib.recompile_material(material)
        # UE5.8 此写入函数执行更新后仍返回 false，下面以实际参数读回验收。
        lib.set_material_instance_scalar_parameter_value(layout, STRENGTH, 1.0)

    actual = lib.get_material_instance_scalar_parameter_value(layout, STRENGTH)
    if abs(actual - 1.0) > 0.001:
        raise RuntimeError("正式实例的高程着色强度回读失败。")
    return {
        "material": MASTER, "layout": LAYOUT, "changed": True, "enable": actual,
        "map": MAP, "parameters": PARAMETERS,
        "coverage": weather_coverage.get_name(),
        "alpineNode": alpine.get_name(),
    }
