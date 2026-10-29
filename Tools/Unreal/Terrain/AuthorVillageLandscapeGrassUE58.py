# -*- coding: utf-8 -*-
"""SSLM / UE5.8 原生 Landscape Grass 增量作者。

前提：仅在正式 Game/DivineBeastsArena.uproject 的干净 UE 编辑器运行。
创建两份可人工编辑网格的 LandscapeGrassType，向既有 Village 地形母材质
追加独立 LS_Grass -> LandscapeGrassOutput 支路；绝不覆盖现有地表 PBR、
正式关卡、地形高度、碰撞、导航或玩家出生点。
编辑材质前在 Saved 建立 SHA256 可核对回退副本；失败保留证据、不删除资产。
空网格代表待人工选择，不视作已生成可见的草花，也不代表 Cook/GPU 验收。
"""
from __future__ import annotations

import hashlib
import json
import traceback
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).parent
OUT = ROOT / "Saved/Validation/LandscapeGrass20261028"
MATERIAL = "/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge"
ASSET_DIR = "/DBAWorldPack_Village/Terrain/LandscapeGrass/GrassTypes"
GRASS = ASSET_DIR + "/LGT_DBA_Village_Grass"
FLOWERS = ASSET_DIR + "/LGT_DBA_Village_Flowers"
LAYER = "LS_Grass"
REPORT = OUT / "AuthorResult.json"


def preflight():
    """确认主工程、原生 UMaterial 和编辑器尚无其他人的未保存修改。"""
    project = unreal.Paths.get_project_file_path().replace("\\", "/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("仅允许正式 DivineBeastsArena.uproject")
    mat = unreal.load_asset(MATERIAL)
    if not isinstance(mat, unreal.Material):
        raise RuntimeError("正式地形桥材质不存在或类型错误：" + MATERIAL)
    dirty = (unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
             + unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
    if dirty:
        raise RuntimeError("存在未保存包，拒绝写入：" + str(dirty[:8]))
    return mat


def new_variety(density, cull_start, cull_end):
    """参数单位：密度=每10平方米候选实例数；Cull距离=厘米；模型由人工设置。"""
    item = unreal.GrassVariety()
    item.set_editor_property("grass_mesh", None)
    item.set_editor_property("grass_density", unreal.PerPlatformFloat(default=float(density)))
    item.set_editor_property("start_cull_distance", unreal.PerPlatformInt(default=int(cull_start)))
    item.set_editor_property("end_cull_distance", unreal.PerPlatformInt(default=int(cull_end)))
    item.set_editor_property("use_grid", True)
    item.set_editor_property("placement_jitter", 0.65)
    item.set_editor_property("random_rotation", True)
    item.set_editor_property("align_to_surface", True)
    item.set_editor_property("cast_dynamic_shadow", False)
    item.set_editor_property("cast_contact_shadow", False)
    for key in ("scale_x", "scale_y", "scale_z"):
        item.set_editor_property(key, unreal.FloatInterval(min=0.8, max=1.2))
    return item


def create_or_verify_types():
    """已有同名原生资产只读验证；绝不覆盖人选 GrassMesh 或其 Variety 参数。"""
    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    if not assets.does_directory_exist(ASSET_DIR):
        if not assets.make_directory(ASSET_DIR):
            raise RuntimeError("无法建立草花类型目录：" + ASSET_DIR)
    # Commandlet 重开后资产注册表可能仍使用旧扫描缓存。先增量重扫该目录，
    # 并以实际已有 .uasset 磁盘文件为权威存在性条件，严禁重新创建同名资产。
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([ASSET_DIR], force_rescan=True)
    disk_dir = (ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Worlds"
                / "DBAWorldPack_Village/Content/Terrain/LandscapeGrass/GrassTypes")
    result = {}
    for name, path, density, start, end in (
        ("VillageGrass", GRASS, 100.0, 2600, 5800),
        ("VillageFlowers", FLOWERS, 18.0, 1800, 4400),
    ):
        # 仅在磁盘、AssetRegistry 均无记录时创建；对于已保存而本轮尚未索引
        # 的同名包，强制使用对象完整路径加载，不因缓存漏报而覆盖原生文件。
        file_path = disk_dir / (path.rsplit("/", 1)[1] + ".uasset")
        existed = file_path.exists() or assets.does_asset_exist(path)
        asset = unreal.load_asset(path)
        if not isinstance(asset, unreal.LandscapeGrassType) and existed:
            object_name = path.rsplit("/", 1)[1]
            asset = unreal.load_asset(path + "." + object_name)
        if not isinstance(asset, unreal.LandscapeGrassType) and existed:
            raise RuntimeError("已存在原生资产但本次 UE 无法加载，拒绝重新创建：" + path)
        if not existed:
            asset = tools.create_asset(
                path.rsplit("/", 1)[1], ASSET_DIR,
                unreal.LandscapeGrassType, unreal.LandscapeGrassTypeFactory())
        if not isinstance(asset, unreal.LandscapeGrassType):
            raise RuntimeError("GrassType 创建/读取失败：" + path)
        if not existed:
            asset.set_editor_property("enable_density_scaling", True)
            asset.set_editor_property("grass_varieties", [new_variety(density, start, end)])
            if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
                raise RuntimeError("GrassType 保存失败：" + path)
            unreal.log("SSLM_GRASS_TYPE_CREATED " + path)
        varieties = list(asset.get_editor_property("grass_varieties"))
        if not varieties:
            raise RuntimeError("GrassType 缺少可编辑 Variety：" + path)
        if not isinstance(unreal.load_asset(path), unreal.LandscapeGrassType):
            raise RuntimeError("GrassType 保存后回读失败：" + path)
        result[name] = asset
    return result


def find_output(material):
    outputs = [n for n in unreal.MaterialEditingLibrary.get_material_expressions(material)
               if isinstance(n, unreal.MaterialExpressionLandscapeGrassOutput)]
    if len(outputs) > 1:
        raise RuntimeError("已有多份 LandscapeGrassOutput，停止写入以避免竞争")
    return outputs[0] if outputs else None


def verify_output(material, types):
    """强制校验两份输出名称、对象引用和 LS_Grass 读取节点。"""
    output = find_output(material)
    if output is None:
        raise RuntimeError("LandscapeGrassOutput 不存在")
    inputs = list(output.get_editor_property("grass_types"))
    actual = []
    for item in inputs:
        kind = item.get_editor_property("grass_type")
        actual.append((str(item.get_editor_property("name")),
                       kind.get_path_name() if kind else None))
    expected = [(name, types[name].get_path_name()) for name in ("VillageGrass", "VillageFlowers")]
    if actual != expected:
        raise RuntimeError("GrassOutput 输入或资产引用不符：" + repr(actual))
    layer_nodes = [n for n in unreal.MaterialEditingLibrary.get_material_expressions(material)
                   if isinstance(n, unreal.MaterialExpressionLandscapeLayerSample)
                   and str(n.get_editor_property("parameter_name")) == LAYER]
    if not layer_nodes:
        raise RuntimeError("未找到 LS_Grass LayerSample")
    return {"GrassOutputInputs": actual, "GrassLayerNodes": len(layer_nodes)}


def append_output(material, types):
    """仅追加草花生成权重分支，不接触已有天气与地形物理表面连线。"""
    edit = unreal.MaterialEditingLibrary
    if find_output(material):
        info = verify_output(material, types)
        info["MaterialAction"] = "already_wired_verified"
        return info
    src = (ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village"
           / "Content/Materials/Landscape/M_DBA_Village_LandscapeBridge.uasset")
    if not src.is_file():
        raise FileNotFoundError("地形母材质二进制未找到：" + str(src))
    OUT.mkdir(parents=True, exist_ok=True)
    sha = hashlib.sha256(src.read_bytes()).hexdigest()
    backup = OUT / ("LandscapeBridge_BeforeGrass_" + sha[:12] + ".uasset")
    if not backup.exists():
        backup.write_bytes(src.read_bytes())
    if hashlib.sha256(backup.read_bytes()).hexdigest() != sha:
        raise RuntimeError("母材质回退副本 SHA256 不匹配")
    before = edit.get_num_material_expressions(material)

    def add(cls, x, y):
        expr = edit.create_material_expression(material, cls, x, y)
        if not expr:
            raise RuntimeError("无法创建原生材质表达式：" + cls.__name__)
        return expr

    layer = add(unreal.MaterialExpressionLandscapeLayerSample, -1720, 5630)
    layer.set_editor_property("parameter_name", unreal.Name(LAYER))
    layer.set_editor_property("preview_weight", 0.0)
    coverage = add(unreal.MaterialExpressionScalarParameter, -1510, 6130)
    coverage.set_editor_property("parameter_name", unreal.Name("VillageFlowerCoverage"))
    coverage.set_editor_property("default_value", 0.30)
    multiply = add(unreal.MaterialExpressionMultiply, -1240, 6020)
    output = add(unreal.MaterialExpressionLandscapeGrassOutput, -830, 5700)
    # UE5.8 的 FGrassInput 是非 BlueprintType 结构；Python 桥只允许无参构造，
    # 名称和原生 GrassType 关联必须使用反射字段写入，不能采用关键字构造。
    def grass_input(name):
        entry = unreal.GrassInput()
        entry.set_editor_property("name", unreal.Name(name))
        entry.set_editor_property("grass_type", types[name])
        return entry

    output.set_editor_property("grass_types", [
        grass_input(name) for name in ("VillageGrass", "VillageFlowers")
    ])

    def connect(src_node, dest_node, pin):
        if not edit.connect_material_expressions(src_node, "", dest_node, pin):
            raise RuntimeError("连线失败：" + pin)

    connect(layer, output, "VillageGrass")
    connect(layer, multiply, "A")
    connect(coverage, multiply, "B")
    connect(multiply, output, "VillageFlowers")
    edit.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError("真实地形材质保存失败")
    info = verify_output(material, types)
    after = edit.get_num_material_expressions(material)
    if after != before + 4:
        raise RuntimeError("新增节点数量异常：{} -> {}".format(before, after))
    info.update({"MaterialAction": "saved", "NodesBefore": before, "NodesAfter": after,
                 "BackupFile": str(backup), "BackupSHA256": sha})
    unreal.log("SSLM_LANDSCAPE_GRASS_OUTPUT_SAVED " + MATERIAL)
    return info


def open_for_human_selection(types):
    """Commandlet 无 Slate/ContentBrowser，绝不可在其内部调编辑器窗口接口。

    仅确认人工选择目标并返回路径。图形 UnrealEditor.exe 将由单独流程启动；
    否则 UE5.8 的 SyncBrowserToObjects 会在无 SlateApplication 时触发 fatal 断言。
    """
    return {"ContentBrowserSynced": False, "AssetEditorsOpened": False,
            "HumanSelectionPaths": [GRASS, FLOWERS],
            "InteractiveEditorRequired": True}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    state = {"Project": str(unreal.Paths.get_project_file_path()), "Engine": "UE5.8",
             "Material": MATERIAL, "WeightLayer": LAYER,
             "Status": "starting", "HumanSelectionRequired": True,
             "LevelModified": False, "GrassInstancesVerified": False,
             "ClientCookVerified": False, "ServerCookVerified": False}
    try:
        material = preflight()
        types = create_or_verify_types()
        state.update(append_output(material, types))
        state["GrassTypeAssets"] = [GRASS, FLOWERS]
        state["MeshSlots"] = {
            name: [(item.get_editor_property("grass_mesh").get_path_name()
                    if item.get_editor_property("grass_mesh") else None)
                   for item in types[name].get_editor_property("grass_varieties")]
            for name in ("VillageGrass", "VillageFlowers")
        }
        state.update(open_for_human_selection(types))
        state["Status"] = "created_and_wired"
        unreal.log("SSLM_LANDSCAPE_GRASS_AUTHOR_PASS")
    except Exception as error:
        state["Status"] = "failed"
        state["Error"] = repr(error)
        unreal.log_error("SSLM_LANDSCAPE_GRASS_AUTHOR_FAILED " + repr(error))
        traceback.print_exc()
        raise
    finally:
        REPORT.write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding="utf-8")
        unreal.log("SSLM_LANDSCAPE_GRASS_REPORT " + str(REPORT))


if __name__ == "__main__":
    main()
