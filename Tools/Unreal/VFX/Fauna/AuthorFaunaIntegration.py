#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8群落正式Definition/场景蓝图与独立审核地图作者。

在完成新C++编译、重启已有-FaunaOwnedAuthoring作者进程后，由root串行执行本文件。
只写本工具标记的Fauna Definition、Habitat蓝图及/Game/Development/Fauna审核夹具。
不修改Village生产地图，不直接创建个体NiagaraActor，不启动PIE，不运行Cook。
实际保存与原生数据验证、AssetRegistry标签、AssetManager映射全部成功后才写完成报告；
这些检查不代表运行期Data租约/GPU可见/性能或人工艺术验收通过。

运行例：import runpy; runpy.run_path(r"E:/work/2026/DivineBeastsWorkspace/Tools/Unreal/VFX/Fauna/AuthorFaunaIntegration.py", run_name="__main__")
"""
from __future__ import annotations

import json
import math
import re
import traceback
from datetime import datetime, timezone
from pathlib import Path

import unreal

GP = "/GamePlatformVFX/Environment/Fauna"
DBA = "/DBAPresentationPack_Core/VFX/Environment/Fauna"
DEV = "/Game/Development/Fauna"
MAP = DEV + "/Maps/L_DBA_FaunaReview"
EFFECT = GP + "/EffectTypes/NET_GP_Fauna_Ambient"
OWNER_KEY = "FaunaIntegrationAuthor"
OWNER = "20261011.v1"
PREFIX = "FaunaReview_"
SPECIES = ("ButterflyAzure", "FishSilver")

# 单位均为局部厘米。花点坐标就是可见花表面；停栖贴合偏移由独立PerchOffsetCm表示。
# BodyRadius继续用于飞行间距和边界，不移动花点去掩盖足端悬空。
CONFIG = {
    "ButterflyAzure": {
        "count": 32, "seed": 137, "origin": (-780.0, 0.0, 180.0),
        "half_extent": (320.0, 280.0, 160.0), "cruise_speed": 65.0,
        "body_radius": 7.0, "neighbor_radius": 125.0, "separation_radius": 26.0,
        "max_neighbors": 16, "perch_offset_cm": 0.72,
        "flowers": [
            (-190.0, -120.0, -90.0), (-80.0, -170.0, -110.0),
            (70.0, -160.0, -105.0), (190.0, -80.0, -80.0),
            (150.0, 90.0, -100.0), (35.0, 160.0, -115.0),
            (-100.0, 140.0, -100.0), (-190.0, 30.0, -95.0),
        ],
        "obstacles": [((0.0, 10.0, -125.0), 35.0)],
        "water_polygon": [],
    },
    "FishSilver": {
        "count": 64, "seed": 409, "origin": (780.0, 0.0, 140.0),
        "half_extent": (500.0, 300.0, 120.0), "cruise_speed": 95.0,
        "body_radius": 16.0, "neighbor_radius": 170.0, "separation_radius": 48.0,
        "max_neighbors": 16, "perch_offset_cm": 0.72,
        "flowers": [], "obstacles": [((0.0, 0.0, -62.0), 58.0)],
        "water_polygon": [
            (-470.0, -270.0, 0.0), (470.0, -270.0, 0.0),
            (470.0, 270.0, 0.0), (-470.0, 270.0, 0.0),
        ],
    },
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)
    return condition


def package_path(value):
    """读取真实对象/软引用路径；不能用期望路径作为失败回退。"""
    if value is None:
        return ""
    if hasattr(value, "get_path_name"):
        text = value.get_path_name()
    elif hasattr(value, "to_string"):
        text = value.to_string()
    else:
        text = str(value)
    require(text.startswith("/"), "未识别的真实资产引用包装：" + repr(value))
    return text.split(".", 1)[0]


def vector_tuple(value):
    return (float(value.x), float(value.y), float(value.z))


def assert_close(actual, expected, message, tolerance=1.0e-4):
    require(len(actual) == len(expected) and
            all(abs(float(a)-float(b)) <= tolerance for a, b in zip(actual, expected)), message)


def make_struct(cls, properties):
    result = cls()
    for key, value in properties.items():
        result.set_editor_property(key, value)
    return result


def definition_path(species):
    return DBA + "/Definitions/DA_DBA_VFX_Fauna_" + species


def blueprint_path(species):
    return DBA + "/Blueprints/BP_DBA_" + species + "Habitat"


def logical_id(species):
    return "presentation.dba.environment.fauna." + species.lower() + "@1"


def assert_owned(asset, path):
    require(asset is not None, "资产读取失败：" + path)
    require(unreal.EditorAssetLibrary.get_metadata_tag(asset, OWNER_KEY) == OWNER,
            "拒绝改写未标记为本作者拥有的资产：" + path)


def mark_owned(asset):
    unreal.EditorAssetLibrary.set_metadata_tag(asset, OWNER_KEY, OWNER)


def load_or_create(path, cls, factory):
    assets = unreal.EditorAssetLibrary
    if assets.does_asset_exist(path):
        asset = assets.load_asset(path)
        require(isinstance(asset, cls), "目标路径资产类型不符：" + path)
        assert_owned(asset, path)
        return asset
    folder, name = path.rsplit("/", 1)
    assets.make_directory(folder)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, cls, factory)
    require(asset is not None, "创建资产失败：" + path)
    mark_owned(asset)
    return asset


def preflight():
    """先确认真实反射及依赖，过期Editor类不能产生缺Fauna字段的假Definition。"""
    project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path()).replace("\\", "/")
    require(project.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"), "非授权工程：" + project)
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8."), "需要锁定UE5.8")
    require(re.search(r"(?:^|\s)-FaunaOwnedAuthoring(?:\s|$)", unreal.SystemLibrary.get_command_line(), re.IGNORECASE),
            "必须在root已拥有的-FaunaOwnedAuthoring进程内执行")
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(editor.get_game_world() is None, "作者操作前必须结束PIE/Simulate")
    for name in ("GamePlatformVFXWorldDefinition", "GamePlatformFaunaConfig", "GamePlatformFaunaFlower",
                 "GamePlatformFaunaObstacle", "GamePlatformFaunaKind", "DivineBeastsFaunaSceneComponent",
                 "EditorValidatorSubsystem", "DataValidationUsecase", "SubobjectDataBlueprintFunctionLibrary"):
        require(getattr(unreal, name, None) is not None, "新C++反射未加载，先编译并重启作者进程：" + name)
    default_definition = unreal.get_default_object(unreal.GamePlatformVFXWorldDefinition)
    default_definition.get_editor_property("enable_fauna")
    default_definition.get_editor_property("fauna")
    # PerchOffsetCm必须是真实新字段；不回退到BodyRadius或篡改花点。
    unreal.GamePlatformFaunaConfig().get_editor_property("perch_offset_cm")
    component = unreal.get_default_object(unreal.DivineBeastsFaunaSceneComponent)
    for key in ("habitat_id", "species_id", "count", "seed"):
        component.get_editor_property(key)
    require(set(component.get_fauna_species_options()) == set(SPECIES), "项目物种目录选项不符")
    require(hasattr(unreal.SubobjectDataBlueprintFunctionLibrary, "get_object_for_blueprint"),
            "UE5.8缺少蓝图组件模板读取API")
    require(hasattr(unreal.EditorValidatorSubsystem, "is_object_valid"), "未加载原生数据验证API")
    require(hasattr(unreal.SystemLibrary, "get_soft_object_reference_from_primary_asset_id"),
            "未加载真实AssetManager主资产路径包装API")
    assets = unreal.EditorAssetLibrary
    effect = assets.load_asset(EFFECT)
    require(isinstance(effect, unreal.NiagaraEffectType), "Fauna EffectType尚未生成：" + EFFECT)
    dependencies = {}
    for species in SPECIES:
        paths = {"system": DBA + "/Niagara/NS_DBA_" + species,
                 "mesh": DBA + "/Meshes/SM_DBA_" + species,
                 "material": DBA + "/Materials/MI_DBA_" + species}
        values = {key: assets.load_asset(path) for key, path in paths.items()}
        require(isinstance(values["system"], unreal.NiagaraSystem), "真实Niagara缺失：" + paths["system"])
        require(isinstance(values["mesh"], unreal.StaticMesh), "真实网格缺失：" + paths["mesh"])
        require(isinstance(values["material"], unreal.MaterialInstanceConstant), "真实材质缺失：" + paths["material"])
        require(package_path(values["system"].get_editor_property("effect_type")) == EFFECT,
                "物种Niagara与Fauna EffectType不一致：" + species)
        maximum = max(vector_tuple(values["mesh"].get_bounds().box_extent))
        expected = 6.3 if species == "ButterflyAzure" else 11.7996
        require(abs(maximum-expected) < 0.02, "物种模型厘米尺度不符：" + species + " " + str(maximum))
        dependencies[species] = {"objects": values, "paths": paths, "maxExtentCm": maximum}
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    require(not registry.is_loading_assets(), "AssetRegistry尚在发现资产，等待该作者进程空闲后重试")
    root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
    return root, effect, dependencies


def fauna_config(species):
    source = CONFIG[species]
    flowers = [make_struct(unreal.GamePlatformFaunaFlower, {
        "position": unreal.Vector(*point), "normal": unreal.Vector(0.0, 0.0, 1.0), "capacity": 1,
    }) for point in source["flowers"]]
    obstacles = [make_struct(unreal.GamePlatformFaunaObstacle, {
        "center": unreal.Vector(*center), "radius": radius,
    }) for center, radius in source["obstacles"]]
    fields = {key: source[key] for key in ("count", "seed", "cruise_speed", "body_radius",
                                         "neighbor_radius", "separation_radius", "max_neighbors",
                                         "perch_offset_cm")}
    fields.update({
        "kind": unreal.GamePlatformFaunaKind.BUTTERFLY if species == "ButterflyAzure" else unreal.GamePlatformFaunaKind.FISH,
        "half_extent": unreal.Vector(*source["half_extent"]), "flowers": flowers, "obstacles": obstacles,
        "water_polygon": [unreal.Vector(*point) for point in source["water_polygon"]],
    })
    return make_struct(unreal.GamePlatformFaunaConfig, fields)


def author_definition(species, effect, dependencies):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.GamePlatformVFXWorldDefinition)
    asset = load_or_create(definition_path(species), unreal.GamePlatformVFXWorldDefinition, factory)
    identity = make_struct(unreal.GamePlatformId, {
        "namespace": "presentation.dba.environment.fauna", "name": species.lower(), "logical_version": 1,
    })
    schema = unreal.GamePlatformVFXParameterSchema()
    # Seed和Speed属于受控CPU行为覆盖值；Niagara只需暴露Count及三份数组。
    text = ('(MaxOverrideCount=3,Rules=('
            '(Name="User.FaunaCount",Type=Integer,bRequired=False,MinValue=0,MaxValue=512),'
            '(Name="User.FaunaSeed",Type=Integer,bRequired=False,MinValue=0,MaxValue=1000000),'
            '(Name="User.FaunaSpeed",Type=Float,bRequired=False,MinValue=5,MaxValue=600)))')
    require(schema.import_text(text), "Fauna参数Schema解析失败")
    params = make_struct(unreal.GamePlatformVFXParameters, {
        "integer_parameters": {"User.FaunaCount": CONFIG[species]["count"], "User.FaunaSeed": CONFIG[species]["seed"]},
        "float_parameters": {"User.FaunaSpeed": CONFIG[species]["cruise_speed"]},
    })
    asset.set_editor_properties({
        "logical_id": identity, "data_version": unreal.GamePlatformDataVersion(),
        "niagara_system": dependencies[species]["objects"]["system"], "effect_type": effect,
        "content_category": unreal.GamePlatformVFXContentCategory.ENVIRONMENT,
        "auto_destroy": False, "allow_pooling": True, "enable_scalability": True,
        "require_fixed_bounds": True, "max_lifetime_seconds": 0.0,
        "enable_fauna": True, "fauna": fauna_config(species),
        "parameter_schema": schema, "default_parameters": params,
    })
    require(unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False), "Definition保存失败")
    return asset


def registry_tag(data, key):
    # UE包装可能以str/None或(bool,str)返回。两种情况都必须读取实际标签。
    raw = data.get_tag_value(unreal.Name(key))
    if isinstance(raw, tuple):
        require(len(raw) == 2 and raw[0], "AssetRegistry标签缺失：" + key)
        return str(raw[1])
    require(raw is not None, "AssetRegistry标签缺失：" + key)
    return str(raw)


def validate_definition(species, asset):
    expected = logical_id(species)
    identity = asset.get_editor_property("logical_id")
    actual = "{}.{}@{}".format(identity.get_editor_property("namespace"), identity.get_editor_property("name"),
                               identity.get_editor_property("logical_version"))
    require(actual == expected, "Definition逻辑身份不符：" + actual)
    require(asset.get_editor_property("enable_fauna"), "Definition未启用Fauna")
    config = asset.get_editor_property("fauna")
    require(int(config.get_editor_property("count")) == CONFIG[species]["count"], "Definition数量不符")
    require(abs(float(config.get_editor_property("perch_offset_cm"))-0.72) < 1.0e-5, "停栖偏移不符")
    assert_close(vector_tuple(config.get_editor_property("half_extent")), CONFIG[species]["half_extent"], "栖息边界未保存")
    flowers = list(config.get_editor_property("flowers"))
    require(len(flowers) == len(CONFIG[species]["flowers"]), "花点数量未保存")
    for flower, point in zip(flowers, CONFIG[species]["flowers"]):
        assert_close(vector_tuple(flower.get_editor_property("position")), point, "花表面位置未保存")
        require(int(flower.get_editor_property("capacity")) == 1, "花点预约容量未保存")
    require(len(config.get_editor_property("water_polygon")) == len(CONFIG[species]["water_polygon"]), "水域顶点未保存")
    data = unreal.EditorAssetLibrary.find_asset_data(definition_path(species))
    require(data.is_valid(), "已保存Definition未进入AssetRegistry")
    require(registry_tag(data, "GamePlatformLogicalId") == expected, "保存后的LogicalId标签不符")
    validator = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    result = validator.is_object_valid(asset, unreal.DataValidationUsecase.MANUAL)
    require(isinstance(result, tuple) and len(result) == 3,
            "is_object_valid包装签名与本机UE源码不符，需查询__doc__：" + repr(result))
    status, errors, warnings = result
    require(status == unreal.DataValidationResult.VALID and not errors,
            "原生Definition验证失败/未执行：" + repr(result))
    # 源码KismetSystemLibrary的包装实际转发给既有UAssetManager；不能从文件名伪造映射。
    asset_id = unreal.SystemLibrary.get_primary_asset_id_from_object(asset)
    canonical = unreal.SystemLibrary.conv_primary_asset_id_to_string(asset_id)
    require(canonical == "GamePlatformDefinition:" + expected,
            "AssetManager未发现正确主资产ID；核对最新扫描配置和保存通知：" + str(canonical))
    mapped = package_path(unreal.SystemLibrary.get_soft_object_reference_from_primary_asset_id(asset_id))
    require(mapped == definition_path(species), "AssetManager映射未指向该Definition：" + mapped)
    return {"species": species, "definition": definition_path(species), "logicalId": expected,
            "primaryAssetId": canonical, "assetManagerPath": mapped, "nativeValidation": str(status),
            "warnings": [str(item) for item in warnings], "count": CONFIG[species]["count"],
            "flowerSlots": len(flowers), "waterPolygonVertices": len(CONFIG[species]["water_polygon"])}


def blueprint_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    results = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = library.get_data(handle)
        obj = library.get_object_for_blueprint(data, blueprint)
        if isinstance(obj, unreal.DivineBeastsFaunaSceneComponent):
            results.append(obj)
    return results


def author_blueprint(species):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.Actor)
    path = blueprint_path(species)
    blueprint = load_or_create(path, unreal.Blueprint, factory)
    templates = blueprint_templates(blueprint)
    require(len(templates) <= 1, "Habitat蓝图有重复Fauna组件：" + path)
    if not templates:
        sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        handles = sub.k2_gather_subobject_data_for_blueprint(blueprint)
        require(handles, "Habitat蓝图缺少根对象")
        params = unreal.AddNewSubobjectParams(parent_handle=handles[0],
                                              new_class=unreal.DivineBeastsFaunaSceneComponent,
                                              blueprint_context=blueprint)
        handle, reason = sub.add_new_subobject(params=params)
        require(not str(reason), "创建Habitat组件失败：" + str(reason))
        require(sub.rename_subobject(handle=handle, new_name=unreal.Text("Fauna")), "Habitat组件命名失败")
        templates = blueprint_templates(blueprint)
    require(len(templates) == 1, "Habitat组件模板无法唯一读取")
    # 原子写多个字段，避免逐次PostEditChange重建SCS后继续访问失效组件。
    templates[0].set_editor_properties({
        "habitat_id": unreal.Name("None"), "species_id": unreal.Name(species),
        "count": CONFIG[species]["count"], "seed": CONFIG[species]["seed"],
    })
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    klass = unreal.EditorAssetLibrary.load_blueprint_class(path)
    require(klass is not None, "Habitat蓝图未生成可加载类：" + path)
    cdo = unreal.get_default_object(klass)
    cdo.set_editor_properties({"replicates": False, "can_be_damaged": False})
    require(unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False), "Habitat蓝图保存失败")
    templates = blueprint_templates(blueprint)
    require(len(templates) == 1 and str(templates[0].get_editor_property("species_id")) == species,
            "编译后Habitat默认物种未保留")
    require(int(templates[0].get_editor_property("count")) == CONFIG[species]["count"], "编译后Habitat默认数量未保留")
    return klass


def review_material(name, color, roughness=0.7, opacity=1.0):
    """审核夹具材质只归开发目录，不覆盖正式Fauna MI或平台母材质。"""
    path = DEV + "/Materials/M_DBA_FaunaReview_" + name
    material = load_or_create(path, unreal.Material, unreal.MaterialFactoryNew())
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(material)
    material.set_editor_properties({"two_sided": True,
        "blend_mode": unreal.BlendMode.BLEND_TRANSLUCENT if opacity < 1.0 else unreal.BlendMode.BLEND_OPAQUE})
    rgb = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 0)
    rgb.set_editor_property("constant", unreal.LinearColor(*color, 1.0))
    require(lib.connect_material_property(rgb, "", unreal.MaterialProperty.MP_BASE_COLOR), "夹具BaseColor连线失败")
    for value, prop, y in ((roughness, unreal.MaterialProperty.MP_ROUGHNESS, 140),
                           (opacity, unreal.MaterialProperty.MP_OPACITY, 260)):
        node = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -400, y)
        node.set_editor_property("r", value)
        require(lib.connect_material_property(node, "", prop), "夹具材质标量连线失败")
    lib.recompile_material(material)
    require(unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False), "夹具材质保存失败")
    return material


def author_review_map(classes):
    assets = unreal.EditorAssetLibrary
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if assets.does_asset_exist(MAP):
        # 先LoadLevel，由LevelEditor管理世界卸载，不以AssetTools复制UWorld。
        require(levels.load_level(MAP), "无法打开既有Fauna审核地图")
        world = editor.get_editor_world()
        require(package_path(world) == MAP, "切换后的世界不是目标审核地图")
        assert_owned(world, MAP)
    else:
        assets.make_directory(MAP.rsplit("/", 1)[0])
        require(levels.new_level(MAP), "Fauna审核地图创建失败")
        world = editor.get_editor_world()
        mark_owned(world)
        # 尽早保存所有权，失败重跑可识别尚未完成的自有夹具。
        require(levels.save_current_level(), "审核地图所有权保存失败")
    materials = {
        "soil": review_material("Soil", (0.12, 0.055, 0.025)),
        "leaf": review_material("Leaf", (0.055, 0.20, 0.065)),
        "petal": review_material("Petal", (0.58, 0.28, 0.42)),
        "pollen": review_material("Pollen", (0.86, 0.51, 0.06)),
        "rock": review_material("Rock", (0.16, 0.18, 0.20)),
        "water": review_material("Water", (0.025, 0.23, 0.31), 0.18, 0.14),
        "glass": review_material("Glass", (0.09, 0.28, 0.31), 0.10, 0.07),
        "frame": review_material("Frame", (0.09, 0.11, 0.13), 0.45),
    }
    primitives = {name: assets.load_asset("/Engine/BasicShapes/" + name)
                  for name in ("Cube", "Cylinder", "Sphere", "Plane")}
    require(all(isinstance(mesh, unreal.StaticMesh) for mesh in primitives.values()), "引擎审核夹具网格缺失")
    expected_labels = set()

    def spawn(cls, suffix, position, rotation=(0.0, 0.0, 0.0)):
        label = PREFIX + suffix
        expected_labels.add(label)
        matches = [actor for actor in actors.get_all_level_actors() if actor.get_actor_label() == label]
        require(len(matches) <= 1, "审核夹具标签重复：" + label)
        if matches:
            actor = matches[0]
            same_class = isinstance(actor, cls) if isinstance(cls, type) else actor.get_class() == cls
            require(same_class, "审核夹具标签被异类Actor占用：" + label)
            require(unreal.Name(OWNER) in actor.get_editor_property("tags"), "拒绝改写未标记的同名Actor：" + label)
        else:
            actor = actors.spawn_actor_from_class(cls, unreal.Vector(*position),
                unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]))
            require(actor is not None, "审核Actor生成失败：" + label)
            actor.set_actor_label(label)
            actor.set_editor_property("tags", [unreal.Name(OWNER)])
        actor.set_actor_location(unreal.Vector(*position), False, False)
        actor.set_actor_rotation(unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]), False)
        return actor

    def mesh(suffix, shape, position, dimensions, material, rotation=(0.0, 0.0, 0.0)):
        actor = spawn(unreal.StaticMeshActor, suffix, position, rotation)
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        component.set_static_mesh(primitives[shape])
        component.set_material(0, materials[material])
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        # 引擎基本体实际100cm基准；Plane的Z无几何厚度。
        actor.set_actor_scale3d(unreal.Vector(*(value/100.0 for value in dimensions)))
        return actor

    habitats = []
    for species in SPECIES:
        actor = spawn(classes[species], "Habitat_" + species, CONFIG[species]["origin"])
        actor.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
        components = list(actor.get_components_by_class(unreal.DivineBeastsFaunaSceneComponent))
        require(len(components) == 1, "审核Habitat必须恰好一个Fauna组件")
        components[0].set_editor_properties({
            "habitat_id": unreal.Name("DBA.Review.Fauna." + species), "species_id": unreal.Name(species),
            "count": CONFIG[species]["count"], "seed": CONFIG[species]["seed"],
        })
        # 属性编辑可重建SCS，重新取得真实实例后验证。
        components = list(actor.get_components_by_class(unreal.DivineBeastsFaunaSceneComponent))
        require(len(components) == 1, "审核Habitat重建后组件数量错误")
        require(str(components[0].get_editor_property("species_id")) == species, "审核物种写入未生效")
        require(int(components[0].get_editor_property("count")) == CONFIG[species]["count"], "审核数量写入未生效")
        habitats.append({"species": species, "actor": actor.get_actor_label(),
                         "habitatId": "DBA.Review.Fauna." + species,
                         "count": CONFIG[species]["count"], "originCm": CONFIG[species]["origin"],
                         "blueprint": blueprint_path(species)})

    # 花园夹具：花朵的可见表面精确位于Definition的Flower.Position。
    bx, by, bz = CONFIG["ButterflyAzure"]["origin"]
    mesh("GardenBed", "Cube", (bx, by, 0.0), (720.0, 640.0, 40.0), "soil")
    for index, local in enumerate(CONFIG["ButterflyAzure"]["flowers"]):
        x, y, z = bx+local[0], by+local[1], bz+local[2]
        stem_height = z-20.0
        mesh("Flower{:02d}_Stem".format(index), "Cylinder", (x, y, 20.0+stem_height/2.0),
             (1.4, 1.4, stem_height), "leaf")
        for petal in range(5):
            angle = 2.0*math.pi*petal/5.0
            mesh("Flower{:02d}_Petal{}".format(index, petal), "Sphere",
                 (x+5.0*math.cos(angle), y+5.0*math.sin(angle), z-1.0),
                 (11.0, 6.0, 1.8), "petal", (0.0, math.degrees(angle), 0.0))
        # 圆盘顶面z是真花点表面，不以BodyRadius调整场景坐标。
        mesh("Flower{:02d}_Pollen".format(index), "Cylinder", (x, y, z-0.6), (5.0, 5.0, 1.2), "pollen")
    for index, (center, radius) in enumerate(CONFIG["ButterflyAzure"]["obstacles"]):
        mesh("ButterflyObstacle{}".format(index), "Sphere",
             (bx+center[0], by+center[1], bz+center[2]), (radius*2.0,)*3, "rock")

    # 矩形实验水池。水面/水底与局部Z盒边界一致，透明池壁说明这是可见水域夹具。
    fx, fy, fz = CONFIG["FishSilver"]["origin"]
    surface = fz+CONFIG["FishSilver"]["half_extent"][2]
    bed = fz-CONFIG["FishSilver"]["half_extent"][2]
    mesh("WaterBed", "Cube", (fx, fy, bed-10.0), (960.0, 560.0, 20.0), "soil")
    mesh("WaterSurface", "Plane", (fx, fy, surface), (940.0, 540.0, 100.0), "water")
    for side, sign in (("West", -1.0), ("East", 1.0)):
        mesh("WaterGlass_" + side, "Cube", (fx+sign*470.0, fy, fz), (1.0, 540.0, 240.0), "glass")
    for side, sign in (("South", -1.0), ("North", 1.0)):
        mesh("WaterGlass_" + side, "Cube", (fx, fy+sign*270.0, fz), (940.0, 1.0, 240.0), "glass")
    for xi in (-1.0, 1.0):
        for yi in (-1.0, 1.0):
            mesh("WaterFrame_{}_{}".format(int(xi), int(yi)), "Cube",
                 (fx+xi*470.0, fy+yi*270.0, fz), (4.0, 4.0, 248.0), "frame")
    for index, (center, radius) in enumerate(CONFIG["FishSilver"]["obstacles"]):
        mesh("FishObstacle{}".format(index), "Sphere",
             (fx+center[0], fy+center[1], fz+center[2]), (radius*2.0,)*3, "rock")

    sun = spawn(unreal.DirectionalLight, "Sun", (0.0, -1000.0, 3000.0), (-45.0, 25.0, 0.0))
    sun.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("intensity", 5.0)
    spawn(unreal.SkyAtmosphere, "Atmosphere", (0.0, 0.0, 0.0))
    sky = spawn(unreal.SkyLight, "SkyLight", (0.0, 0.0, 1500.0))
    sky.get_component_by_class(unreal.SkyLightComponent).set_editor_property("intensity", 0.8)
    mesh("Floor", "Plane", (0.0, 0.0, -22.0), (3400.0, 2100.0, 100.0), "frame")
    cameras = [
        ("Camera_Overview", (0.0, -2600.0, 1550.0), (-27.0, 90.0, 0.0), 58.0, True),
        ("Camera_Butterfly", (bx, -620.0, 310.0), (-17.0, 90.0, 0.0), 48.0, False),
        ("Camera_Fish", (fx, -730.0, 400.0), (-21.0, 90.0, 0.0), 48.0, False),
    ]
    for suffix, position, rotation, fov, primary in cameras:
        camera = spawn(unreal.CameraActor, suffix, position, rotation)
        camera.set_editor_property("auto_activate_for_player",
                                   unreal.AutoReceiveInput.PLAYER0 if primary else unreal.AutoReceiveInput.DISABLED)
        camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(fov)
    spawn(unreal.PlayerStart, "PlayerStart", (0.0, -2100.0, 200.0), (0.0, 90.0, 0.0))
    world.get_world_settings().set_editor_property("default_game_mode", unreal.GameModeBase)
    require(levels.save_current_level(), "审核地图保存失败")
    # 不删除其他Actor；任何旧版本自有夹具残留须显式评估，避免悄悄遗留重复表现。
    actual_owned = {a.get_actor_label() for a in actors.get_all_level_actors()
                    if unreal.Name(OWNER) in a.get_editor_property("tags")}
    require(actual_owned == expected_labels, "审核夹具标签集合不一致：" + repr(actual_owned ^ expected_labels))
    require(not any(isinstance(a, unreal.NiagaraActor) for a in actors.get_all_level_actors()),
            "审核地图不应有绕过正式VFX执行链的NiagaraActor")
    return {"map": MAP, "habitats": habitats, "ownedActorCount": len(actual_owned),
            "flowerSurfaces": len(CONFIG["ButterflyAzure"]["flowers"]),
            "waterSurfaceZCm": surface, "waterBedZCm": bed,
            "cameras": [PREFIX+item[0] for item in cameras], "runtimePath": "SceneComponent->Presentation->Data->VFX->Niagara"}


def main():
    root, effect, dependencies = preflight()
    out = root / "Saved/Validation/Fauna/Integration"
    out.mkdir(parents=True, exist_ok=True)
    report = {"status": "running", "startedUtc": datetime.now(timezone.utc).isoformat(),
              "owner": OWNER, "definitions": [], "blueprints": [], "review": None,
              "villageMap": "/DBAWorldPack_Village/Maps/L_Village_Start",
              "villagePlacement": "deferred_no_verified_flower_surface_or_water_body",
              "runtimeAcceptance": "pending", "manualArtAcceptance": "pending",
              "cookStageAcceptance": "pending"}
    try:
        definitions = {species: author_definition(species, effect, dependencies) for species in SPECIES}
        for species in SPECIES:
            report["definitions"].append(validate_definition(species, definitions[species]))
        classes = {}
        for species in SPECIES:
            classes[species] = author_blueprint(species)
            report["blueprints"].append({"asset": blueprint_path(species), "class": classes[species].get_path_name()})
        report["review"] = author_review_map(classes)
        # 地图切换后重新核对主资产管理映射，排除仅靠旧世界临时对象成功的假象。
        for species in SPECIES:
            loaded = unreal.EditorAssetLibrary.load_asset(definition_path(species))
            validate_definition(species, loaded)
        report["status"] = "authored_and_validated"
        report["finishedUtc"] = datetime.now(timezone.utc).isoformat()
        print("FAUNA_INTEGRATION_AUTHORED", MAP, "definitions=2 blueprints=2 counts=32/64")
        print("FAUNA_RUNTIME_AND_ART_REVIEW_PENDING")
    except Exception:
        report["status"] = "failed"
        report["error"] = traceback.format_exc()
        raise
    finally:
        (out / "FaunaIntegrationAuthoring.json").write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    main()
