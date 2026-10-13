#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 通用材质函数库中性审核用材质作者。

归属：DivineBeasts（项目层）Development（仅开发审核）内容，不向平台资源反向引用项目。
目的：将首批8个MF（材质函数）逐项接入真正的UMaterial属性，调用编辑器原生着色器编译、
保存与原生回读，从而识别无消费者情况下函数图可能隐藏的编译错误。
模式：inspect（仅目标预览）/apply（新增审核材质）/verify（只读检查已有图和重新编译）。
禁止：覆盖现有正式游戏或审核资产、移动既有路径、生成文本假uasset、替代人工美术验收。
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
QA_ROOT = "/Game/Development/MaterialLibrary/Materials"
QA_DISK_ROOT = ROOT / "Game" / "Content" / "Development" / "MaterialLibrary" / "Materials"

FUNCTIONS = (
    ("HeightBlendMask", "Core", "/GamePlatformSurface/MaterialFunctions/Core/MF_GP_HeightBlendMask"),
    ("MacroMicroBlend", "Core", "/GamePlatformSurface/MaterialFunctions/Core/MF_GP_MacroMicroBlend"),
    ("LandscapeBiomeMask", "Landscape", "/GamePlatformSurface/MaterialFunctions/Landscape/MF_GP_LandscapeBiomeMask"),
    ("LandscapeDistanceDetail", "Landscape", "/GamePlatformSurface/MaterialFunctions/Landscape/MF_GP_LandscapeDistanceDetail"),
    ("FrostSurface", "Layers", "/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_FrostSurface"),
    ("VFXFlowUV", "VFX", "/GamePlatformVFX/Materials/Functions/MF_GP_VFX_FlowUV"),
    ("VFXDissolve", "VFX", "/GamePlatformVFX/Materials/Functions/MF_GP_VFX_Dissolve"),
    ("VFXRadialEnergy", "VFX", "/GamePlatformVFX/Materials/Functions/MF_GP_VFX_RadialEnergyGradient"),
)


def asset_path(code: str) -> str:
    """审核专属路径，不参与正式主流程或服务器内容。"""
    return f"{QA_ROOT}/M_DBA_MaterialLibrary_{code}_Review"


def require_project(unreal) -> None:
    """确保仅在用户指定的唯一主工程执行可写的编辑器动作。"""
    actual = str(unreal.Paths.get_project_file_path()).replace("\\", "/")
    if not actual.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("不是正式神兽联盟UE5.8主工程：" + actual)


def inspect_review_graph(unreal, material, expected_function_path: str) -> int:
    """检查真实函数调用节点身份，禁止用无关空白Material假装已经集成。"""
    edit = unreal.MaterialEditingLibrary
    expressions = list(edit.get_material_expressions(material))
    calls = [e for e in expressions if isinstance(e, unreal.MaterialExpressionMaterialFunctionCall)]
    if len(calls) != 1:
        raise RuntimeError("审核材质必须含且仅含一个函数调用节点：" + material.get_path_name())
    selected = calls[0].get_editor_property("material_function")
    if selected is None or selected.get_path_name().split(".")[0] != expected_function_path:
        raise RuntimeError("审核材质引用的MF与台账身份不一致：" + material.get_path_name())
    if len(expressions) < 5:
        # 最小合法函数消费图是一个FunctionCall（材质函数调用）加四个独立参数节点。
        raise RuntimeError("审核材质图节点不足，禁止空白材质假通过")
    return len(expressions)


def build(unreal, material, code: str, function) -> None:
    """建立实际Shader消费图，验证标量、向量、UV和法线等不同输出类型。"""
    edit = unreal.MaterialEditingLibrary

    def add(class_name: str, x: int, y: int):
        cls = getattr(unreal, "MaterialExpression" + class_name, None)
        if cls is None:
            raise RuntimeError("UE5.8未公开材质表达式：" + class_name)
        result = edit.create_material_expression(material, cls, x, y)
        if result is None:
            raise RuntimeError("无法创建真实Material表达式：" + class_name)
        return result

    def scalar(v: float, x: int, y: int):
        c = add("Constant", x, y)
        c.set_editor_property("r", float(v))
        return c

    def vector(r: float, g: float, b: float, x: int, y: int):
        c = add("Constant3Vector", x, y)
        c.set_editor_property("constant", unreal.LinearColor(r, g, b, 1.0))
        return c

    def vector2(r: float, g: float, x: int, y: int):
        c = add("Constant2Vector", x, y)
        c.set_editor_property("r", float(r))
        c.set_editor_property("g", float(g))
        return c

    def wire(source, target, input_name: str, output: str = "") -> None:
        """所有函数输入以真实pin名称绑定，避免默认引脚排列变化后静默接错。"""
        ok = edit.connect_material_expressions(source, output, target, input_name)
        if not ok and input_name in {"Input", "VectorInput"}:
            # UE5.8的单一输入引脚可能在反射层使用空名称，需要显式后备。
            ok = edit.connect_material_expressions(source, output, target, "")
        if not ok:
            raise RuntimeError("函数参数接线失败：" + input_name + " 资产：" + material.get_path_name())

    old_calls = [node for node in edit.get_material_expressions(material)
                 if isinstance(node, unreal.MaterialExpressionMaterialFunctionCall)]
    if old_calls:
        if code != "VFXDissolve" or len(old_calls) != 1:
            raise RuntimeError("已有调用节点不符合指定的审核临时图恢复范围")
        call = old_calls[0]
        if call.get_editor_property("material_function") != function:
            raise RuntimeError("已有审核函数身份与目标不一致")
    else:
        call = add("MaterialFunctionCall", -430, -260)
        if not call.set_material_function(function):
            raise RuntimeError("MaterialFunctionCall未绑定目标原生MF：" + function.get_path_name())

    def bind(name: str, value):
        wire(value, call, name)

    def material_output(output_name: str, property_name: str):
        property_enum = getattr(unreal.MaterialProperty, "MP_" + property_name)
        if not edit.connect_material_property(call, output_name, property_enum):
            raise RuntimeError("未将真实函数输出连接到Material属性：" + output_name)

    if code == "HeightBlendMask":
        for idx, (name, value) in enumerate((("HeightA", 0.28), ("HeightB", 0.72), ("BlendAlpha", 0.46), ("Contrast", 1.4))):
            bind(name, scalar(value, -1250, -600 + idx * 190))
        material_output("HeightBlendMask", "BASE_COLOR")
    elif code == "MacroMicroBlend":
        for idx, (name, tint) in enumerate((("BaseColor", (0.32, 0.36, 0.32)),
                                            ("MacroColor", (0.26, 0.46, 0.24)),
                                            ("MicroColor", (0.40, 0.38, 0.28)))):
            bind(name, vector(*tint, -1260, -700 + idx * 195))
        bind("MacroWeight", scalar(0.50, -1260, 50))
        bind("MicroWeight", scalar(0.24, -1260, 240))
        material_output("BaseColor", "BASE_COLOR")
    elif code == "LandscapeBiomeMask":
        for idx, (name, value) in enumerate((("BiomeWeight", 0.80), ("SlopeMask", 0.77),
                                            ("AltitudeMask", 0.70), ("LocalWeight", 0.95))):
            bind(name, scalar(value, -1250, -640 + idx * 180))
        material_output("BiomeMask", "BASE_COLOR")
    elif code == "LandscapeDistanceDetail":
        bind("NearColor", vector(0.24, 0.44, 0.28, -1250, -600))
        bind("FarColor", vector(0.45, 0.55, 0.61, -1250, -420))
        bind("StartDistanceCm", scalar(150, -1250, -220))
        bind("EndDistanceCm", scalar(5800, -1250, -40))
        material_output("DetailColor", "BASE_COLOR")
    elif code == "FrostSurface":
        bind("BaseColor", vector(0.32, 0.34, 0.36, -1250, -720))
        bind("FrostColor", vector(0.88, 0.93, 0.97, -1250, -530))
        bind("BaseRoughness", scalar(0.72, -1250, -340))
        bind("FrostRoughness", scalar(0.94, -1250, -150))
        bind("BaseNormal", vector(0, 0, 1, -1250, 80))
        bind("FrostAmount", scalar(0.70, -1250, 310))
        bind("NormalFlattenStrength", scalar(0.65, -1250, 520))
        material_output("BaseColor", "BASE_COLOR")
        material_output("Roughness", "ROUGHNESS")
        material_output("Normal", "NORMAL")
    elif code == "VFXFlowUV":
        bind("UV", add("TextureCoordinate", -1300, -540))
        bind("VelocityUVPerSecond", vector2(0.07, 0.10, -1300, -330))
        bind("TimeSeconds", add("Time", -1300, -100))
        bind("Tiling", scalar(1.0, -1300, 120))
        append = add("AppendVector", 0, -300)
        wire(call, append, "A", "UV")
        wire(scalar(0.0, -200, 100), append, "B")
        if not edit.connect_material_property(append, "", unreal.MaterialProperty.MP_BASE_COLOR):
            raise RuntimeError("VFX滚动UV的视觉显示材质输出未连接")
    elif code == "VFXDissolve":
        # 已存在但未保存的本次审核临时图需要沿用原节点，避免重复添加调用器。
        prior = list(edit.get_material_expressions(material))
        tex_nodes = [e for e in prior if isinstance(e, unreal.MaterialExpressionTextureCoordinate)]
        mask_nodes = [e for e in prior if isinstance(e, unreal.MaterialExpressionComponentMask)]
        if len(tex_nodes) == 1 and len(mask_nodes) == 1:
            tex, mask = tex_nodes[0], mask_nodes[0]
        else:
            tex = add("TextureCoordinate", -1270, -610)
            mask = add("ComponentMask", -1050, -610)
            mask.set_editor_property("r", True)
        wire(tex, mask, "Input")
        bind("NoiseMask", mask)
        bind("Progress", scalar(0.49, -1260, -280))
        bind("EdgeWidth", scalar(0.10, -1260, -60))
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        material_output("VisibilityMask", "OPACITY_MASK")
        mult = add("Multiply", 10, -100)
        wire(call, mult, "A", "EdgeMask")
        wire(vector(1.0, 0.4, 0.06, -200, 180), mult, "B")
        if not edit.connect_material_property(mult, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
            raise RuntimeError("溶解边缘发光未能连接到正式Shader")
        if not edit.connect_material_property(vector(0.18, 0.27, 0.30, -320, -550), "", unreal.MaterialProperty.MP_BASE_COLOR):
            raise RuntimeError("溶解基础色未连接")
    elif code == "VFXRadialEnergy":
        bind("UV", add("TextureCoordinate", -1240, -550))
        bind("CenterUV", vector2(0.50, 0.50, -1240, -340))
        bind("RadiusUV", scalar(0.37, -1240, -130))
        bind("EdgeSoftnessUV", scalar(0.18, -1240, 100))
        material_output("RadialMask", "BASE_COLOR")
        mult = add("Multiply", 20, 180)
        wire(call, mult, "A", "RadialMask")
        wire(vector(0.15, 0.62, 1.0, -150, 420), mult, "B")
        if not edit.connect_material_property(mult, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
            raise RuntimeError("径向能量预览的发光通道未连接")
    else:
        raise ValueError("未知测试配方：" + code)


def main() -> None:
    """不覆盖已有资源；着色器编译报告只说明当前预览母材质。"""
    parser = argparse.ArgumentParser(description="8项已制作材质函数的UE5.8消费Shader审核")
    parser.add_argument("--mode", choices=("inspect", "apply", "verify"),
                        default=os.getenv("MATERIAL_REVIEW_MODE", "inspect"))
    opts, _ = parser.parse_known_args()
    for code, group, function_path in FUNCTIONS:
        print("REVIEW_TARGET", code, group, function_path, asset_path(code))
    if opts.mode == "inspect":
        print("INSPECT_ONLY：不会创建审核材质资产")
        return

    try:
        import unreal  # type: ignore[import-not-found]
    except ImportError as err:
        raise RuntimeError("本作者必须在实际UE5.8 Editor Python环境运行") from err

    require_project(unreal)
    library = unreal.EditorAssetLibrary
    edit = unreal.MaterialEditingLibrary
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([QA_ROOT, "/GamePlatformSurface", "/GamePlatformVFX"], force_rescan=True)

    # 先审查全部八项函数与潜在重名审核材质，确保不会中途覆盖其它版本。
    for code, _, path in FUNCTIONS:
        asset = library.load_asset(path)
        if not isinstance(asset, unreal.MaterialFunction):
            raise TypeError("未发现真实平台MF（材质函数）：" + path)
        target = asset_path(code)
        existed = library.load_asset(target) if library.does_asset_exist(target) else None
        if existed is not None and not isinstance(existed, unreal.Material):
            raise TypeError("审核路径同名UE资产类别错误：" + target)
        disk = QA_DISK_ROOT / (target.rsplit("/", 1)[-1] + ".uasset")
        if existed is None and any(disk.with_suffix(s).exists() for s in (".uasset", ".uexp", ".ubulk", ".umap")):
            raise RuntimeError("现有未注册物理资源/侧车文件不可覆盖：" + str(disk))
        if opts.mode == "verify" and existed is None:
            raise FileNotFoundError("真实审核材质不存在：" + target)

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialFactoryNew()
    if opts.mode == "apply" and not library.does_directory_exist(QA_ROOT):
        library.make_directory(QA_ROOT)

    for code, _, path in FUNCTIONS:
        target = asset_path(code)
        existing = library.does_asset_exist(target)
        if existing:
            mat = library.load_asset(target)
            physical = QA_DISK_ROOT / (target.rsplit("/", 1)[-1] + ".uasset")
            if (opts.mode == "apply" and not physical.exists() and code == "VFXDissolve"
                    and len(edit.get_material_expressions(mat)) == 3):
                # 只延续这次未落盘的指定3节点QA测试图；不会修改已经保存的材质。
                build(unreal, mat, code, library.load_asset(path))
                print("REVIEW_MATERIAL_UNSAVED_GRAPH_COMPLETED", code)
            nodes = inspect_review_graph(unreal, mat, path)
            if opts.mode == "apply" and not physical.exists():
                # 上次作者在内存中创建了本工具独占的完整审核图，但验收中断且无物理包；
                # 只允许对符合函数调用身份且节点已完整的该专属内存对象编译并存盘，绝不清图。
                diagnostic = list(edit.recompile_material(mat) or [])
                for message in diagnostic:
                    print("REVIEW_SHADER_RECOVERY_DIAGNOSTIC", code, str(message))
                    if "error" in str(message).lower():
                        raise RuntimeError("恢复审核材质时真实Shader编译报错：" + code)
                if not library.save_loaded_asset(mat, only_if_is_dirty=False):
                    raise RuntimeError("恢复审核材质保存失败：" + target)
                print("REVIEW_MATERIAL_RECOVERED_SAVED", code, target, "nodes", nodes)
            else:
                print("REVIEW_MATERIAL_VERIFIED_EXISTING", code, nodes, target)
            continue
        if opts.mode != "apply":
            raise RuntimeError("只读核验禁止创建未知资产：" + target)
        mat = tools.create_asset(target.rsplit("/", 1)[-1], QA_ROOT,
                                 unreal.Material, factory)
        if not isinstance(mat, unreal.Material):
            raise RuntimeError("真实UMaterial审核材质创建失败：" + target)
        fn = library.load_asset(path)
        build(unreal, mat, code, fn)
        count = inspect_review_graph(unreal, mat, path)
        errors = list(edit.recompile_material(mat) or [])
        for message in errors:
            print("REVIEW_SHADER_DIAGNOSTIC", code, str(message))
            if "error" in str(message).lower():
                raise RuntimeError("UE真实着色器编译错误：" + code)
        if not library.save_loaded_asset(mat, only_if_is_dirty=False):
            raise RuntimeError("UE审核材质原生保存失败：" + target)
        reloaded = library.load_asset(target)
        if not isinstance(reloaded, unreal.Material):
            raise RuntimeError("审核母材质保存后回读失败：" + target)
        print("REVIEW_MATERIAL_COMPILED_SAVED", code, target, "nodes", count)

    print("MATERIAL_LIBRARY_REVIEW_MATERIALS_DONE：八项已处理；人工预览、Cook/Stage仍须独立验收")


if __name__ == "__main__":
    main()
