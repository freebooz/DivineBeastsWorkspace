#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟通用UI材质函数库：12项原生MD_UI审核Material制作。

只允许经Monolith连接的正式UE5.8 Editor执行，物理归属为项目Development审核
目录，所有通用MF继续由GamePlatformUI（平台UI插件）拥有。
此作者负责创建UI Material及真实MaterialFunctionCall、参数、最终颜色和透明度连接。
默认inspect只列目标；apply仅创建缺失的MD_UI审核材质；verify只读回读。
不创建项目WBP（Widget蓝图仍须Monolith ui_query创建），不修改现有项目主题，
不编译蓝图、Shader或客户端/服务器，最终统一运行正式编译/Cook/人工视觉验收。
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
RECIPE = ROOT / "Tools/Unreal/UI/MaterialLibraryUIMonolithRecipes_V1.json"
INDEX = ROOT / "Docs/Implementation/Materials/MaterialCapabilityRegistry_V1.json"
QA_ROOT = "/Game/Development/MaterialLibrary/UI/Materials"
QA_DISK = ROOT / "Game/Content/Development/MaterialLibrary/UI/Materials"
FUNCTION_ROOT = "/GamePlatformUI/Materials/Functions/MF_GP_UI_"

# 既有已真实创建的五项平台MF，由本专用审核材质直接消费。
EXISTING = {
    "SDFRoundRect": (
        ("UV", "vector2"), ("HalfSize", "vector2"),
        ("CornerRadius", "scalar"), ("Softness", "scalar"),
    ),
    "GradientBackground": (
        ("ColorA", "vector3"), ("ColorB", "vector3"), ("GradientAlpha", "scalar"),
    ),
    "RadialCooldown": (
        ("UV", "vector2"), ("CenterUV", "vector2"),
        ("Progress", "scalar"), ("Clockwise", "scalar"),
    ),
    "OutlineGlow": (
        ("InnerMask", "scalar"), ("OuterMask", "scalar"), ("GlowStrength", "scalar"),
    ),
    "ButtonShimmer": (
        ("UV", "vector2"), ("TimeSeconds", "scalar"),
        ("ScrollSpeed", "scalar"), ("ShimmerWidth", "scalar"),
    ),
}
DEFAULT_SCALAR = {
    "GradientAlpha": .5, "CornerRadius": .09, "Softness": .01,
    "Progress": .62, "Clockwise": 1.0,
    "InnerMask": .52, "OuterMask": .84, "GlowStrength": 1.25,
    "ScrollSpeed": .30, "ShimmerWidth": .12,
    "BorderWidth": .06, "IconAlpha": 1.0, "ThemeMask": 1.0,
    "Opacity": .94, "HealthRatio": .75, "FlowSpeed": .3, "StripeDensity": 12.0,
    "SelectionMask": .30, "EdgeMask": .75, "PulseSpeed": 2.4,
    "ClickProgress": .55, "RippleRadius": .33, "RippleWidth": .12,
    "NoiseMask": .8, "EdgeWidth": .08,
}
DEFAULT_VECTOR2 = {
    "HalfSize": (.42, .42), "CenterUV": (.5, .5),
    "ClickCenterUV": (.5, .5), "RippleCenterUV": (.5, .5),
}
DEFAULT_VECTOR3 = {
    "ColorA": (.035, .14, .20),
    "ColorB": (.33, .61, .47),
    "HealthyColor": (.20, .75, .31),
    "CriticalColor": (.90, .23, .16),
    "TintColor": (.28, .73, .56),
}


def load_specs() -> list[dict]:
    """校验已有5项与余7项配方共12条，没有UI域跨层资源漂移。"""
    original = json.loads(RECIPE.read_text(encoding="utf-8"))
    registry = json.loads(INDEX.read_text(encoding="utf-8"))
    if original.get("schemaVersion") != 1 or len(original.get("items", [])) != 7:
        raise ValueError("平台UI剩余配方必须是七项")
    if registry.get("candidateCount") != 201 or len(registry.get("items", [])) != 201:
        raise ValueError("201项机器索引真源不存在或不完整")
    registered = {row["NameEN"]: row for row in registry["items"]
                  if row["OwnerPlugin"] == "GamePlatformUI"}
    specs = []
    for name, pins in EXISTING.items():
        specs.append({"name": name, "inputs": pins})
    for row in original["items"]:
        pins = tuple((p["name"], p["type"]) for p in row["graphSpec"]["inputs"])
        specs.append({"name": row["name"], "inputs": pins})
    if len(specs) != 12 or len({row["name"] for row in specs}) != 12:
        raise ValueError("通用UI材质函数配方必须唯一且恰好十二项")
    for spec in specs:
        row = registered.get(spec["name"])
        if row is None or row["ImplementationMode"] != "MF":
            raise ValueError("UI材质函数没有正式归属：" + spec["name"])
        if row["ProposedAssetPath"] != FUNCTION_ROOT + spec["name"]:
            raise ValueError("UI真实函数路径偏离201项合同：" + spec["name"])
        for pin_name, type_name in spec["inputs"]:
            if type_name not in ("scalar", "vector2", "vector3"):
                raise ValueError("不支持的UI函数输入类型：" + spec["name"] + "." + pin_name)
    return specs


def qa_path(name: str) -> str:
    """所有中性审核样品必须在游戏项目第三层Development下，与正式UI隔离。"""
    return QA_ROOT + "/M_DBA_UI_" + name + "_Review"


def require_editor(unreal) -> None:
    """不能在普通Python假造UMaterial，也不能误操作其他用户UE工程。"""
    project = str(unreal.Paths.get_project_file_path())
    expected = str(ROOT / "Game/DivineBeastsArena.uproject")
    if os.path.normcase(os.path.realpath(project)) != os.path.normcase(os.path.realpath(expected)):
        raise RuntimeError("非锁定DivineBeastsWorkspace正式工程：" + project)
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("必须由UE5.8原生编辑器创建12份UI审核材质")


def check_asset(unreal, spec: dict, require_review: bool) -> tuple:
    """检查原函数和审核Material的原生类别、UI域及调用者身份；不写已有资源。"""
    lib = unreal.EditorAssetLibrary
    edit = unreal.MaterialEditingLibrary
    fp = FUNCTION_ROOT + spec["name"]
    fn = lib.load_asset(fp)
    if not isinstance(fn, unreal.MaterialFunction):
        raise TypeError("平台通用UI材质函数未实际生成：" + fp)
    native_inputs = {
        str(exp.get_editor_property("input_name"))
        for exp in edit.get_material_function_expressions(fn)
        if isinstance(exp, unreal.MaterialExpressionFunctionInput)
    }
    needed = {name for name, _ in spec["inputs"]}
    if native_inputs != needed:
        raise RuntimeError("真实UI MF输入与正式配方不一致：" + fp +
                           " inputs=" + repr(sorted(native_inputs)))

    path = qa_path(spec["name"])
    mat = lib.load_asset(path) if lib.does_asset_exist(path) else None
    if mat is None:
        physical = QA_DISK / (path.rsplit("/", 1)[-1] + ".uasset")
        if any(physical.with_suffix(ext).exists()
               for ext in (".uasset", ".uexp", ".ubulk", ".umap")):
            raise RuntimeError("审核包被未注册物理资产占用，禁止创建：" + str(physical))
        if require_review:
            raise FileNotFoundError("需要真正UE原生MD_UI审核Material：" + path)
        return fn, None
    if not isinstance(mat, unreal.Material):
        raise TypeError("已有审核包不是UMaterial：" + path)
    physical = QA_DISK / (path.rsplit("/", 1)[-1] + ".uasset")
    if not physical.is_file():
        raise RuntimeError("审核材质仅在内存中注册且未真实落盘，禁止跳过：" + path)
    if mat.get_editor_property("material_domain") != unreal.MaterialDomain.MD_UI:
        raise RuntimeError("现有审核材质不是MD_UI界面材质域：" + path)
    call_nodes = [node for node in edit.get_material_expressions(mat)
                  if isinstance(node, unreal.MaterialExpressionMaterialFunctionCall)]
    if len(call_nodes) != 1:
        raise RuntimeError("已有审核材质函数调用节点数量错误：" + path)
    selected = call_nodes[0].get_editor_property("material_function")
    if selected is None or selected.get_path_name().split(".")[0] != fp:
        raise RuntimeError("已有审核材质引用的UI函数不匹配：" + path)
    if edit.get_num_material_expressions(mat) < len(needed) + 2:
        raise RuntimeError("现有UI审核材质图未包含必要真实节点：" + path)
    return fn, mat


def create_review(unreal, spec: dict) -> None:
    """真正构造在用户界面域的颜色/透明输出，不把函数空引用当交付。"""
    edit = unreal.MaterialEditingLibrary
    lib = unreal.EditorAssetLibrary
    fn, existing = check_asset(unreal, spec, require_review=False)
    path = qa_path(spec["name"])
    if existing is not None:
        print("UI_REVIEW_EXISTING_PRESERVED", path)
        return

    if not lib.does_directory_exist(QA_ROOT):
        lib.make_directory(QA_ROOT)
    name = path.rsplit("/", 1)[-1]
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, QA_ROOT, unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(mat, unreal.Material):
        raise RuntimeError("UE5.8未创建原生UI审核Material：" + path)
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    def expression(class_name: str, x: int, y: int):
        klass = getattr(unreal, "MaterialExpression" + class_name, None)
        if klass is None:
            raise RuntimeError("UE5.8缺少原生UI表达式：" + class_name)
        result = edit.create_material_expression(mat, klass, x, y)
        if result is None:
            raise RuntimeError("没有创建实际UI材质节点：" + class_name)
        return result

    def link(src, out_name: str, target, pin_name: str):
        ok = edit.connect_material_expressions(src, out_name, target, pin_name)
        if not ok and pin_name in ("Input", "VectorInput"):
            ok = edit.connect_material_expressions(src, out_name, target, "")
        if not ok:
            raise RuntimeError("UI材质表达式连接失败：" + spec["name"] + "." + pin_name)

    def scalar(value: float, x: int, y: int):
        result = expression("Constant", x, y)
        result.set_editor_property("r", float(value))
        return result

    def vector3(value, x: int, y: int):
        result = expression("Constant3Vector", x, y)
        result.set_editor_property("constant",
                                   unreal.LinearColor(*map(float, value), 1.0))
        return result

    def vector2(value, x: int, y: int):
        result = expression("Constant2Vector", x, y)
        result.set_editor_property("r", float(value[0]))
        result.set_editor_property("g", float(value[1]))
        return result

    def input_expression(pin_name: str, pin_type: str, x: int, y: int):
        # 时间及UV由引擎动态提供，避免全部审核样品变成无动画常量球体。
        if pin_name == "UV":
            return expression("TextureCoordinate", x, y)
        if pin_name == "TimeSeconds":
            return expression("Time", x, y)
        if pin_name == "GradientAlpha":
            uv = expression("TextureCoordinate", x - 230, y)
            component = expression("ComponentMask", x, y)
            component.set_editor_property("g", True)
            link(uv, "", component, "Input")
            return component
        if pin_type == "scalar":
            return scalar(DEFAULT_SCALAR.get(pin_name, .58), x, y)
        if pin_type == "vector2":
            return vector2(DEFAULT_VECTOR2.get(pin_name, (.5, .5)), x, y)
        if pin_type == "vector3":
            return vector3(DEFAULT_VECTOR3.get(pin_name, (.3, .58, .65)), x, y)
        raise ValueError("审核材质不支持的输入类型：" + pin_type)

    call = expression("MaterialFunctionCall", -450, -260)
    if not call.set_material_function(fn):
        raise RuntimeError("无法把真实UI函数绑定到调用节点：" + fn.get_path_name())
    for idx, (input_name, input_type) in enumerate(spec["inputs"]):
        source = input_expression(input_name, input_type,
                                  -1150, -740 + idx * 175)
        link(source, "", call, input_name)

    output_nodes = [exp for exp in edit.get_material_function_expressions(fn)
                    if isinstance(exp, unreal.MaterialExpressionFunctionOutput)]
    outputs = {str(exp.get_editor_property("output_name")) for exp in output_nodes}
    if not outputs:
        raise RuntimeError("当前MF没有实际函数输出：" + fn.get_path_name())

    # UI渲染只使用最终颜色与透明度；独立的GlowMask/EdgeMask用颜色乘法预览。
    if "Color" in outputs:
        color_src, color_out = call, "Color"
    else:
        out_name = ("EdgeMask" if "EdgeMask" in outputs else
                    "GlowMask" if "GlowMask" in outputs else
                    next(iter(sorted(outputs))))
        multiply = expression("Multiply", 80, -280)
        link(call, out_name, multiply, "A")
        link(vector3((.24, .75, .63), -240, 180), "", multiply, "B")
        color_src, color_out = multiply, ""
    if not edit.connect_material_property(color_src, color_out,
                                          unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("界面材质未接通Final Color（最终颜色）：" + path)

    mask_name = (
        "VisibilityMask" if "VisibilityMask" in outputs else
        "Mask" if "Mask" in outputs else
        "BorderMask" if "BorderMask" in outputs else
        "RadialMask" if "RadialMask" in outputs else None
    )
    opacity_source = call if mask_name else scalar(1.0, -220, 420)
    if not edit.connect_material_property(opacity_source, mask_name or "",
                                          unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError("界面材质未接通Opacity（透明度）：" + path)
    if not lib.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError("UE5.8未保存实际UI审核材质：" + path)
    again = lib.load_asset(path)
    if not isinstance(again, unreal.Material) or (
        again.get_editor_property("material_domain") != unreal.MaterialDomain.MD_UI):
        raise RuntimeError("原生UI Material保存回读失败：" + path)
    print("UI_REVIEW_MATERIAL_CREATED_SAVED_NO_COMPILE", path,
          "nodes", edit.get_num_material_expressions(again))


def main() -> None:
    """制作阶段不编译，接受原生UI材质函数/母版读档后由统一门禁测试Shader。"""
    parser = argparse.ArgumentParser(description="SSLM 12项UE原生MD_UI开发审核材质作者")
    parser.add_argument("--mode", choices=("inspect", "apply", "verify"),
                        default="inspect")
    parser.add_argument("--start-index",type=int,default=0)
    parser.add_argument("--limit",type=int,default=0,help="0表示12项全部；非零为单次审查母版批次")
    options, _ = parser.parse_known_args()
    complete = load_specs()
    if options.start_index<0 or options.start_index>len(complete) or options.limit<0:
        raise ValueError("UI审核材质批次范围非法")
    specs=complete[options.start_index:options.start_index+options.limit] if options.limit else complete[options.start_index:]
    for spec in specs:
        print("UI_REVIEW_AUTHOR_TARGET", spec["name"], qa_path(spec["name"]))
    if options.mode == "inspect":
        print("INSPECT_ONLY：不写UE资源；正式UI函数和MD_UI原生作者待编辑器执行")
        return
    try:
        import unreal  # type: ignore[import-not-found]
    except ImportError as error:
        raise RuntimeError("必须通过真实UE5.8/Monolith编辑器创建UI母材质") from error
    require_editor(unreal)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/GamePlatformUI", QA_ROOT], force_rescan=True)
    for spec in specs:
        check_asset(unreal, spec, options.mode == "verify")
    if options.mode == "verify":
        print("UI_REVIEW_ASSETS_VERIFIED", len(specs))
        return
    for spec in specs:
        create_review(unreal, spec)
    print("UI_REVIEW_AUTHOR_COMPLETE", len(specs), "FINAL_SHADER_COMPILE_DEFERRED")


if __name__ == "__main__":
    main()
