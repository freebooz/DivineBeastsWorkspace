#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 平台通用 PBR 覆盖材质函数原生作者。

归属 GamePlatformSurface（平台表现层），由 UE5.8 编辑器运行。
用于地形、岩石和建筑的基础色／切线空间法线／粗糙度／环境遮蔽四通道覆盖。
不携带项目贴图、天气事实、服务器权威行为、RVT 假采样或项目级参数。
默认 inspect（只读），apply 只新建不覆写，verify 在独立编辑器核验接口。
"""
from __future__ import annotations

import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MODE = os.environ.get("TERRAIN_PBR_BLEND_FUNCTION_MODE", "inspect").strip().lower()
PACKAGE = "/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_PBRLayerBlend"
DISK = (ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Content"
        / "MaterialFunctions/Layers/MF_GP_PBRLayerBlend.uasset")
INPUTS = {
    "BaseColor": "VECTOR3",
    "OverlayColor": "VECTOR3",
    "BaseNormal": "VECTOR3",
    "OverlayNormal": "VECTOR3",
    "BaseRoughness": "SCALAR",
    "OverlayRoughness": "SCALAR",
    "BaseAO": "SCALAR",
    "OverlayAO": "SCALAR",
    "Coverage": "SCALAR",
}
OUTPUTS = ("BaseColor", "Normal", "Roughness", "AO")


def run() -> None:
    if MODE not in {"inspect", "apply", "verify"}:
        raise ValueError("TERRAIN_PBR_BLEND_FUNCTION_MODE 只允许 inspect/apply/verify")
    if MODE == "inspect":
        print("TERRAIN_PBR_BLEND_FUNCTION_PLAN", PACKAGE, "PBR=Color/Normal/Roughness/AO")
        return
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("真实材质函数必须由 UE5.8 Editor 创建或核验") from exc

    project = str(unreal.Paths.get_project_file_path()).replace("\\", "/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("禁止在其他项目生成通用平台函数")

    lib = unreal.EditorAssetLibrary
    edit = unreal.MaterialEditingLibrary

    def verify() -> None:
        func = lib.load_asset(PACKAGE)
        if not isinstance(func, unreal.MaterialFunction):
            raise RuntimeError("PBR覆盖函数并非真实原生 MaterialFunction")
        expressions = list(edit.get_material_function_expressions(func))
        inputs = {}
        outputs = {}
        for expression in expressions:
            if isinstance(expression, unreal.MaterialExpressionFunctionInput):
                name = str(expression.get_editor_property("input_name"))
                inputs[name] = str(expression.get_editor_property("input_type"))
            elif isinstance(expression, unreal.MaterialExpressionFunctionOutput):
                name = str(expression.get_editor_property("output_name"))
                outputs[name] = expression
        if set(inputs) != set(INPUTS) or set(outputs) != set(OUTPUTS):
            raise RuntimeError("PBR函数输入输出合同不一致：" + str((inputs, list(outputs))))
        for name, signature in INPUTS.items():
            if signature not in inputs[name].upper():
                raise RuntimeError("PBR函数引脚数据类型不一致：" + name + " " + inputs[name])
        if len(expressions) < 19:
            raise RuntimeError("PBR函数内部图计算节点不足：" + str(len(expressions)))
        print("TERRAIN_PBR_BLEND_FUNCTION_VERIFIED",
              "inputs=", len(inputs), "outputs=", len(outputs),
              "expressions=", len(expressions))

    if lib.does_asset_exist(PACKAGE):
        verify()
        return
    if MODE == "verify":
        raise RuntimeError("PBR材质函数尚未创建")
    if DISK.exists():
        raise RuntimeError("磁盘已有未登记的同名平台函数，拒绝覆盖：" + str(DISK))

    folder, name = PACKAGE.rsplit("/", 1)
    if not lib.does_directory_exist(folder):
        lib.make_directory(folder)
    function = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
    if not isinstance(function, unreal.MaterialFunction):
        raise RuntimeError("MaterialFunction 资产创建失败")

    def node(kind, x, y):
        expression = edit.create_material_expression_in_function(function, kind, x, y)
        if expression is None:
            raise RuntimeError("平台 PBR 材质函数节点创建失败：" + str(kind))
        return expression

    def wire(left, right, pin):
        ok = edit.connect_material_expressions(left, "", right, pin)
        if not ok and pin in ("Input", "VectorInput"):
            ok = edit.connect_material_expressions(left, "", right, "")
        if not ok:
            raise RuntimeError("平台 PBR 材质函数连线失败：" + left.get_name() + "→" + pin)

    input_nodes = {}
    for index, (name, typ) in enumerate(INPUTS.items()):
        expression = node(unreal.MaterialExpressionFunctionInput,
                          -1400, index * 220 - 920)
        expression.set_editor_property("input_name", unreal.Name(name))
        expression.set_editor_property(
            "input_type", getattr(unreal.FunctionInputType, "FUNCTION_INPUT_" + typ))
        expression.set_editor_property("use_preview_value_as_default", False)
        input_nodes[name] = expression

    alpha = node(unreal.MaterialExpressionSaturate, -1040, 1030)
    wire(input_nodes["Coverage"], alpha, "Input")

    def blend(name, a, b, x, y):
        expression = node(unreal.MaterialExpressionLinearInterpolate, x, y)
        wire(input_nodes[a], expression, "A")
        wire(input_nodes[b], expression, "B")
        wire(alpha, expression, "Alpha")
        return expression

    color = blend("Color", "BaseColor", "OverlayColor", -650, -850)
    mixed_normal = blend("Normal", "BaseNormal", "OverlayNormal", -650, -380)
    normal = node(unreal.MaterialExpressionNormalize, -410, -370)
    wire(mixed_normal, normal, "VectorInput")
    rough = blend("Roughness", "BaseRoughness", "OverlayRoughness", -650, 200)
    ao_mixed = blend("AO", "BaseAO", "OverlayAO", -650, 680)
    ao = node(unreal.MaterialExpressionSaturate, -390, 670)
    wire(ao_mixed, ao, "Input")
    outputs = {
        "BaseColor": color,
        "Normal": normal,
        "Roughness": rough,
        "AO": ao,
    }
    for index, (name, expression) in enumerate(outputs.items()):
        output = node(unreal.MaterialExpressionFunctionOutput, 50, index * 500 - 850)
        output.set_editor_property("output_name", unreal.Name(name))
        wire(expression, output, "Input")
    edit.update_material_function(function)
    if not lib.save_loaded_asset(function, only_if_is_dirty=False):
        raise RuntimeError("平台 PBR 材质函数保存失败")
    verify()


if __name__ == "__main__":
    run()
