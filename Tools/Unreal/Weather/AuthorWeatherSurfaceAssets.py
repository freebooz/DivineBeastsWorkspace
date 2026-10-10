#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 天气Surface真实材质函数与母材质编辑器制作脚本。

仅通过真实UE PythonScriptPlugin的AssetTools与MaterialEditingLibrary写.uasset。
非UE Python或未设置 WEATHER_SURFACE_AUTHOR_MODE=apply 时只输出制作规划。
支持 WEATHER_SURFACE_AUTHOR_PHASE=functions|materials|all，严格拒绝目标重名。
材质功能域：坡度/高度/世界噪声/雪层/苔藓/湿润/积水。
材质使用现有GamePlatformSurface八参数MPC，不创建重复的参数真源。
所有代码、注释与验收说明使用中文；未在引擎实际运行前绝不声称已生成资源。
"""
from __future__ import annotations

import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = Path(__file__).with_name("WeatherSourceArtManifest.json")
MODE = os.environ.get("WEATHER_SURFACE_AUTHOR_MODE", "inspect").strip().lower()
PHASE = os.environ.get("WEATHER_SURFACE_AUTHOR_PHASE", "all").strip().lower()
SURFACE = "/GamePlatformSurface"
MPC = SURFACE + "/ParameterCollections/MPC_GP_SurfaceGlobal"
T_SNOW = SURFACE + "/Textures/Weather/T_GP_Weather_SnowAlbedo"
T_NORMAL = SURFACE + "/Textures/Weather/T_GP_Weather_SnowNormal"
FUNCTIONS = (
    ("Masks", "MF_GP_SlopeMask"),
    ("Masks", "MF_GP_HeightMask"),
    ("Utility", "MF_GP_WorldNoise"),
    ("Layers", "MF_GP_SnowLayer"),
    ("Layers", "MF_GP_MossLayer"),
    ("Layers", "MF_GP_WetnessLayer"),
    ("Layers", "MF_GP_PuddleLayer"),
)
MATERIALS = ("M_GP_Surface_Master", "M_GP_Surface_Lite")


def paths() -> list[str]:
    functions = [f"{SURFACE}/MaterialFunctions/{area}/{name}" for area, name in FUNCTIONS]
    materials = [f"{SURFACE}/Materials/{name}" for name in MATERIALS]
    return functions if PHASE == "functions" else materials if PHASE == "materials" else functions + materials


def inspect() -> None:
    if PHASE not in ("all", "functions", "materials"):
        raise ValueError("WEATHER_SURFACE_AUTHOR_PHASE必须为all/functions/materials")
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    names = {Path(x["sourcePath"]).stem for x in manifest["textures"]}
    if manifest["schemaVersion"] != 1 or not {
        "T_GP_Weather_SnowAlbedo_Source", "T_GP_Weather_SnowNormal_Source"
    } <= names:
        raise ValueError("雪地纹理源合同不满足")
    for target in paths():
        print("SURFACE_TARGET", target)
    print("MPC_REQUIRED", MPC)
    print("TEXTURE_REQUIRED", T_SNOW, T_NORMAL)


def run_editor() -> None:
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("只允许UE5.8 Editor内执行，不能写假.uasset") from exc

    lib = unreal.MaterialEditingLibrary
    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for name in (MPC, T_SNOW, T_NORMAL):
        if not assets.does_asset_exist(name):
            raise FileNotFoundError("缺少正式UE前置资产：" + name)
    for path in paths():
        if assets.does_asset_exist(path):
            raise FileExistsError("已存在资源；拒绝覆盖：" + path)

    mpc = assets.load_asset(MPC)
    snow_texture = assets.load_asset(T_SNOW)
    normal_texture = assets.load_asset(T_NORMAL)
    if not isinstance(mpc, unreal.MaterialParameterCollection):
        raise TypeError("MPC对象类别不正确")
    for texture in (snow_texture, normal_texture):
        if not isinstance(texture, unreal.Texture2D):
            raise TypeError("Surface材质纹理不是Texture2D")

    def node(owner, expr_type, x: int, y: int):
        if isinstance(owner, unreal.Material):
            value = lib.create_material_expression(owner, expr_type, x, y)
        else:
            value = lib.create_material_expression_in_function(owner, expr_type, x, y)
        if value is None:
            raise RuntimeError(f"无法在{owner.get_name()}创建材质表达式{expr_type}")
        return value

    def setprop(obj, prop: str, value):
        obj.set_editor_property(prop, value)

    def link(left, right, pin: str, output=""):
        if not lib.connect_material_expressions(left, output, right, pin):
            raise RuntimeError(f"材质图连接失败：{left.get_name()} -> {right.get_name()}({pin})")

    def constant(owner, value, x, y):
        o = node(owner, unreal.MaterialExpressionConstant, x, y)
        setprop(o, "r", float(value))
        return o

    def color(owner, r, g, b, x, y):
        o = node(owner, unreal.MaterialExpressionConstant3Vector, x, y)
        setprop(o, "constant", unreal.LinearColor(float(r), float(g), float(b), 1.0))
        return o

    def binary(owner, class_ref, a, b, x, y, input_a="A", input_b="B"):
        o = node(owner, class_ref, x, y)
        link(a, o, input_a)
        link(b, o, input_b)
        return o

    def mix(owner, a, b, amount, x, y):
        o = node(owner, unreal.MaterialExpressionLinearInterpolate, x, y)
        link(a, o, "A")
        link(b, o, "B")
        link(amount, o, "Alpha")
        return o

    def mpc_value(owner, prop, x, y):
        obj = node(owner, unreal.MaterialExpressionCollectionParameter, x, y)
        setprop(obj, "collection", mpc)
        setprop(obj, "parameter_name", unreal.Name(prop))
        return obj

    def function_input(owner, name, vector, x, y):
        o = node(owner, unreal.MaterialExpressionFunctionInput, x, y)
        setprop(o, "input_name", name)
        kind = ("FUNCTION_INPUT_VECTOR3" if vector else "FUNCTION_INPUT_SCALAR")
        setprop(o, "input_type", getattr(unreal.FunctionInputType, kind))
        setprop(o, "use_preview_value_as_default", True)
        # 仅HeightRangeCm设100厘米非零默认预览值，避免未连接函数输入导致除0或无意义Shader。
        if name == "HeightRangeCm":
            setprop(o, "preview_value", unreal.LinearColor(100.0, 0.0, 0.0, 0.0))
        return o

    def function_output(owner, source, name, x, y):
        o = node(owner, unreal.MaterialExpressionFunctionOutput, x, y)
        setprop(o, "output_name", name)
        link(source, o, "A")

    def finish_function(asset, label):
        count = lib.get_num_material_expressions_in_function(asset)
        if count < 3:
            raise RuntimeError(f"{label}节点数异常：{count}")
        lib.update_material_function(asset)
        if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"{label}真实函数保存失败")
        if not isinstance(assets.load_asset(asset.get_path_name()), unreal.MaterialFunction):
            raise RuntimeError(f"{label}回读类错误")
        print(f"UE_WEATHER_FUNCTION_SAVED {asset.get_path_name()} nodes={count}")

    def create_functions() -> None:
        factory = unreal.MaterialFunctionFactoryNew()
        for area, name in FUNCTIONS:
            dest = f"{SURFACE}/MaterialFunctions/{area}"
            if not assets.does_directory_exist(dest):
                assets.make_directory(dest)
            fn = tools.create_asset(name, dest, unreal.MaterialFunction, factory)
            if not isinstance(fn, unreal.MaterialFunction):
                raise RuntimeError("UE无法创建真实材质函数：" + name)
            if name == "MF_GP_SlopeMask":
                normal = node(fn, unreal.MaterialExpressionVertexNormalWS, -900, -100) # 使用未受Normal输入影响的顶点法线，避免雪遮罩/Normal输出构成环形材质依赖。
                mask = node(fn, unreal.MaterialExpressionComponentMask, -650, -100)
                setprop(mask, "r", False); setprop(mask, "g", False)
                setprop(mask, "b", True); setprop(mask, "a", False)
                link(normal, mask, "Input")
                saturated = node(fn, unreal.MaterialExpressionSaturate, -420, -100)
                link(mask, saturated, "Input")
                function_output(fn, saturated, "SlopeMask", -140, -100)
            elif name == "MF_GP_HeightMask":
                position = node(fn, unreal.MaterialExpressionWorldPosition, -900, -100)
                zmask = node(fn, unreal.MaterialExpressionComponentMask, -650, -120)
                setprop(zmask, "r", False); setprop(zmask, "g", False)
                setprop(zmask, "b", True); setprop(zmask, "a", False)
                link(position, zmask, "Input")
                height_start = function_input(fn, "HeightStartCm", False, -900, 260)
                height_range = function_input(fn, "HeightRangeCm", False, -900, 520)
                offset = binary(fn, unreal.MaterialExpressionSubtract, zmask, height_start, -380, -100)
                divided = binary(fn, unreal.MaterialExpressionDivide, offset, height_range, -170, -100)
                clamp = node(fn, unreal.MaterialExpressionSaturate, 30, -100)
                link(divided, clamp, "Input")
                function_output(fn, clamp, "HeightMask", 230, -100)
            elif name == "MF_GP_WorldNoise":
                position = node(fn, unreal.MaterialExpressionWorldPosition, -920, 0)
                scale = constant(fn, .0015, -920, 300)
                scaled = binary(fn, unreal.MaterialExpressionMultiply, position, scale, -600, 30)
                noise = node(fn, unreal.MaterialExpressionNoise, -300, 30)
                link(scaled, noise, "Position")
                function_output(fn, noise, "WorldNoise", 50, 30)
            elif name in ("MF_GP_SnowLayer", "MF_GP_MossLayer", "MF_GP_WetnessLayer", "MF_GP_PuddleLayer"):
                base = function_input(fn, "BaseColor", True, -1050, -250)
                amount = function_input(fn, "Amount", False, -1050, 200)
                if name == "MF_GP_SnowLayer":
                    tint = color(fn, .91, .94, .97, -800, -550)
                    roughness = constant(fn, .93, -760, 550)
                elif name == "MF_GP_MossLayer":
                    tint = color(fn, .18, .29, .14, -800, -550)
                    roughness = constant(fn, .85, -760, 550)
                elif name == "MF_GP_WetnessLayer":
                    tint = color(fn, .18, .17, .14, -800, -550)
                    roughness = constant(fn, .34, -760, 550)
                else:
                    tint = color(fn, .12, .13, .13, -800, -550)
                    roughness = constant(fn, .065, -760, 550)
                blend = mix(fn, base, tint, amount, -360, -180)
                function_output(fn, blend, "BaseColor", -75, -180)
                dryrough = function_input(fn, "BaseRoughness", False, -1050, 700)
                roughblend = mix(fn, dryrough, roughness, amount, -330, 450)
                function_output(fn, roughblend, "Roughness", -75, 450)
                # 每个表面层均输出切线空间法线，至少雪/湿润/积水有差异而不是仅改变颜色。
                normal_in = function_input(fn, "BaseNormal", True, -1050, 1020)
                normal_out = normal_in
                if name in ("MF_GP_SnowLayer", "MF_GP_WetnessLayer", "MF_GP_PuddleLayer"):
                    plane_normal = color(fn, 0.0, 0.0, 1.0, -770, 1260)
                    normal_blend_weight = amount
                    if name == "MF_GP_WetnessLayer":
                        flatten_factor = constant(fn, .38, -790, 1540)
                        normal_blend_weight = binary(fn, unreal.MaterialExpressionMultiply,
                                                     amount, flatten_factor, -540, 1490)
                    normal_out = mix(fn, normal_in, plane_normal,
                                     normal_blend_weight, -290, 1180)
                function_output(fn, normal_out, "Normal", -75, 1180)
            else:
                raise RuntimeError(f"未实现材质函数：{name}")
            finish_function(fn, name)

    def finish_material(material):
        count = lib.get_num_material_expressions(material)
        if count < 13:
            raise RuntimeError(f"{material.get_name()}材质图节点过少：{count}")
        problems = list(lib.recompile_material(material))
        for message in problems:
            print(f"MATERIAL_COMPILER_MESSAGE {material.get_name()} {message}")
        if any("error" in str(p).lower() for p in problems):
            raise RuntimeError("UE着色器编译存在错误：" + material.get_name())
        if not assets.save_loaded_asset(material, only_if_is_dirty=False):
            raise RuntimeError("材质包保存失败：" + material.get_path_name())
        reloaded = assets.load_asset(material.get_path_name())
        if not isinstance(reloaded, unreal.Material):
            raise RuntimeError("保存材质后引擎资产回读失败")
        print("UE_WEATHER_MATERIAL_SAVED", material.get_path_name(), "nodes=", count)

    def create_materials():
        destination = SURFACE + "/Materials"
        if not assets.does_directory_exist(destination):
            assets.make_directory(destination)
        for name in MATERIALS:
            lite = name.endswith("_Lite")
            mat = tools.create_asset(name, destination, unreal.Material, unreal.MaterialFactoryNew())
            if not isinstance(mat, unreal.Material):
                raise RuntimeError("创建真实Surface材质失败：" + name)

            dry = node(mat, unreal.MaterialExpressionVectorParameter, -1600, 0)
            setprop(dry, "parameter_name", unreal.Name("BaseDryColor"))
            setprop(dry, "default_value", unreal.LinearColor(.40, .35, .26, 1))
            wet = node(mat, unreal.MaterialExpressionVectorParameter, -1550, 240)
            setprop(wet, "parameter_name", unreal.Name("BaseWetColor"))
            setprop(wet, "default_value", unreal.LinearColor(.20, .18, .14, 1))

            wetness = mpc_value(mat, "GP_Surface_GlobalWetness", -1580, -420)
            snowamount = mpc_value(mat, "GP_Surface_GlobalSnowAmount", -1580, -640)
            puddle = mpc_value(mat, "GP_Surface_GlobalPuddleAmount", -1570, -900)

            wetcolor = mix(mat, dry, wet, wetness, -1120, 100)
            if lite:
                snowcolor = node(mat, unreal.MaterialExpressionVectorParameter, -1130, 660)
                setprop(snowcolor, "parameter_name", unreal.Name("SnowTint"))
                setprop(snowcolor, "default_value", unreal.LinearColor(.90, .94, .98, 1))
                snow_alpha = snowamount
            else:
                snowcolor = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -1130, 720)
                setprop(snowcolor, "parameter_name", unreal.Name("SnowAlbedoTexture"))
                setprop(snowcolor, "texture", snow_texture)
                normalws = node(mat, unreal.MaterialExpressionVertexNormalWS, -1410, -1200) # 不得由本材质Normal反向参与坡度遮罩。
                up = color(mat, 0, 0, 1, -1390, -1450)
                slope_dot = binary(mat, unreal.MaterialExpressionDotProduct, normalws, up, -1100, -1180)
                slope = node(mat, unreal.MaterialExpressionSaturate, -920, -1160)
                link(slope_dot, slope, "Input")
                snow_alpha = binary(mat, unreal.MaterialExpressionMultiply, snowamount, slope, -690, -700)
            snowfinal = mix(mat, wetcolor, snowcolor, snow_alpha, -450, 140)
            if not lib.connect_material_property(snowfinal, "", unreal.MaterialProperty.MP_BASE_COLOR):
                raise RuntimeError("BaseColor输出未连接")

            r_dry = constant(mat, .76, -1480, 950)
            r_wet = constant(mat, .31, -1440, 1120)
            r_snow = constant(mat, .90, -1440, 1290)
            r_puddle = constant(mat, .06, -1380, 1460)
            r_wet_mix = mix(mat, r_dry, r_wet, wetness, -1020, 1000)
            r_snow_mix = mix(mat, r_wet_mix, r_snow, snow_alpha, -720, 1000)
            r_final = mix(mat, r_snow_mix, r_puddle, puddle, -410, 1000)
            if not lib.connect_material_property(r_final, "", unreal.MaterialProperty.MP_ROUGHNESS):
                raise RuntimeError("Roughness输出未连接")

            flatnormal = color(mat, 0, 0, 1, -1180, 1760) # Unreal切线空间Material Normal输入直接使用(0,0,1)，不是法线纹理的(0.5,0.5,1)编码值。
            if lite:
                output_normal = flatnormal
            else:
                ntex = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, -950, 1930)
                setprop(ntex, "parameter_name", unreal.Name("SnowNormalTexture"))
                setprop(ntex, "texture", normal_texture)
                output_normal = mix(mat, flatnormal, ntex, snow_alpha, -470, 1840)
            if not lib.connect_material_property(output_normal, "", unreal.MaterialProperty.MP_NORMAL):
                raise RuntimeError("Normal输出未连接")
            finish_material(mat)

    if PHASE in ("all", "functions"):
        create_functions()
    if PHASE in ("all", "materials"):
        create_materials()


def main() -> None:
    inspect()
    if MODE == "inspect":
        print("WEATHER_SURFACE_INSPECT_ONLY 未创建UE资产")
        return
    if MODE != "apply":
        raise ValueError("WEATHER_SURFACE_AUTHOR_MODE只能是inspect/apply")
    run_editor()


if __name__ == "__main__":
    main()
