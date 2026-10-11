#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""写实积雪雪量遮罩曲线的 UE5.8 真实材质迁移器。

所属：Tools/Unreal/Weather（工程内容制作工具），平台父材质仍归GamePlatformSurface。
输入：已存在的M_GP_SnowCover_Detailed，或从它复制出的Development/Snow审核材质。
背景：旧曲线threshold=1-SnowAmount，原噪声在小雪量0.23时只有约0.18%的像素越阈。
目标：coverage=SnowAmount*saturate((Noise-(0.5-SnowAmount*0.4))/EdgeSoftness)*Slope*Height。
这样小雪形成薄而不均匀的雪层，大雪覆盖连续，同时保持雪量0时覆盖严格为0。
限制：不会修改MPC契约、服务器天气、游戏碰撞/摩擦或原始雪纹理。
模式：默认inspect不写；SNOW_COVER_UPGRADE_MODE=apply且在正确UE Editor内才编辑。
安全：仅允许平台原材质和单独Developer审核副本；标记已迁移则不重复插入节点。
修改后必须运行Shader编译并真实保存；失败不假装成功。
"""
from __future__ import annotations

import os

MODE = os.getenv("SNOW_COVER_UPGRADE_MODE", "inspect").strip().lower()
TARGET = os.getenv(
    "SNOW_COVER_UPGRADE_TARGET",
    "/GamePlatformSurface/Materials/M_GP_SnowCover_Detailed",
).strip()
ALLOWED_TARGETS = {
    "/GamePlatformSurface/Materials/M_GP_SnowCover_Detailed",
    "/Game/Development/Snow/Materials/M_DBA_SnowMaskExperiment",
}
PARAM_NAME = "SnowCoverageShiftScale"


def execute() -> None:
    if MODE not in ("inspect", "apply"):
        raise ValueError("SNOW_COVER_UPGRADE_MODE只允许inspect或apply")
    if TARGET not in ALLOWED_TARGETS:
        raise ValueError("拒绝在非审核或非既定平台父材质上更改遮罩：" + TARGET)
    print("SNOW_COVER_UPGRADE_TARGET", TARGET)
    if MODE == "inspect":
        print("SNOW_COVER_UPGRADE_INSPECT_ONLY")
        return

    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("只能在锁定UE5.8正式编辑器中编辑真实Material") from exc

    actual_project = unreal.Paths.get_project_file_path().replace("\\", "/")
    if not actual_project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("当前Editor工程不是神兽联盟正式工程：" + actual_project)

    asset_tools = unreal.EditorAssetLibrary
    editor = unreal.MaterialEditingLibrary
    material = asset_tools.load_asset(TARGET)
    if not isinstance(material, unreal.Material):
        raise TypeError("现有母材质不是UE Material：" + TARGET)

    def names() -> dict[str, object]:
        return {value.get_name(): value for value in editor.get_material_expressions(material)}

    available = names()
    expected = {
        "MaterialExpressionConstant_0": unreal.MaterialExpressionConstant,
        "MaterialExpressionSubtract_1": unreal.MaterialExpressionSubtract,
        "MaterialExpressionCollectionParameter_1": unreal.MaterialExpressionCollectionParameter,
        "MaterialExpressionMultiply_3": unreal.MaterialExpressionMultiply,
        "MaterialExpressionSaturate_3": unreal.MaterialExpressionSaturate,
    }
    for identity, cls in expected.items():
        if not isinstance(available.get(identity), cls):
            raise RuntimeError("原Snow父材质拓扑变化，拒绝按旧版本盲目重接：" + identity)
    global_snow = available["MaterialExpressionCollectionParameter_1"]
    if str(global_snow.get_editor_property("parameter_name")) != "GP_Surface_GlobalSnowAmount":
        raise RuntimeError("global snow参数引用身份发生变化，拒绝迁移")

    already = [
        node for node in available.values()
        if isinstance(node, unreal.MaterialExpressionScalarParameter)
        and str(node.get_editor_property("parameter_name")) == PARAM_NAME
    ]
    if already:
        if len(already) != 1:
            raise RuntimeError("存在重复的雪遮罩迁移标记，需人工审查")
        if editor.get_num_material_expressions(material) < 56:
            raise RuntimeError("已迁移材质的表达式数量不符")
        const = available["MaterialExpressionConstant_0"]
        if abs(float(const.get_editor_property("r")) - 0.5) > 1e-6:
            raise RuntimeError("已迁移雪层阈值基准常量异常")
        final_node = available.get("MaterialExpressionMultiply_7")
        if not isinstance(final_node, unreal.MaterialExpressionMultiply):
            raise RuntimeError("已迁移SnowAmount强度乘法节点丢失")
        sat_node = available["MaterialExpressionSaturate_3"]
        input_nodes = editor.get_inputs_for_material_expression(material, sat_node)
        connected = [node.get_name() for node in input_nodes]
        if final_node.get_name() not in connected:
            if connected != ["MaterialExpressionMultiply_3"]:
                raise RuntimeError("现有遮罩输入来自非预期节点，拒绝覆盖：" + str(connected))
            # 上一次UE编辑器未识别Input文字引脚时，只允许完成尚未连接的最后一步。
            if not editor.connect_material_expressions(final_node, "", sat_node, ""):
                raise RuntimeError("断点续作仍无法连接Saturate默认引脚")
            issues = list(editor.recompile_material(material))
            if any("error" in str(item).lower() for item in issues):
                raise RuntimeError("断点续作Snow遮罩材质编译失败")
            if not asset_tools.save_loaded_asset(material, only_if_is_dirty=False):
                raise RuntimeError("断点续作完成但UE材质保存失败")
            print("SNOW_COVER_UPGRADE_RESUMED", TARGET)
        else:
            print("SNOW_COVER_UPGRADE_ALREADY_PRESENT", TARGET)
        return

    const_base = available["MaterialExpressionConstant_0"]
    if abs(float(const_base.get_editor_property("r")) - 1.0) > 1e-6:
        raise RuntimeError("旧阈值常量非1，存在未受控的美术修改；拒绝覆盖")

    # 增量插入三个表达式，复用原Slope、Height和噪声/材质输入。
    shift = editor.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, -1880, -3020
    )
    shift.set_editor_property("parameter_name", unreal.Name(PARAM_NAME))
    shift.set_editor_property("default_value", 0.4)

    scaled_amount = editor.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -1500, -2800
    )
    amount_mask = editor.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -10, -1920
    )

    def link(source, dest, input_name: str):
        connected = editor.connect_material_expressions(source, "", dest, input_name)
        # UE5.8中Saturate等单输入节点在反射接口上可能使用空FName。
        if not connected and input_name == "Input":
            connected = editor.connect_material_expressions(source, "", dest, "")
        if not connected:
            raise RuntimeError(
                "雪遮罩节点引脚重连失败：" +
                source.get_name() + " -> " + dest.get_name() + ":" + input_name
            )

    link(global_snow, scaled_amount, "A")
    link(shift, scaled_amount, "B")
    # 原Subtract_1的B为裸SnowAmount，改为SnowAmount*0.4；底线变为0.5。
    link(scaled_amount, available["MaterialExpressionSubtract_1"], "B")
    const_base.set_editor_property("r", 0.5)
    # 原Saturate_3曾仅乘Slope*Height，现显式乘SnowAmount保证无雪时Alpha为0。
    link(available["MaterialExpressionMultiply_3"], amount_mask, "A")
    link(global_snow, amount_mask, "B")
    link(amount_mask, available["MaterialExpressionSaturate_3"], "Input")

    problems = list(editor.recompile_material(material))
    for diag in problems:
        print("SNOW_COVER_SHADER_DIAGNOSTIC", str(diag))
    if any("error" in str(issue).lower() for issue in problems):
        raise RuntimeError("新的雪材质着色器有编译错误，拒绝保存")
    count = editor.get_num_material_expressions(material)
    if count < 56:
        raise RuntimeError("新版本材质表达式数量不足：" + str(count))
    if not asset_tools.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError("UE无法保存完成遮罩迁移的真实母材质")

    check = asset_tools.load_asset(TARGET)
    if not isinstance(check, unreal.Material):
        raise RuntimeError("UE保存后无法回读材质类别")
    print("SNOW_COVER_UPGRADE_SAVED", TARGET, "nodes", count)
    print("SNOW_COVER_FORMULA", "SnowAmount*saturate((Noise-(0.5-SnowAmount*0.4))/Edge)*Slope*Height")


if __name__ == "__main__":
    execute()
