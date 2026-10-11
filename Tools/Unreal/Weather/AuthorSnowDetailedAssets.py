#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""积雪写实母材质、贴地雪粉材质及神兽联盟项目实例的真实UE5.8作者工具。

边界：
  - GamePlatformSurface的M_GP_SnowCover_Detailed：跨游戏视觉材质，不计算权威地形/摩擦。
  - GamePlatformVFX的M_GP_VFX_SnowGroundPuff：跨游戏客户端雪粉Niagara用的透明粒子材质。
  - DBAPresentationPack_Core的MI_DBA_Snow_Detailed：第三层项目可换肤实例，不依赖MOBA。
  - 工作空间原有M_GP_Surface_Master/NS_GP_Weather_Snow一律不修改。
  - 默认只读；SNOW_MATERIAL_AUTHOR_MODE=apply才创建；已有同名严格校验并跳过。
执行时机：使用已加载正式项目全部Editor反射模块的UE5.8 Python插件。
"""

from __future__ import annotations

import os

MODE = os.getenv("SNOW_MATERIAL_AUTHOR_MODE", "inspect").strip().lower()
SURFACE = "/GamePlatformSurface"
VFX = "/GamePlatformVFX"
DBA = "/DBAPresentationPack_Core"
MPC = SURFACE + "/ParameterCollections/MPC_GP_SurfaceGlobal"
MASTER = SURFACE + "/Materials/M_GP_SnowCover_Detailed"
PUFF_MATERIAL = VFX + "/Weather/Materials/M_GP_VFX_SnowGroundPuff"
INSTANCE = DBA + "/Materials/Snow/MI_DBA_Snow_Detailed"
TEXTURES = {
    "albedo": SURFACE + "/Textures/Weather/T_GP_Weather_SnowAlbedo",
    "normal": SURFACE + "/Textures/Weather/T_GP_Weather_SnowNormal",
    "orm": SURFACE + "/Textures/Weather/T_GP_Weather_SnowORM",
    "micro": SURFACE + "/Textures/Snow/T_GP_Snow_MicroHeight",
    "coverage": SURFACE + "/Textures/Snow/T_GP_Snow_CoverageNoise",
    "puff": VFX + "/Weather/Textures/T_GP_Snow_GroundPuff",
}


def author() -> None:
    if MODE not in ("inspect", "apply"):
        raise ValueError("SNOW_MATERIAL_AUTHOR_MODE只允许inspect/apply")
    for target in (MASTER, PUFF_MATERIAL, INSTANCE):
        print("SNOW_MATERIAL_PLAN", target)
    if MODE == "inspect":
        return
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("仅允许在实际UE5.8 Editor内生成真实材质资产") from exc

    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.MaterialEditingLibrary
    requirements = [MPC, *TEXTURES.values()]
    for package in requirements:
        if not assets.does_asset_exist(package):
            raise FileNotFoundError("前置UE资产不存在：" + package)
    loaded = {key: assets.load_asset(path) for key, path in TEXTURES.items()}
    if not all(isinstance(t, unreal.Texture2D) for t in loaded.values()):
        raise TypeError("积雪贴图资源类错误")
    mpc = assets.load_asset(MPC)
    if not isinstance(mpc, unreal.MaterialParameterCollection):
        raise TypeError("Surface MPC资源类错误")

    def new_node(material, cls, x, y):
        expr = lib.create_material_expression(material, cls, x, y)
        if expr is None:
            raise RuntimeError("创建材质表达式失败：" + str(cls))
        return expr

    def connect(a, b, pin, output=""):
        good = lib.connect_material_expressions(a, output, b, pin)
        if not good and pin in ("Input", "Position", "Coordinates"):
            # UE5.8材质脚本代理偶尔使用空FName描述材质节点输入。
            # 对贴图UV首先尝试编辑器中的UVs显示名，最终只连接真实默认引脚。
            if pin == "Coordinates":
                good = lib.connect_material_expressions(a, output, b, "UVs")
            if not good:
                good = lib.connect_material_expressions(a, output, b, "")
        if not good:
            raise RuntimeError("连线失败：" + a.get_name() + " -> " + b.get_name() + ":" + pin)

    def scalar(mat, name, default, x, y):
        expr = new_node(mat, unreal.MaterialExpressionScalarParameter, x, y)
        expr.set_editor_property("parameter_name", unreal.Name(name))
        expr.set_editor_property("default_value", float(default))
        return expr

    def vector(mat, name, rgb, x, y):
        expr = new_node(mat, unreal.MaterialExpressionVectorParameter, x, y)
        expr.set_editor_property("parameter_name", unreal.Name(name))
        expr.set_editor_property("default_value", unreal.LinearColor(*rgb, 1.0))
        return expr

    def rgb(mat, r, g, b, x, y):
        expr = new_node(mat, unreal.MaterialExpressionConstant3Vector, x, y)
        expr.set_editor_property("constant", unreal.LinearColor(r, g, b, 1.0))
        return expr

    def num(mat, val, x, y):
        expr = new_node(mat, unreal.MaterialExpressionConstant, x, y)
        expr.set_editor_property("r", float(val))
        return expr

    def sample(mat, name, texture, uv, x, y):
        expr = new_node(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        expr.set_editor_property("parameter_name", unreal.Name(name))
        expr.set_editor_property("texture", texture)
        connect(uv, expr, "Coordinates")
        return expr

    def binary(mat, cls, a, b, x, y, out_a="", out_b=""):
        expr = new_node(mat, cls, x, y)
        # Power节点在UE5.8的输入不是A/B，而是Base/Exp。
        if cls is unreal.MaterialExpressionPower:
            connect(a, expr, "Base", out_a)
            connect(b, expr, "Exp", out_b)
        else:
            connect(a, expr, "A", out_a)
            connect(b, expr, "B", out_b)
        return expr

    def sat(mat, expr, x, y):
        target = new_node(mat, unreal.MaterialExpressionSaturate, x, y)
        connect(expr, target, "Input")
        return target

    def lerp(mat, a, b, alpha, x, y, out_a="", out_b=""):
        target = new_node(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
        connect(a, target, "A", out_a)
        connect(b, target, "B", out_b)
        connect(alpha, target, "Alpha")
        return target

    def mpc_scalar(mat, name, x, y):
        expr = new_node(mat, unreal.MaterialExpressionCollectionParameter, x, y)
        expr.set_editor_property("collection", mpc)
        expr.set_editor_property("parameter_name", unreal.Name(name))
        return expr

    def channels(mat, value, red, green, blue, x, y):
        expr = new_node(mat, unreal.MaterialExpressionComponentMask, x, y)
        expr.set_editor_property("r", red)
        expr.set_editor_property("g", green)
        expr.set_editor_property("b", blue)
        expr.set_editor_property("a", False)
        connect(value, expr, "Input")
        return expr

    def save_compiled(mat, at_least):
        count = lib.get_num_material_expressions(mat)
        if count < at_least:
            raise RuntimeError("表达式数量不足，材质图不完整：" + mat.get_name())
        messages = list(lib.recompile_material(mat))
        for message in messages:
            print("SNOW_SHADER_DIAGNOSTIC", mat.get_name(), str(message))
        if any("error" in str(x).lower() for x in messages):
            raise RuntimeError("雪材质着色器编译失败：" + mat.get_name())
        if not assets.save_loaded_asset(mat, only_if_is_dirty=False):
            raise RuntimeError("材质保存失败：" + mat.get_path_name())
        if not isinstance(assets.load_asset(mat.get_path_name()), unreal.Material):
            raise RuntimeError("材质保存回读失败：" + mat.get_path_name())
        print("SNOW_MATERIAL_UE_SAVED", mat.get_path_name(), "nodes", count)

    if not assets.does_asset_exist(MASTER):
        folder, name = MASTER.rsplit("/", 1)
        if not assets.does_directory_exist(folder):
            assets.make_directory(folder)
        mat = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
        if not isinstance(mat, unreal.Material):
            raise RuntimeError("未能创建积雪母材质")

        worldpos = new_node(mat, unreal.MaterialExpressionWorldPosition, -2150, -1370)
        posxy = channels(mat, worldpos, True, True, False, -1900, -1470)
        posz = channels(mat, worldpos, False, False, True, -1900, -1150)
        macro_scale = scalar(mat, "SnowWorldMacroScale", 0.00045, -1910, -1770)
        world_uv = binary(mat, unreal.MaterialExpressionMultiply, posxy, macro_scale, -1670, -1500)
        tex_uv = new_node(mat, unreal.MaterialExpressionTextureCoordinate, -2090, 270)
        micro_tile = scalar(mat, "SnowMicroTiling", 7.5, -2070, 490)
        micro_uv = binary(mat, unreal.MaterialExpressionMultiply, tex_uv, micro_tile, -1830, 360)

        macro = sample(mat, "SnowCoverageTexture", loaded["coverage"], world_uv, -1420, -1690)
        micro = sample(mat, "SnowMicroHeightTexture", loaded["micro"], micro_uv, -1410, -20)
        albedo = sample(mat, "SnowAlbedoTexture", loaded["albedo"], micro_uv, -1400, 240)
        normal = sample(mat, "SnowNormalTexture", loaded["normal"], micro_uv, -1400, 540)
        orm = sample(mat, "SnowORMTexture", loaded["orm"], micro_uv, -1410, 810)

        # 使用世界顶点法线而不是PixelNormalWS，消除Mask与输出Normal的循环依赖。
        vertex_normal = new_node(mat, unreal.MaterialExpressionVertexNormalWS, -2020, -2260)
        up = rgb(mat, 0, 0, 1, -2010, -2030)
        dot = binary(mat, unreal.MaterialExpressionDotProduct, vertex_normal, up, -1730, -2170)
        slope_saturate = sat(mat, dot, -1510, -2170)
        slope_power = scalar(mat, "SnowSlopePower", 3.0, -1530, -1980)
        slope = binary(mat, unreal.MaterialExpressionPower, slope_saturate, slope_power, -1210, -2140)

        # 高度是世界单位厘米：默认WorldZ=0附近不截断；项目可覆盖Reference与Fade。
        height_reference = mpc_scalar(mat, "GP_Surface_GlobalSnowHeightCm", -1770, -1020)
        delta = binary(mat, unreal.MaterialExpressionSubtract, posz, height_reference, -1470, -1120)
        height_bias = scalar(mat, "SnowHeightBiasCm", 500.0, -1750, -770)
        biased = binary(mat, unreal.MaterialExpressionAdd, delta, height_bias, -1180, -1170)
        height_fade = scalar(mat, "SnowHeightFadeCm", 500.0, -1430, -900)
        height = sat(mat, binary(mat, unreal.MaterialExpressionDivide,
                                 biased, height_fade, -910, -1120), -690, -1100)

        # 完全无雪时由阈值归零；全雪时近乎全覆盖，但坡面仍受到坡度约束。
        global_snow = mpc_scalar(mat, "GP_Surface_GlobalSnowAmount", -1530, -2650)
        global_wetness = mpc_scalar(mat, "GP_Surface_GlobalWetness", -1870, 1000)
        inverse = binary(mat, unreal.MaterialExpressionSubtract, num(mat, 1.0, -1720, -2700),
                         global_snow, -1280, -2580)
        noise_delta = binary(mat, unreal.MaterialExpressionSubtract,
                             macro, inverse, -1010, -1820, out_a="R")
        edge = scalar(mat, "SnowEdgeSoftness", 0.25, -1080, -1520)
        gated = sat(mat, binary(mat, unreal.MaterialExpressionDivide,
                                noise_delta, edge, -760, -1760), -550, -1780)
        slope_height = binary(mat, unreal.MaterialExpressionMultiply, slope, height, -320, -1710)
        coverage = sat(mat, binary(mat, unreal.MaterialExpressionMultiply,
                                   gated, slope_height, -100, -1570), 120, -1530)

        dry = vector(mat, "BaseDryColor", (0.31, .31, .33), -1440, 1180)
        wet = vector(mat, "BaseWetColor", (.17, .18, .20), -1440, 1410)
        base_color = lerp(mat, dry, wet, global_wetness, -1130, 1340)
        snow_tint = vector(mat, "SnowTint", (.96, .975, 1), -1090, 470)
        snow_col = binary(mat, unreal.MaterialExpressionMultiply,
                          albedo, snow_tint, -780, 420, out_a="RGB")
        final_color = lerp(mat, base_color, snow_col, coverage, 410, 400)
        if not lib.connect_material_property(final_color, "", unreal.MaterialProperty.MP_BASE_COLOR):
            raise RuntimeError("Snow PBR BaseColor未连接")

        dry_r = scalar(mat, "BaseDryRoughness", 0.78, -1270, 1760)
        wet_r = scalar(mat, "BaseWetRoughness", 0.34, -1270, 1950)
        base_r = lerp(mat, dry_r, wet_r, global_wetness, -940, 1780)
        rough_delta = binary(mat, unreal.MaterialExpressionMultiply, micro,
                             scalar(mat, "SnowRoughnessMicroInfluence", .11, -940, 2240),
                             -700, 2150, out_a="R")
        snow_r = sat(mat, binary(mat, unreal.MaterialExpressionSubtract,
                                 orm, rough_delta, -420, 1850, out_a="G"), -180, 1870)
        final_r = lerp(mat, base_r, snow_r, coverage, 425, 1720)
        if not lib.connect_material_property(final_r, "", unreal.MaterialProperty.MP_ROUGHNESS):
            raise RuntimeError("Snow PBR Roughness未连接")

        flat = rgb(mat, 0, 0, 1, -600, 2680)
        final_normal = lerp(mat, flat, normal, coverage, 425, 2510)
        if not lib.connect_material_property(final_normal, "", unreal.MaterialProperty.MP_NORMAL):
            raise RuntimeError("Snow PBR Normal未连接")
        save_compiled(mat, 42)
    else:
        material = assets.load_asset(MASTER)
        if not isinstance(material, unreal.Material) or lib.get_num_material_expressions(material) < 42:
            raise RuntimeError("已有同名雪母材质但类别或节点不符合本制作脚本，禁止覆盖")
        print("SNOW_MATERIAL_ALREADY_PRESENT", MASTER)

    if not assets.does_asset_exist(PUFF_MATERIAL):
        folder, name = PUFF_MATERIAL.rsplit("/", 1)
        if not assets.does_directory_exist(folder):
            assets.make_directory(folder)
        mat = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
        if not isinstance(mat, unreal.Material):
            raise RuntimeError("无法创建雪粉粒子材质")
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property("two_sided", True)
        # Niagara用途标记必须设置：否则Emitter虽编译但渲染时静默替换默认材质。
        mat.set_editor_property("used_with_niagara_sprites", True)
        tex = new_node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -950, 80)
        tex.set_editor_property("parameter_name", unreal.Name("SnowGroundPuffTexture"))
        tex.set_editor_property("texture", loaded["puff"])
        particle = new_node(mat, unreal.MaterialExpressionParticleColor, -940, 460)
        tint = binary(mat, unreal.MaterialExpressionMultiply, tex, particle,
                      -590, 150, out_a="RGB", out_b="RGB")
        if not lib.connect_material_property(tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
            raise RuntimeError("贴地雪粉Emissive未连接")
        alpha = binary(mat, unreal.MaterialExpressionMultiply, tex, particle,
                       -560, 570, out_a="A", out_b="A")
        opacity = scalar(mat, "GroundPuffOpacityScale", .55, -580, 890)
        out_alpha = binary(mat, unreal.MaterialExpressionMultiply, alpha, opacity, -190, 630)
        if not lib.connect_material_property(out_alpha, "", unreal.MaterialProperty.MP_OPACITY):
            raise RuntimeError("贴地雪粉Opacity未连接")
        save_compiled(mat, 6)
    else:
        mat = assets.load_asset(PUFF_MATERIAL)
        if not isinstance(mat, unreal.Material) or lib.get_num_material_expressions(mat) < 6:
            raise RuntimeError("已有同名贴地雪粉材质但结构不匹配")
        # 新建脚本早期版本未标记Niagara Sprite用途；对本脚本唯一拥有的
        # M_GP_VFX_SnowGroundPuff执行幂等修复，不修改其他已有雨雪粒子材质。
        if not mat.get_editor_property("used_with_niagara_sprites"):
            mat.set_editor_property("used_with_niagara_sprites", True)
            save_compiled(mat, 6)
            print("SNOW_PUFF_NIAGARA_USAGE_FIXED", PUFF_MATERIAL)
        print("SNOW_MATERIAL_ALREADY_PRESENT", PUFF_MATERIAL)

    if not assets.does_asset_exist(INSTANCE):
        folder, name = INSTANCE.rsplit("/", 1)
        if not assets.does_directory_exist(folder):
            assets.make_directory(folder)
        instance = tools.create_asset(name, folder, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(instance, unreal.MaterialInstanceConstant):
            raise RuntimeError("未生成第三层神兽联盟雪材质实例")
        master = assets.load_asset(MASTER)
        lib.set_material_instance_parent(instance, master)
        lib.set_material_instance_scalar_parameter_value(instance, "SnowSlopePower", 2.6)
        lib.set_material_instance_scalar_parameter_value(instance, "SnowWorldMacroScale", .00034)
        lib.set_material_instance_scalar_parameter_value(instance, "SnowMicroTiling", 6.0)
        lib.set_material_instance_scalar_parameter_value(instance, "SnowEdgeSoftness", .18)
        if not assets.save_loaded_asset(instance, only_if_is_dirty=False):
            raise RuntimeError("项目雪材质实例保存失败")
        print("SNOW_PROJECT_MI_SAVED", INSTANCE)
    else:
        instance = assets.load_asset(INSTANCE)
        if not isinstance(instance, unreal.MaterialInstanceConstant):
            raise RuntimeError("已有同名实例资源不是UMaterialInstanceConstant")
        print("SNOW_PROJECT_MI_ALREADY_PRESENT", INSTANCE)

    for target, cls in ((MASTER, unreal.Material),
                        (PUFF_MATERIAL, unreal.Material),
                        (INSTANCE, unreal.MaterialInstanceConstant)):
        if not isinstance(assets.load_asset(target), cls):
            raise TypeError("雪材质资产回读类别不正确：" + target)
        print("SNOW_ASSET_VERIFIED", target)


if __name__ == "__main__":
    author()
