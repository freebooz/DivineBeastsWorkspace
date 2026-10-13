#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8平台VFX通用材质函数首批：UV流动、溶解、径向能量遮罩。

此脚本只负责视觉材质表达式，不实现Niagara发射、碰撞、运动或生命周期。
GamePlatformVFX（平台视觉特效）仍是唯一运行时机制。
默认inspect（打印真实目标）、apply（UE编辑器内仅创建缺失资产）、
verify（UE编辑器内只读检查类型及表达式数量）。
所有.uasset只允许UE真实MaterialEditingLibrary创建、保存与回读；
现有同名资产只做验证，禁止覆盖、迁移或重建。
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PLUGIN = "/GamePlatformVFX"
FOLDER = PLUGIN + "/Materials/Functions"
CONTENT = ROOT / "Game" / "Plugins" / "GamePlatform" / "Presentation" / "GamePlatformVFX" / "Content"

FUNCTIONS = (
    ("MF_GP_VFX_FlowUV", "uvflow", 7, "二维纹理坐标匀速流动"),
    ("MF_GP_VFX_Dissolve", "dissolve", 12, "噪声溶解与边缘发光遮罩"),
    ("MF_GP_VFX_RadialEnergyGradient", "radial", 10, "径向能量衰减遮罩"),
)


def require_ue_project(unreal) -> None:
    """不允许用已打开的其它虚幻工程创建同名插件资产。"""
    path = str(unreal.Paths.get_project_file_path()).replace("\\", "/")
    if not path.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("非正式神兽联盟UE主工程：" + path)


def build_graph(unreal, fn, recipe: str) -> None:
    """明确连接真实运算节点；图不依赖项目Niagara定义，便于跨项目复用。"""
    editing = unreal.MaterialEditingLibrary

    def expr(type_name: str, x: int, y: int):
        klass = getattr(unreal, "MaterialExpression" + type_name, None)
        if klass is None:
            raise RuntimeError("UE5.8材质表达式不可用：" + type_name)
        node = editing.create_material_expression_in_function(fn, klass, x, y)
        if node is None:
            raise RuntimeError("引擎未创建材质节点：" + type_name)
        return node

    def connect(a, b, input_name: str):
        ok = editing.connect_material_expressions(a, "", b, input_name)
        if not ok and input_name in {"Input", "VectorInput"}:
            ok = editing.connect_material_expressions(a, "", b, "")
        if not ok:
            raise RuntimeError("VFX材质图节点接线失败：" + str(a.get_name()) + " -> " + str(b.get_name()) + "." + input_name)

    def fn_input(name: str, kind: str, x: int, y: int):
        node = expr("FunctionInput", x, y)
        enum = getattr(unreal.FunctionInputType, "FUNCTION_INPUT_" + kind, None)
        if enum is None:
            raise RuntimeError("引擎不支持函数输入类型：" + kind)
        node.set_editor_property("input_name", name)
        node.set_editor_property("input_type", enum)
        node.set_editor_property("use_preview_value_as_default", False)
        return node

    def value(number: float, x: int, y: int):
        node = expr("Constant", x, y)
        node.set_editor_property("r", number)
        return node

    def op(name: str, a, b, x: int, y: int):
        node = expr(name, x, y)
        connect(a, node, "A")
        connect(b, node, "B")
        return node

    def unary(name: str, a, x: int, y: int):
        node = expr(name, x, y)
        connect(a, node, "Input")
        return node

    def save_output(src, name: str, x: int, y: int):
        out = expr("FunctionOutput", x, y)
        out.set_editor_property("output_name", name)
        connect(src, out, "Input")

    def safe_div(a, width, x: int, y: int):
        epsilon = value(0.0001, x - 380, y + 230)
        safe = op("Max", width, epsilon, x - 230, y + 120)
        return op("Divide", a, safe, x, y)

    if recipe == "uvflow":
        uv = fn_input("UV", "VECTOR2", -1200, -480)
        velocity = fn_input("VelocityUVPerSecond", "VECTOR2", -1200, -240)
        seconds = fn_input("TimeSeconds", "SCALAR", -1200, 0)
        tiling = fn_input("Tiling", "SCALAR", -1200, 240)
        tiled = op("Multiply", uv, tiling, -820, -390)
        travel = op("Multiply", velocity, seconds, -820, -70)
        result = op("Add", tiled, travel, -470, -270)
        save_output(result, "UV", -150, -270)
    elif recipe == "dissolve":
        noise = fn_input("NoiseMask", "SCALAR", -1350, -490)
        progress = fn_input("Progress", "SCALAR", -1350, -220)
        width = fn_input("EdgeWidth", "SCALAR", -1350, 40)
        delta = op("Subtract", noise, progress, -990, -400)
        alpha = unary("Saturate", safe_div(delta, width, -640, -430), -380, -430)
        edge_start = op("Subtract", progress, width, -990, 50)
        outer_delta = op("Subtract", noise, edge_start, -770, 60)
        outer = unary("Saturate", safe_div(outer_delta, width, -470, 20), -210, 20)
        edge_mask = op("Subtract", outer, alpha, 20, -120)
        save_output(alpha, "VisibilityMask", 270, -420)
        save_output(unary("Saturate", edge_mask, 250, -90), "EdgeMask", 490, -90)
    elif recipe == "radial":
        uv = fn_input("UV", "VECTOR2", -1200, -480)
        center = fn_input("CenterUV", "VECTOR2", -1200, -250)
        radius = fn_input("RadiusUV", "SCALAR", -1200, 0)
        feather = fn_input("EdgeSoftnessUV", "SCALAR", -1200, 250)
        radial_distance = op("Distance", uv, center, -830, -390)
        outside = op("Subtract", radial_distance, radius, -560, -220)
        ratio = safe_div(outside, feather, -240, -150)
        falloff = unary("Saturate", ratio, 30, -110)
        inside = unary("OneMinus", falloff, 250, -110)
        save_output(inside, "RadialMask", 460, -110)
    else:
        raise ValueError("未知VFX材质配方：" + recipe)


def main() -> None:
    """每个函数单独保存回读；如果出现部分完成，不自动覆盖或清理已有资源。"""
    parser = argparse.ArgumentParser(description="UE5.8 VFX通用材质3函数真实制作工具")
    parser.add_argument("--mode", choices=("inspect", "apply", "verify"),
                        default=os.getenv("VFX_MATERIAL_LIBRARY_MODE", "inspect"))
    opts, _ = parser.parse_known_args()
    for name, recipe, _, meaning in FUNCTIONS:
        print("VFX_MATERIAL_FUNCTION_TARGET", FOLDER + "/" + name, recipe, meaning)
    if opts.mode == "inspect":
        print("INSPECT_ONLY：未生成UE二进制材质资产")
        return

    try:
        import unreal  # type: ignore[import-not-found]
    except ImportError as err:
        raise RuntimeError("VFX函数必须在正式UE5.8编辑器运行") from err

    require_ue_project(unreal)
    assets = unreal.EditorAssetLibrary
    editor = unreal.MaterialEditingLibrary
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    reg.scan_paths_synchronous([PLUGIN], force_rescan=True)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialFunctionFactoryNew()

    # 创建前统一审查目标：磁盘资源被占用但注册表未发现时必须拒绝写。
    for name, _, _, _ in FUNCTIONS:
        path = FOLDER + "/" + name
        package = CONTENT / (path.removeprefix(PLUGIN + "/") + ".uasset")
        found = assets.load_asset(path) if assets.does_asset_exist(path) else None
        if found is not None and not isinstance(found, unreal.MaterialFunction):
            raise TypeError("原有资产类别冲突：" + path)
        if found is None and any(package.with_suffix(suffix).exists() for suffix in (".uasset", ".uexp", ".ubulk", ".umap")):
            raise RuntimeError("现有物理包或侧车文件未被注册，拒绝同名写入：" + str(package))
        if opts.mode == "verify" and found is None:
            raise FileNotFoundError("尚未创建UE材质函数：" + path)

    for name, recipe, required_nodes, _ in FUNCTIONS:
        path = FOLDER + "/" + name
        if assets.does_asset_exist(path):
            function = assets.load_asset(path)
            if not isinstance(function, unreal.MaterialFunction):
                raise TypeError("不是UE材质函数：" + path)
            size = editor.get_num_material_expressions_in_function(function)
            if size < required_nodes:
                raise RuntimeError(f"已有材质函数图不完整：{path} node_count={size}")
            print("VFX_FUNCTION_EXISTS_VERIFIED", path, size)
            continue

        if opts.mode != "apply":
            raise RuntimeError("verify是只读模式：" + path)
        if not assets.does_directory_exist(FOLDER):
            assets.make_directory(FOLDER)
        function = tools.create_asset(name, FOLDER, unreal.MaterialFunction, factory)
        if not isinstance(function, unreal.MaterialFunction):
            raise RuntimeError("UE无法创建材质函数：" + path)
        build_graph(unreal, function, recipe)
        size = editor.get_num_material_expressions_in_function(function)
        if size < required_nodes:
            raise RuntimeError("真实函数节点不足，不保存：" + path)
        editor.update_material_function(function)
        if not assets.save_loaded_asset(function, only_if_is_dirty=False):
            raise RuntimeError("UE材质函数保存失败：" + path)
        reopened = assets.load_asset(path)
        if not isinstance(reopened, unreal.MaterialFunction):
            raise RuntimeError("保存后函数回读失败：" + path)
        if editor.get_num_material_expressions_in_function(reopened) < required_nodes:
            raise RuntimeError("回读后节点缺失：" + path)
        print("VFX_FUNCTION_CREATED", path, size)

    print("VFX_FUNCTIONS_PROCESSED：Shader与Niagara真实效果须在母材质与演示中单独验证")


if __name__ == "__main__":
    main()
