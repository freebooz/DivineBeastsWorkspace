#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 平台通用材质函数首批五项真实作者工具。

归属：GamePlatformSurface（平台环境表面材质插件）已有Editor职责；无独立运行模块。
默认inspect（只列计划）。apply只能在唯一神兽联盟UE编辑器内用真实UE MaterialEditingLibrary
创建并连接计算节点、更新、保存、回读；verify只读原生类型与节点数。
不更改原七个Surface MF、原两个母版、正式Village地图、UI/VFX或服务器资源。
函数一旦已存在只做类别和节点数量校验，不覆盖、清图、重命名或自动修复。
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SURFACE_ROOT = "/GamePlatformSurface"
CONTENT_ROOT = ROOT / "Game" / "Plugins" / "GamePlatform" / "Presentation" / "GamePlatformSurface" / "Content"

# 仅首批可用基础节点实现；复杂RVT、三平面纹理采样仍须按锁定引擎单独验证。
FUNCTIONS = (
    ("Core", "MF_GP_HeightBlendMask", "height", 8, "高度混合遮罩"),
    ("Core", "MF_GP_MacroMicroBlend", "macro", 9, "宏观/微观纹理混合"),
    ("Landscape", "MF_GP_LandscapeBiomeMask", "biome", 8, "地形生物群落遮罩"),
    ("Landscape", "MF_GP_LandscapeDistanceDetail", "distance", 12, "距离纹理细节混合"),
    ("Layers", "MF_GP_FrostSurface", "frost", 16, "表面结霜PBR属性混合"),
)


def target_path(folder: str, name: str) -> str:
    """既有Surface插件挂载根；不创建新的独立Materials插件。"""
    return f"{SURFACE_ROOT}/MaterialFunctions/{folder}/{name}"


def on_disk_package(package: str) -> Path:
    """资产注册表无条目时也要检查磁盘，避免误以为可安全占用同名包。"""
    return CONTENT_ROOT / (package.removeprefix(SURFACE_ROOT + "/") + ".uasset")


def require_project(unreal) -> None:
    """只准在正式主项目写引擎资产，不以当前工作目录猜测编辑器身份。"""
    project = str(unreal.Paths.get_project_file_path()).replace("\\", "/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("拒绝跨项目创建通用材质：" + project)


def create_author(unreal):
    """本作用域独立构造节点，避免材质函数加载顺序依赖。"""
    editing = unreal.MaterialEditingLibrary

    def expression(function, class_name: str, x: int, y: int):
        klass = getattr(unreal, "MaterialExpression" + class_name, None)
        if klass is None:
            raise RuntimeError("锁定引擎缺少材质表达式：" + class_name)
        value = editing.create_material_expression_in_function(function, klass, x, y)
        if value is None:
            raise RuntimeError("真实UE表达式创建失败：" + class_name)
        return value

    def wire(left, right, pin: str):
        """针对Unreal无名的单输入引脚允许安全回退，其余连接必须明确成功。"""
        connected = editing.connect_material_expressions(left, "", right, pin)
        if not connected and pin in {"Input", "VectorInput"}:
            connected = editing.connect_material_expressions(left, "", right, "")
        if not connected:
            raise RuntimeError("材质表达式引脚连接失败：" + left.get_name() + " -> " + right.get_name() + "." + pin)

    def inp(function, name: str, type_name: str, x: int, y: int):
        value = expression(function, "FunctionInput", x, y)
        value.set_editor_property("input_name", name)
        enum = getattr(unreal.FunctionInputType, "FUNCTION_INPUT_" + type_name, None)
        if enum is None:
            raise RuntimeError("引擎未公开函数输入类型：" + type_name)
        value.set_editor_property("input_type", enum)
        value.set_editor_property("use_preview_value_as_default", False)
        return value

    def const(function, num: float, x: int, y: int):
        value = expression(function, "Constant", x, y)
        value.set_editor_property("r", float(num))
        return value

    def rgb(function, r: float, g: float, b: float, x: int, y: int):
        value = expression(function, "Constant3Vector", x, y)
        value.set_editor_property("constant", unreal.LinearColor(r, g, b, 1.0))
        return value

    def unary(function, name: str, a, x: int, y: int):
        value = expression(function, name, x, y)
        wire(a, value, "Input")
        return value

    def binary(function, name: str, a, b, x: int, y: int):
        value = expression(function, name, x, y)
        wire(a, value, "A")
        wire(b, value, "B")
        return value

    def lerp(function, a, b, alpha, x: int, y: int):
        value = expression(function, "LinearInterpolate", x, y)
        wire(a, value, "A")
        wire(b, value, "B")
        wire(alpha, value, "Alpha")
        return value

    def output(function, source, name: str, x: int, y: int):
        value = expression(function, "FunctionOutput", x, y)
        value.set_editor_property("output_name", name)
        wire(source, value, "Input")
        return value

    def saturate(function, a, x: int, y: int):
        return unary(function, "Saturate", a, x, y)

    def safe_div(function, a, divisor, x: int, y: int):
        """分母最小1e-4，避免距离范围和过渡宽度为零造成NaN。"""
        minimum = const(function, 0.0001, x - 300, y + 140)
        bounded = binary(function, "Max", divisor, minimum, x - 150, y + 140)
        return binary(function, "Divide", a, bounded, x, y)

    def build(function, recipe: str):
        """生成真实运算而不是直接把输入连到输出的空函数。"""
        if recipe == "height":
            a = inp(function, "HeightA", "SCALAR", -1350, -550)
            b = inp(function, "HeightB", "SCALAR", -1350, -330)
            amount = inp(function, "BlendAlpha", "SCALAR", -1350, -110)
            sharpness = inp(function, "Contrast", "SCALAR", -1350, 110)
            diff = binary(function, "Subtract", b, a, -1050, -400)
            offset = binary(function, "Add", diff, amount, -750, -280)
            weighted = binary(function, "Multiply", offset, sharpness, -450, -140)
            output(function, saturate(function, weighted, -200, -100), "HeightBlendMask", 100, -100)
        elif recipe == "macro":
            base = inp(function, "BaseColor", "VECTOR3", -1350, -550)
            macro = inp(function, "MacroColor", "VECTOR3", -1350, -350)
            micro = inp(function, "MicroColor", "VECTOR3", -1350, -150)
            macrow = inp(function, "MacroWeight", "SCALAR", -1350, 100)
            microw = inp(function, "MicroWeight", "SCALAR", -1350, 300)
            first = lerp(function, base, macro, saturate(function, macrow, -1100, 130), -780, -320)
            second = lerp(function, first, micro, saturate(function, microw, -1060, 340), -410, -200)
            output(function, second, "BaseColor", -70, -180)
        elif recipe == "biome":
            weight = inp(function, "BiomeWeight", "SCALAR", -1350, -550)
            slope = inp(function, "SlopeMask", "SCALAR", -1350, -350)
            elevation = inp(function, "AltitudeMask", "SCALAR", -1350, -150)
            local = inp(function, "LocalWeight", "SCALAR", -1350, 50)
            first = binary(function, "Multiply", weight, slope, -940, -450)
            second = binary(function, "Multiply", first, elevation, -670, -290)
            third = binary(function, "Multiply", second, local, -390, -160)
            output(function, saturate(function, third, -150, -120), "BiomeMask", 90, -120)
        elif recipe == "distance":
            near = inp(function, "NearColor", "VECTOR3", -1500, -450)
            far = inp(function, "FarColor", "VECTOR3", -1500, -250)
            start = inp(function, "StartDistanceCm", "SCALAR", -1500, -50)
            end = inp(function, "EndDistanceCm", "SCALAR", -1500, 150)
            depth = expression(function, "PixelDepth", -1480, 440)
            numerator = binary(function, "Subtract", depth, start, -1050, 310)
            range_cm = binary(function, "Subtract", end, start, -1080, 100)
            alpha = saturate(function, safe_div(function, numerator, range_cm, -680, 180), -390, 200)
            color = lerp(function, near, far, alpha, -170, -180)
            output(function, color, "DetailColor", 130, -180)
            output(function, alpha, "FarWeight", 130, 170)
        elif recipe == "frost":
            base = inp(function, "BaseColor", "VECTOR3", -1700, -670)
            frost_color = inp(function, "FrostColor", "VECTOR3", -1700, -450)
            base_rough = inp(function, "BaseRoughness", "SCALAR", -1700, -180)
            frost_rough = inp(function, "FrostRoughness", "SCALAR", -1700, 40)
            base_normal = inp(function, "BaseNormal", "VECTOR3", -1700, 290)
            amount = inp(function, "FrostAmount", "SCALAR", -1700, 520)
            flatten = inp(function, "NormalFlattenStrength", "SCALAR", -1700, 780)
            alpha = saturate(function, amount, -1300, 570)
            color = lerp(function, base, frost_color, alpha, -820, -480)
            roughness = lerp(function, base_rough, frost_rough, alpha, -820, -190)
            flat_normal = rgb(function, 0.0, 0.0, 1.0, -1300, 1130)
            normal_weight = saturate(function, binary(function, "Multiply", alpha, flatten, -970, 760), -740, 750)
            normal = lerp(function, base_normal, flat_normal, normal_weight, -530, 410)
            normalized = unary(function, "Normalize", normal, -300, 430)
            output(function, color, "BaseColor", 130, -440)
            output(function, roughness, "Roughness", 130, -100)
            output(function, normalized, "Normal", 130, 430)
        else:
            raise ValueError("未实现函数配方：" + recipe)

    return build


def main() -> None:
    """成功保存真实UE资产后才打印CREATED；仅通过inspect不可升为已制作。"""
    parser = argparse.ArgumentParser(description="UE5.8 通用材质首批5函数制作与审查")
    parser.add_argument("--mode", choices=("inspect", "apply", "verify"),
                        default=os.getenv("MATERIAL_CORE_AUTHOR_MODE", "inspect"))
    options, _ = parser.parse_known_args()

    print("PROJECT", ROOT)
    for directory, name, recipe, _, chinese in FUNCTIONS:
        print("CORE_FUNCTION_PLAN", target_path(directory, name), recipe, chinese)
    if options.mode == "inspect":
        print("INSPECT_ONLY：本轮未创建任何.uasset（虚幻材质资产）")
        return

    try:
        import unreal  # type: ignore[import-not-found]
    except ImportError as error:
        raise RuntimeError("仅允许在真正的UE5.8编辑器执行apply/verify") from error

    require_project(unreal)
    lib = unreal.EditorAssetLibrary
    editor = unreal.MaterialEditingLibrary
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([SURFACE_ROOT], force_rescan=True)
    factory = unreal.MaterialFunctionFactoryNew()
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    build = create_author(unreal)

    # 全部目标首先做保护性预检，先确认没有重名其他类别或未被注册表发现的物理资产。
    for folder, name, _, _, _ in FUNCTIONS:
        package = target_path(folder, name)
        asset = lib.load_asset(package) if lib.does_asset_exist(package) else None
        if asset is not None and not isinstance(asset, unreal.MaterialFunction):
            raise TypeError("已有同名包但不是MaterialFunction：" + package)
        physical = on_disk_package(package)
        if asset is None and any(physical.with_suffix(suffix).exists() for suffix in (".uasset", ".uexp", ".ubulk", ".umap")):
            raise RuntimeError("磁盘资源或侧车文件未注册但物理包已占用，禁止创建覆盖：" + package)
        if options.mode == "verify" and asset is None:
            raise FileNotFoundError("UE尚未制作目标函数：" + package)

    for folder, name, recipe, minimum_nodes, chinese in FUNCTIONS:
        package = target_path(folder, name)
        existing = lib.does_asset_exist(package)
        if existing:
            asset = lib.load_asset(package)
            if not isinstance(asset, unreal.MaterialFunction):
                raise TypeError("UE真实材质函数类别错误：" + package)
            node_count = editor.get_num_material_expressions_in_function(asset)
            if node_count < minimum_nodes:
                raise RuntimeError(f"{chinese}已存在但真实图节点不足，拒绝覆盖：{package} nodes={node_count}")
            print("CORE_FUNCTION_VERIFIED_EXISTING", package, "nodes=", node_count)
            continue
        if options.mode != "apply":
            raise RuntimeError("verify禁止创建缺失函数：" + package)
        destination = package.rsplit("/", 1)[0]
        if not lib.does_directory_exist(destination):
            lib.make_directory(destination)
        asset = tools.create_asset(name, destination, unreal.MaterialFunction, factory)
        if not isinstance(asset, unreal.MaterialFunction):
            raise RuntimeError("UE没有成功创建函数：" + package)
        build(asset, recipe)
        nodes = editor.get_num_material_expressions_in_function(asset)
        if nodes < minimum_nodes:
            raise RuntimeError(f"新函数节点异常，未保存：{package} nodes={nodes}")
        editor.update_material_function(asset)
        if not lib.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError("材质函数原生保存失败：" + package)
        loaded = lib.load_asset(package)
        if not isinstance(loaded, unreal.MaterialFunction):
            raise RuntimeError("材质函数保存后回读失败：" + package)
        if editor.get_num_material_expressions_in_function(loaded) < minimum_nodes:
            raise RuntimeError("材质函数回读节点数不足：" + package)
        print("CORE_FUNCTION_CREATED", package, "nodes=", nodes)

    print("CORE_SURFACE_FUNCTIONS_DONE：5项已处理；Shader/Cook/人工视觉仍须单独核验")


if __name__ == "__main__":
    main()
