#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V3.0地形配色原生UE5.8资产创建/冷重载/独立地图审核工具。

职责与层级：第三层DBAPresentationPack_Core仅承担十层地形客户端配色衍生表现。
保留40份Poly Haven扫描纹理及平台GamePlatformSurface既有函数；10份V3颜色纹理
从原始BaseColor离线派生，不能创建第二套PCG、服务器地形或材质函数。
运行边界：默认inspect只查签名；apply在UE Editor中只创建新Texture2D/新母材质/
新MI，原函数化材质、正常Village/主竞技场地图以及原权重数据绝不改动。
map_copy、map_bind、map_verify必须依次使用三个彼此独立的UE进程，
避免UE5.8复制UWorld后同一进程直接LoadLevel导致World Memory Leaks崩溃。
人工审核/打包/性能不因为代码或反射检查成功而视为通过。
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CONFIG = ROOT / "Tools/Unreal/Terrain/TerrainV3ColorMapping.json"
MODE = os.getenv("TERRAIN_COLOR_V3_UE_MODE", "inspect").strip().lower()
REPORT = ROOT / "Saved/ArtReview/TerrainColorV3/GenerationManifest.json"
MODES = {"inspect", "apply", "verify", "map_copy", "map_bind", "map_verify"}
STATES = ("Review", "High", "Medium", "Low")


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as source:
        for part in iter(lambda: source.read(1024 * 1024), b""):
            result.update(part)
    return result.hexdigest()


def validate_inputs() -> tuple[dict, list[dict]]:
    if MODE not in MODES:
        raise ValueError("模式只允许inspect/apply/verify/map_copy/map_bind/map_verify")
    config = json.loads(CONFIG.read_text(encoding="utf-8"))
    if config.get("schemaVersion") != 1 or len(config.get("layers", [])) != 10:
        raise RuntimeError("V3层配置不是唯一十层合同")
    original = json.loads((ROOT / config["baselinePBRManifest"]).read_text(encoding="utf-8"))
    palette = json.loads((ROOT / config["referencePalette"]).read_text(encoding="utf-8"))
    if len(original["files"]) != 40 or len(palette["items"]) != 41:
        raise RuntimeError("既有40份PBR或41项V3颜色合同不完整")
    if not REPORT.is_file():
        raise RuntimeError("源颜色派生尚未完成；先运行GenerateTerrainV3ColorSource.ps1")
    report = json.loads(REPORT.read_text(encoding="utf-8-sig"))
    if report["paletteSha256"] != digest(ROOT / config["referencePalette"]):
        raise RuntimeError("主色板已修改，旧派生色纹理必须重新单独评审")
    if report["productionManifestSha256"] != digest(ROOT / config["baselinePBRManifest"]):
        raise RuntimeError("PBR原始来源合同已更改")
    by_layers = {item["layer"]: item for item in report["layers"]}
    palette_by_id = {item["semanticId"]: item for item in palette["items"]}
    keys = [part["layer"] for part in config["layers"]]
    if len(set(keys)) != 10 or set(keys) != set(by_layers):
        raise RuntimeError("十层候选身份重复或与源派生清单不一致")
    result = []
    for item in config["layers"]:
        layer = item["layer"]
        source = next((f for f in original["files"] if f["layer"] == layer and f["role"] == "BaseColor"), None)
        if source is None or item["semanticId"] not in palette_by_id:
            raise RuntimeError("原始PBR或V3颜色语义缺失：" + layer)
        row = by_layers[layer]
        expected_path = config["outputSourceRoot"] + "/T_DBA_Terrain_" + layer + "_BaseColor_V3_Source.png"
        expected_asset = config["outputAssetRoot"] + "/T_DBA_Terrain_" + layer + "_BaseColor_V3"
        if (row["derivedSourcePath"] != expected_path or
            row["futureUnrealAsset"] != expected_asset or
            row["targetSRGB"] != palette_by_id[item["semanticId"]]["referenceSRGBHex"]):
            raise RuntimeError("候选原图路径或颜色不是已批准的源清单：" + layer)
        png = ROOT / row["derivedSourcePath"]
        original_png = ROOT / source["sourcePath"]
        if not png.is_file() or digest(png) != row["derivedSha256"]:
            raise RuntimeError("V3派生原图缺失或被修改：" + layer)
        if not original_png.is_file() or digest(original_png) != source["sha256"]:
            raise RuntimeError("原始扫描PBR贴图已改变：" + layer)
        result.append({"layer": layer, "png": png, "asset": expected_asset, "origin": source["assetPath"]})
    return config, result


def asset_path(obj) -> str:
    return str(obj.get_path_name()).split(".")[0] if obj else ""


def use_unreal_project():
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("仅可在正式UE5.8 Editor中导入/保存.uasset/.umap") from exc
    if not str(unreal.Paths.get_project_file_path()).replace("\\", "/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("正式工程身份错误，拒绝操作UE资产")
    return unreal


def texture_import(unreal, specs: list[dict], apply: bool) -> None:
    lib = unreal.EditorAssetLibrary
    need = []
    tool = unreal.AssetToolsHelpers.get_asset_tools()
    for row in specs:
        if not lib.does_asset_exist(row["asset"]):
            if not apply:
                raise RuntimeError("候选V3纹理尚不存在：" + row["asset"])
            target_dir, name = row["asset"].rsplit("/", 1)
            if not lib.does_directory_exist(target_dir):
                lib.make_directory(target_dir)
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", str(row["png"]))
            task.set_editor_property("destination_path", target_dir)
            task.set_editor_property("destination_name", name)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", False)
            task.set_editor_property("save", False)
            need.append((row, task))
    if need:
        tool.import_asset_tasks([task for _, task in need])
        for row, task in need:
            if not task.get_editor_property("imported_object_paths"):
                raise RuntimeError("UE导入任务未返回真实原生对象：" + row["asset"])
            tex = lib.load_asset(row["asset"])
            if not isinstance(tex, unreal.Texture2D):
                raise TypeError("UE未导入Texture2D：" + row["asset"])
            tex.set_editor_property("srgb", True)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
            if not lib.save_loaded_asset(tex, only_if_is_dirty=False):
                raise RuntimeError("UE导入后保存失败：" + row["asset"])
            print("SSLM_TERRAIN_V3_TEXTURE_SAVED", row["layer"], flush=True)

    for row in specs:
        tex = lib.load_asset(row["asset"])
        if not isinstance(tex, unreal.Texture2D):
            raise RuntimeError("V3 Texture2D缺失：" + row["asset"])
        if (not bool(tex.get_editor_property("srgb")) or
            tex.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_DEFAULT):
            raise RuntimeError("V3贴图不是正确的sRGB基础色：" + row["asset"])
        data = lib.find_asset_data(row["asset"])
        if not data.is_valid() or str(data.get_tag_value("Dimensions") or "") != "2048x2048":
            raise RuntimeError("V3源尺寸非2048x2048：" + row["asset"])
        imported = tex.get_editor_property("asset_import_data")
        source_path = str(imported.get_first_filename()).replace("\\", "/") if imported else ""
        if source_path.lower() != str(row["png"]).replace("\\", "/").lower():
            raise RuntimeError("UE实际导入源文件不是V3派生PNG：" + row["asset"])
    print("SSLM_TERRAIN_V3_NATIVE_TEXTURES_VERIFIED count=10", flush=True)


def mat_samples(unreal, mat):
    edit = unreal.MaterialEditingLibrary
    return {str(node.get_editor_property("parameter_name")): node
            for node in edit.get_material_expressions(mat)
            if isinstance(node, unreal.MaterialExpressionTextureSampleParameter2D)}


def make_material(unreal, config: dict, rows: list[dict], apply: bool) -> None:
    lib = unreal.EditorAssetLibrary
    edit = unreal.MaterialEditingLibrary
    baseline = lib.load_asset(config["originalMaster"])
    if not isinstance(baseline, unreal.Material):
        raise RuntimeError("平台函数化地形母材质未通过真实加载")
    old_samples = mat_samples(unreal, baseline)
    if len(set("Terrain" + r["layer"] + "BaseColor" for r in rows) - set(old_samples)) > 0:
        raise RuntimeError("原十层母材质缺少基础色纹理参数，不可进行衍生")
    # 源母材质须确实调用平台MF及四通道输出，不能在替换色彩时丢失法线等PBR机制。
    calls = [node for node in edit.get_material_expressions(baseline)
             if isinstance(node, unreal.MaterialExpressionMaterialFunctionCall)]
    if len(calls) != 8:
        raise RuntimeError("已有平台函数调用不是8项，先修复原材质合同")
    target = config["candidateMaster"]
    if not lib.does_asset_exist(target):
        if not apply:
            raise RuntimeError("V3真实UE母材质尚未创建：" + target)
        new = lib.duplicate_asset(config["originalMaster"], target)
        if not isinstance(new, unreal.Material):
            raise RuntimeError("UE母材质复制失败：" + target)
        samples = mat_samples(unreal, new)
        for row in rows:
            parameter = "Terrain" + row["layer"] + "BaseColor"
            tex = lib.load_asset(row["asset"])
            samples[parameter].set_editor_property("texture", tex)
            if asset_path(samples[parameter].get_editor_property("texture")) != row["asset"]:
                raise RuntimeError("V3母材质基础色引用修改未生效：" + parameter)
        diagnostics = [str(x) for x in edit.recompile_material(new)]
        if any("error" in text.lower() for text in diagnostics):
            raise RuntimeError("Shader编译错误：" + ";".join(diagnostics))
        if not lib.save_loaded_asset(new, only_if_is_dirty=False):
            raise RuntimeError("V3母材质保存失败")
        print("SSLM_TERRAIN_V3_MASTER_SAVED", target, flush=True)
    derived = lib.load_asset(target)
    if not isinstance(derived, unreal.Material):
        raise RuntimeError("新母材质真实类型不符")
    new_samples = mat_samples(unreal, derived)
    for row in rows:
        key = "Terrain" + row["layer"] + "BaseColor"
        if asset_path(new_samples[key].get_editor_property("texture")) != row["asset"]:
            raise RuntimeError("V3母材质基础色未链接正确：" + key)
    calls_v3 = [node for node in edit.get_material_expressions(derived)
                if isinstance(node, unreal.MaterialExpressionMaterialFunctionCall)]
    if len(calls_v3) != len(calls):
        raise RuntimeError("平台函数化调用在新母材质中缺失")
    # 必须对比其它物理贴图参数，确保Normal/ORM/Height及雪/苔藓覆盖原样复用。
    for name, node in old_samples.items():
        if name in {"Terrain" + row["layer"] + "BaseColor" for row in rows}:
            continue
        other = new_samples.get(name)
        if not other or asset_path(other.get_editor_property("texture")) != asset_path(
                node.get_editor_property("texture")):
            raise RuntimeError("V3误改了Normal、ORM、高度或覆盖贴图：" + name)
    print("SSLM_TERRAIN_V3_MASTER_VERIFIED 10_srgb_basecolors 8_platform_calls normal_orm_height_preserved", flush=True)

    for suffix in STATES:
        source = config["originalMiStem"] + suffix
        output = config["candidateMiStem"] + suffix
        if not lib.does_asset_exist(source):
            raise RuntimeError("缺少旧质量档MI，禁止建立缺失原型：" + source)
        if not lib.does_asset_exist(output):
            if not apply:
                raise RuntimeError("V3质量档MI尚未保存：" + output)
            new_mi = lib.duplicate_asset(source, output)
            if not isinstance(new_mi, unreal.MaterialInstanceConstant):
                raise RuntimeError("UE没有生成真实材质实例：" + output)
            edit.set_material_instance_parent(new_mi, derived)
            edit.update_material_instance(new_mi)
            if not lib.save_loaded_asset(new_mi, only_if_is_dirty=False):
                raise RuntimeError("UE未保存质量档MI：" + output)
            print("SSLM_TERRAIN_V3_MI_SAVED", suffix, flush=True)
        mi = lib.load_asset(output)
        if not isinstance(mi, unreal.MaterialInstanceConstant) or asset_path(
                mi.get_editor_property("parent")) != target:
            raise RuntimeError("V3质量MI父材质错误：" + output)
        # 如果旧MI存在覆盖参数，应暴露错误，而不是悄然继续消费旧Raw基础色。
        for row in rows:
            tex = edit.get_material_instance_texture_parameter_value(
                mi, "Terrain" + row["layer"] + "BaseColor")
            if asset_path(tex) != row["asset"]:
                raise RuntimeError("V3实例仍在使用旧基础色：" + output + "/" + row["layer"])
    print("SSLM_TERRAIN_V3_MATERIAL_VARIANTS_VERIFIED count=4", flush=True)


def review_map(unreal, config: dict) -> None:
    lib = unreal.EditorAssetLibrary
    mode = MODE
    original = config["originalMap"]
    target = config["candidateMap"]
    expected_new = config["candidateMiStem"] + "Review"
    expected_old = config["originalMiStem"] + "Review"
    if not lib.does_asset_exist(original):
        raise RuntimeError("不存在旧独立地形审核地图，不允许编辑正式Village")
    mi = lib.load_asset(expected_new)
    if not isinstance(mi, unreal.MaterialInstanceConstant):
        raise RuntimeError("须先导入UE新质量档MI")
    if mode == "map_copy":
        if lib.does_asset_exist(target):
            raise RuntimeError("审核地图已存在，不做重复复制覆盖")
        new_world = lib.duplicate_asset(original, target)
        if not isinstance(new_world, unreal.World):
            raise RuntimeError("UE没有复制为原生地图")
        if not lib.save_asset(target, only_if_is_dirty=False):
            raise RuntimeError("UE审核地图保存失败")
        print("SSLM_TERRAIN_V3_REVIEW_MAP_COPY_SAVED", target,
              "separate_process_required=true", flush=True)
        return
    if not lib.does_asset_exist(target):
        raise RuntimeError("请先用独立UE进程完成map_copy")
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not editor or not editor.load_level(target):
        raise RuntimeError("UE不能加载独立V3审查地图")
    terrain = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
               if isinstance(actor, unreal.LandscapeProxy)]
    if len(terrain) != 1:
        raise RuntimeError("V3审核关卡必须恰好含一个Landscape")
    before = asset_path(terrain[0].get_editor_property("landscape_material"))
    if mode == "map_bind":
        if before == expected_old:
            terrain[0].set_editor_property("landscape_material", mi)
            if not editor.save_current_level():
                raise RuntimeError("UE未保存V3独立地图")
        elif before != expected_new:
            raise RuntimeError("地图已绑定未知材质，拒绝覆盖：" + before)
    after = asset_path(terrain[0].get_editor_property("landscape_material"))
    if after != expected_new:
        raise RuntimeError("V3地图未实际绑定新MI：" + after)
    if not bool(terrain[0].get_editor_property("use_dynamic_material_instance")):
        raise RuntimeError("原地图动态材质实例属性丢失")
    print("SSLM_TERRAIN_V3_REVIEW_MAP_VERIFIED", mode, target, "landscape=1",
          "formal_village_unchanged=true", flush=True)


def main() -> None:
    config, layers = validate_inputs()
    if MODE == "inspect":
        print("SSLM_TERRAIN_COLOR_V3_PLAN count=10 source=cc0_srgb derived_only=true"
              " no_unreal_map_or_material_written=true", flush=True)
        return
    unreal = use_unreal_project()
    if MODE in {"apply", "verify"}:
        texture_import(unreal, layers, apply=MODE == "apply")
        make_material(unreal, config, layers, apply=MODE == "apply")
    else:
        review_map(unreal, config)
    print("SSLM_TERRAIN_COLOR_V3_UE_DONE mode=" + MODE, flush=True)


if __name__ == "__main__":
    main()
