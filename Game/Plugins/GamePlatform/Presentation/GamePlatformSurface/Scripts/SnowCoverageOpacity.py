# -*- coding: utf-8 -*-
"""平台层只用于UE编辑器的雪覆盖不透明度制作配方。

将“雪量”与覆盖区域内的“透底程度”分开：重雪可实覆，小雪仍保留薄雪。
调用方提供本地参数名及节点，不识别项目、地图或天气权威；所有真实图由Monolith执行。
共享服务器地图的中性材质可展开同一配方，避免对ClientOnly内容包产生硬引用。
展开图与平台MaterialFunction使用同一制作真源，运行时仍仅有Weather/Surface唯一状态链。
"""
import unreal


def build_opacity(owner, amount, pattern, full_cover_amount, x=0, y=0, function=False):
    """输入均为原生MaterialExpression；返回饱和覆盖强度与形状相乘的节点。"""
    edit = unreal.MaterialEditingLibrary

    def create(cls, px, py):
        return (edit.create_material_expression_in_function(owner, cls, px, py)
                if function else edit.create_material_expression(owner, cls, px, py))

    def wire(a, b, pin):
        ok = edit.connect_material_expressions(a, "", b, pin)
        if not ok and pin == "Input":
            ok = edit.connect_material_expressions(a, "", b, "")
        if not ok:
            raise RuntimeError("雪覆盖图接线失败: " + pin)

    ratio = create(unreal.MaterialExpressionDivide, x, y)
    strength = create(unreal.MaterialExpressionSaturate, x + 190, y)
    result = create(unreal.MaterialExpressionMultiply, x + 380, y)
    wire(amount, ratio, "A")
    wire(full_cover_amount, ratio, "B")
    wire(ratio, strength, "Input")
    wire(strength, result, "A")
    wire(pattern, result, "B")
    return result


def author_function():
    """真实创建通用MaterialFunction；已有资产只核验输入输出，不覆盖用户改动。"""
    path = "/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_SnowCoverageOpacity"
    existing = unreal.load_asset(path)
    if existing:
        if not isinstance(existing, unreal.MaterialFunction):
            raise TypeError("雪覆盖函数身份冲突")
        # 上次编辑若在函数输出引脚校验前中断，只修复同一原生图，不创建第二份资产。
        expressions = unreal.MaterialEditingLibrary.get_material_function_expressions(existing)
        output = next(n for n in expressions if isinstance(n, unreal.MaterialExpressionFunctionOutput))
        result = next(n for n in expressions if isinstance(n, unreal.MaterialExpressionMultiply))
        if not unreal.MaterialEditingLibrary.connect_material_expressions(result, "", output, ""):
            raise RuntimeError("雪覆盖函数输出未连接")
        unreal.MaterialEditingLibrary.update_material_function(existing)
        unreal.EditorAssetLibrary.save_loaded_asset(existing)
        return existing
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        path.rsplit("/", 1)[1], path.rsplit("/", 1)[0],
        unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
    edit = unreal.MaterialEditingLibrary
    inputs = []
    for index, name in enumerate(("SnowAmount", "CoveragePattern", "FullCoverAmount")):
        n = edit.create_material_expression_in_function(asset, unreal.MaterialExpressionFunctionInput, -600, index * 180)
        n.set_editor_property("input_name", name)
        n.set_editor_property("input_type", unreal.FunctionInputType.FUNCTION_INPUT_SCALAR)
        n.set_editor_property("sort_priority", index)
        inputs.append(n)
    result = build_opacity(asset, *inputs, x=-250, y=0, function=True)
    output = edit.create_material_expression_in_function(asset, unreal.MaterialExpressionFunctionOutput, 400, 0)
    output.set_editor_property("output_name", "SnowOpacity")
    if not edit.connect_material_expressions(result, "", output, ""):
        raise RuntimeError("雪覆盖函数输出未连接")
    edit.update_material_function(asset)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError("雪覆盖MaterialFunction保存失败")
    return asset
