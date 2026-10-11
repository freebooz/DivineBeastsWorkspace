#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 Village共享中性地形母材质与单张正式新手村地图装配。

第一层平台Surface仍拥有高清雪纹理/MPC；第三层Village世界包只拥有极小中性默认纹理、
不硬引用任何纯客户端插件的Material；客户端由DivineBeastsVillageSnowWorldSubsystem将
平台纹理异步写入Landscape MID。此工具禁止在正式地图外的资源上更改任何资产。
默认inspect只读。VILLAGE_LANDSCAPE_AUTHOR_MODE=material创建并保存母材质；
=map只编辑真实L_Village_Start中的LandscapeMaterial和bUseDynamicMaterialInstance；
=all串行执行两阶段。已存在合法资产跳过，拒绝覆盖其他人的地形材质。
实际UE资产必须保存/反射回读，禁止伪造.uasset和.umap。
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = Path(__file__).with_name("VillageLandscapeBridgeSourceManifest.json")
MODE = os.getenv("VILLAGE_LANDSCAPE_AUTHOR_MODE", "inspect").strip().lower()
CONTENT = "/DBAWorldPack_Village/Materials/Landscape"
MASTER = CONTENT + "/M_DBA_Village_LandscapeBridge"
MAP = "/DBAWorldPack_Village/Maps/L_Village_Start"


def preflight() -> list[dict]:
    if MODE not in ("inspect", "material", "map", "all", "verify"):
        raise ValueError("VILLAGE_LANDSCAPE_AUTHOR_MODE仅允许inspect/material/map/all/verify")
    source = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if source.get("schemaVersion") != 1 or len(source.get("textures", [])) != 3:
        raise RuntimeError("地形中性纹理清单格式不正确")
    for rec in source["textures"]:
        src = (ROOT / rec["sourcePath"]).resolve()
        if not src.is_relative_to(ROOT) or not src.is_file():
            raise RuntimeError("地形中性纹理源文件不可用或越过工作空间：" + str(src))
        if hashlib.sha256(src.read_bytes()).hexdigest() != rec["sha256"]:
            raise RuntimeError("地形中性纹理SHA256异常：" + str(src))
        if not rec.get("assetPath", "").startswith(CONTENT + "/Defaults/"):
            raise RuntimeError("地形默认Texture2D不属于Village挂载点")
    return source["textures"]


def create_assets(unreal, records: list[dict]) -> None:
    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.MaterialEditingLibrary
    defaults = {}
    pending = []
    # 永不覆盖现有平台雪资源或他人材质；仅三张有生成器身份的WorldPack默认贴图。
    for rec in records:
        dst = rec["assetPath"]
        folder, name = dst.rsplit("/", 1)
        if assets.does_asset_exist(dst):
            obj = assets.load_asset(dst)
            if not isinstance(obj, unreal.Texture2D):
                raise TypeError("已存在同名非Texture2D资源：" + dst)
            defaults[rec["mode"]] = obj
            continue
        if not assets.does_directory_exist(folder):
            assets.make_directory(folder)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(ROOT / rec["sourcePath"]))
        task.set_editor_property("destination_path", folder)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", False)
        pending.append((rec, task))
    if pending:
        tools.import_asset_tasks([task for _, task in pending])
    compression = {
        "Default": unreal.TextureCompressionSettings.TC_DEFAULT,
        "Masks": unreal.TextureCompressionSettings.TC_MASKS,
        "Normalmap": unreal.TextureCompressionSettings.TC_NORMALMAP,
    }
    for rec, task in pending:
        if not task.get_editor_property("imported_object_paths"):
            raise RuntimeError("UE纹理导入失败：" + rec["assetPath"])
        obj = assets.load_asset(rec["assetPath"])
        if not isinstance(obj, unreal.Texture2D):
            raise TypeError("UE未生成真实Texture2D：" + rec["assetPath"])
        obj.set_editor_property("srgb", bool(rec["sourceSRGB"]))
        obj.set_editor_property("compression_settings", compression[rec["recommendedCompression"]])
        if not assets.save_loaded_asset(obj, only_if_is_dirty=False):
            raise RuntimeError("默认地形纹理保存失败：" + rec["assetPath"])
        defaults[rec["mode"]] = assets.load_asset(rec["assetPath"])
        print("VILLAGE_LANDSCAPE_TEXTURE_CREATED", rec["assetPath"])
    if len(defaults) != 2 or not all(assets.does_asset_exist(r["assetPath"]) for r in records):
        raise RuntimeError("默认地形纹理通道不完整")
    # 两份RGB纹理分别按AssetPath尾部区分，不允许Normal与Albedo使用相同默认纹理。
    tex = {Path(r["sourcePath"]).stem.removesuffix("_Source"): assets.load_asset(r["assetPath"]) for r in records}
    for obj in tex.values():
        if not isinstance(obj, unreal.Texture2D):
            raise TypeError("材质依赖Texture2D无法回读")
    if assets.does_asset_exist(MASTER):
        existing = assets.load_asset(MASTER)
        if (not isinstance(existing, unreal.Material) or
                lib.get_num_material_expressions(existing) < 40):
            raise RuntimeError("地形母材质路径已被非本方案资源占用，拒绝覆盖")
        print("VILLAGE_LANDSCAPE_MASTER_EXISTS", MASTER)
        return
    folder, name = MASTER.rsplit("/", 1)
    if not assets.does_directory_exist(folder):
        assets.make_directory(folder)
    mat = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(mat, unreal.Material):
        raise RuntimeError("真实Material未能创建：" + MASTER)

    def node(cls, x, y):
        expression = lib.create_material_expression(mat, cls, x, y)
        if expression is None:
            raise RuntimeError("UE无法创建MaterialExpression：" + str(cls))
        return expression

    def wire(left, right, pin, out=""):
        ok = lib.connect_material_expressions(left, out, right, pin)
        if not ok and pin in ("Input", "Coordinates"):
            if pin == "Coordinates":
                ok = lib.connect_material_expressions(left, out, right, "UVs")
            if not ok:
                ok = lib.connect_material_expressions(left, out, right, "")
        if not ok:
            raise RuntimeError("UE材质图无法连线：" + left.get_name() + "->" + right.get_name() + "." + pin)

    def val(value, x, y):
        n = node(unreal.MaterialExpressionConstant, x, y)
        n.set_editor_property("r", float(value))
        return n

    def color(rgb, x, y):
        n = node(unreal.MaterialExpressionConstant3Vector, x, y)
        n.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
        return n

    def scalar(name, default, x, y):
        n = node(unreal.MaterialExpressionScalarParameter, x, y)
        n.set_editor_property("parameter_name", unreal.Name(name))
        n.set_editor_property("default_value", float(default))
        return n

    def tint(name, values, x, y):
        n = node(unreal.MaterialExpressionVectorParameter, x, y)
        n.set_editor_property("parameter_name", unreal.Name(name))
        n.set_editor_property("default_value", unreal.LinearColor(*values, 1.0))
        return n

    def op(cls, a, b, x, y, output_a="", output_b=""):
        n = node(cls, x, y)
        if cls is unreal.MaterialExpressionPower:
            wire(a, n, "Base", output_a)
            wire(b, n, "Exp", output_b)
        else:
            wire(a, n, "A", output_a)
            wire(b, n, "B", output_b)
        return n

    def saturate(src, x, y):
        n = node(unreal.MaterialExpressionSaturate, x, y)
        wire(src, n, "Input")
        return n

    def mix(a, b, weight, x, y, out_a="", out_b=""):
        n = node(unreal.MaterialExpressionLinearInterpolate, x, y)
        wire(a, n, "A", out_a)
        wire(b, n, "B", out_b)
        wire(weight, n, "Alpha")
        return n

    def mask(src, r, g, b, x, y):
        n = node(unreal.MaterialExpressionComponentMask, x, y)
        for ch, enabled in (("r", r), ("g", g), ("b", b), ("a", False)):
            n.set_editor_property(ch, enabled)
        wire(src, n, "Input")
        return n

    def tex_sample(parameter, default_texture, coordinates, x, y):
        n = node(unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        n.set_editor_property("parameter_name", unreal.Name(parameter))
        n.set_editor_property("texture", default_texture)
        wire(coordinates, n, "Coordinates")
        return n

    # 世界XY投射：材质必须能在Landscape平地和坡面上连续，不依赖偶然UV展开。
    pos = node(unreal.MaterialExpressionWorldPosition, -2700, -900)
    xy = mask(pos, True, True, False, -2440, -1070)
    z = mask(pos, False, False, True, -2440, -670)
    macro_scale = scalar("VillageSnowMacroScale", .00036, -2450, -1380)
    detail_scale = scalar("VillageSnowDetailScale", .0032, -2400, -360)
    macro_uv = op(unreal.MaterialExpressionMultiply, xy, macro_scale, -2160, -1250)
    detail_uv = op(unreal.MaterialExpressionMultiply, xy, detail_scale, -2160, -280)

    coverage_tex = tex_sample("VillageSnowCoverageTexture",
        tex["T_DBA_Village_SnowCoverageDefault"], macro_uv, -1850, -1310)
    albedo_tex = tex_sample("VillageSnowAlbedoTexture",
        tex["T_DBA_Village_SnowAlbedoDefault"], detail_uv, -1840, 80)
    normal_tex = tex_sample("VillageSnowNormalTexture",
        tex["T_DBA_Village_SnowNormalDefault"], detail_uv, -1840, 420)

    # 上朝向越明显越积雪；使用VertexNormalWS避免对最终法线输入构成反向引用。
    vertex_normal = node(unreal.MaterialExpressionVertexNormalWS, -2530, -2240)
    up = color((0, 0, 1), -2510, -2020)
    dot = op(unreal.MaterialExpressionDotProduct, vertex_normal, up, -2240, -2180)
    slope_sat = saturate(dot, -2000, -2180)
    slope_power = scalar("VillageSnowSlopePower", 2.6, -2000, -1990)
    slope = op(unreal.MaterialExpressionPower, slope_sat, slope_power, -1720, -2190)

    snow = scalar("VillageSnowAmount", 0., -2400, -2730)
    wetness = scalar("VillageWetness", 0., -2430, 1540)
    base_threshold = val(.5, -2140, -3050)
    shift = scalar("VillageSnowCoverageShiftScale", .4, -2410, -3050)
    scaled_amount = op(unreal.MaterialExpressionMultiply, snow, shift, -2130, -2780)
    threshold = op(unreal.MaterialExpressionSubtract, base_threshold, scaled_amount, -1850, -2870)
    shifted = op(unreal.MaterialExpressionSubtract, coverage_tex, threshold,
                 -1580, -1390, output_a="R")
    softness = scalar("VillageSnowEdgeSoftness", .18, -1580, -1120)
    spread = saturate(op(unreal.MaterialExpressionDivide, shifted, softness, -1260, -1290),
                      -1040, -1290)

    # 高度因子由本项目地形海拔调节，默认足够低使村庄完整雪地成为可见基线。
    min_height = scalar("VillageSnowHeightStartCm", -200000., -2210, -700)
    height_diff = op(unreal.MaterialExpressionSubtract, z, min_height, -1800, -720)
    height_span = scalar("VillageSnowHeightFadeCm", 1000., -1830, -490)
    height = saturate(op(unreal.MaterialExpressionDivide,
                         height_diff, height_span, -1510, -730), -1300, -700)
    snow_edge = op(unreal.MaterialExpressionMultiply, snow, spread, -770, -1390)
    slope_snow = op(unreal.MaterialExpressionMultiply, snow_edge, slope, -520, -1300)
    coverage = saturate(op(unreal.MaterialExpressionMultiply,
                           slope_snow, height, -250, -1140), 20, -1050)

    dry = tint("VillageDryColor", (.29, .32, .22), -1340, 1590)
    wet = tint("VillageWetColor", (.16, .19, .15), -1330, 1840)
    earth = mix(dry, wet, wetness, -980, 1630)
    snow_tint = tint("VillageSnowTint", (.96, .975, 1), -1370, 250)
    detailed_snow = op(unreal.MaterialExpressionMultiply, albedo_tex,
                       snow_tint, -1000, 380, output_a="RGB")
    final_color = mix(earth, detailed_snow, coverage, 370, 400)
    if not lib.connect_material_property(final_color, "",
                                         unreal.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError("Landscape BaseColor未连接")

    dry_r = scalar("VillageDryRoughness", .85, -1250, 2070)
    wet_r = scalar("VillageWetRoughness", .36, -1260, 2300)
    snow_r = scalar("VillageSnowRoughness", .88, -1060, 2540)
    earth_r = mix(dry_r, wet_r, wetness, -800, 2130)
    final_rough = mix(earth_r, snow_r, coverage, 370, 1940)
    if not lib.connect_material_property(final_rough, "",
                                         unreal.MaterialProperty.MP_ROUGHNESS):
        raise RuntimeError("Landscape Roughness未连接")
    flat = color((0, 0, 1), -690, 2670)
    final_normal = mix(flat, normal_tex, coverage, 360, 2720)
    if not lib.connect_material_property(final_normal, "",
                                         unreal.MaterialProperty.MP_NORMAL):
        raise RuntimeError("Landscape Normal未连接")

    nodes = lib.get_num_material_expressions(mat)
    if nodes < 40:
        raise RuntimeError("Village地形母材质图节点缺失，拒绝保存：" + str(nodes))
    diagnostics = [str(msg) for msg in lib.recompile_material(mat)]
    for msg in diagnostics:
        print("VILLAGE_LANDSCAPE_SHADER_MESSAGE", msg)
    if any("error" in msg.lower() for msg in diagnostics):
        raise RuntimeError("Village地形母材质Shader错误，禁止交付")
    if not assets.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError("Village Landscape材质保存失败")
    if not isinstance(assets.load_asset(MASTER), unreal.Material):
        raise RuntimeError("Village Landscape保存后回读类型错误")
    print("VILLAGE_LANDSCAPE_MASTER_SAVED", MASTER, "expressions", nodes)


def bind_map(unreal) -> None:
    assets = unreal.EditorAssetLibrary
    if not assets.does_asset_exist(MAP) or not assets.does_asset_exist(MASTER):
        raise FileNotFoundError("正式Village地图或中性地形材质不存在")
    master = assets.load_asset(MASTER)
    if not isinstance(master, unreal.Material):
        raise TypeError("共享地图不能使用非Material的地形母材质")

    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if editor is None or not editor.load_level(MAP):
        raise RuntimeError("正式Village地图无法由真实编辑器加载")
    landscapes = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
                  if isinstance(actor, unreal.LandscapeProxy)]
    if len(landscapes) != 1:
        raise RuntimeError("Village Landscape应恰好为1，拒绝批量修改地图：" + str(len(landscapes)))
    landscape = landscapes[0]
    old = landscape.get_editor_property("landscape_material")
    if old is not None and old.get_path_name() != master.get_path_name():
        raise RuntimeError("Village已配置其他美术地形材质，拒绝覆盖：" + old.get_path_name())

    # 此属性在Cook前由Editor设置，UE运行时只使用事先烘焙生成的Landscape MID；
    # 绝不尝试在Client运行时使用ALandscapeProxy::EditorSetLandscapeMaterial。
    already = bool(landscape.get_editor_property("use_dynamic_material_instance"))
    if old is None:
        landscape.set_editor_property("landscape_material", master)
    if not already:
        landscape.set_editor_property("use_dynamic_material_instance", True)
    if (old is None or not already) and not editor.save_current_level():
        raise RuntimeError("Village正式地图地形材质/MID开关未能保存")
    loaded = landscape.get_editor_property("landscape_material")
    if not loaded or loaded.get_path_name() != master.get_path_name():
        raise RuntimeError("Village地形材质赋值后回读不一致")
    if not landscape.get_editor_property("use_dynamic_material_instance"):
        raise RuntimeError("Landscape运行时动态MID开关未激活")
    print("VILLAGE_LANDSCAPE_MAP_SAVED", MAP,
          "landscape_material", loaded.get_path_name(), "dynamic_mid=True")


def main() -> None:
    sources = preflight()
    print("VILLAGE_LANDSCAPE_PLAN", MODE, MASTER, MAP)
    if MODE == "inspect":
        print("VILLAGE_LANDSCAPE_NO_ENGINE_ASSETS_WRITTEN")
        return
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("真实UE资产只能在已加载正式UE5.8工程的Editor中创建") from exc
    project = unreal.Paths.get_project_file_path().replace("\\", "/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("拒绝在其他UE工程创建Village地形资产：" + project)

    if MODE in ("material", "all"):
        create_assets(unreal, sources)
    if MODE in ("map", "all"):
        bind_map(unreal)
    if MODE == "verify":
        if not unreal.EditorAssetLibrary.does_asset_exist(MASTER):
            raise RuntimeError("材质尚未创建")
        for rec in sources:
            if not unreal.EditorAssetLibrary.does_asset_exist(rec["assetPath"]):
                raise RuntimeError("缺少默认世界纹理：" + rec["assetPath"])
        print("VILLAGE_LANDSCAPE_VERIFY_ASSETS_PRESENT")
    print("VILLAGE_LANDSCAPE_AUTHOR_DONE", MODE)


if __name__ == "__main__":
    main()
