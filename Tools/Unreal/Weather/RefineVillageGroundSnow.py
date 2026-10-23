# -*- coding: utf-8 -*-
"""通过Monolith在正确UE5.8工程增量打磨新手村雪层，绝不保存或替换地图。

平台SnowCoverageOpacity是制作配方唯一真源；共享中性桥展开数学节点以保持服务器资源隔离。
本脚本只更新既有母材质，保留草地Texture Bombing、底图、所有地形层及出生点。
调用前必须备份目标uasset；修改后原生编译、保存，并运行原生连线数值测试。
"""
import importlib.util
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.get_project_file_path()).resolve().parents[1]
if ROOT.name != "DivineBeastsWorkspace":
    raise RuntimeError("拒绝修改非正式工作空间")
recipe_path = ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Scripts/SnowCoverageOpacity.py"
spec = importlib.util.spec_from_file_location("gp_snow_opacity", recipe_path)
recipe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recipe)
function = recipe.author_function()
PATH = "/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge"
mat = unreal.load_asset(PATH)
if not isinstance(mat, unreal.Material):
    raise RuntimeError("新手村既有地形母材质缺失")
edit = unreal.MaterialEditingLibrary
nodes = {n.get_name(): n for n in edit.get_material_expressions(mat)}
params = {str(n.get_editor_property("parameter_name")): n for n in nodes.values()
          if isinstance(n, unreal.MaterialExpressionScalarParameter)}
if "VillageSnowFullCoverAmount" in params:
    print("VILLAGE_GROUND_SNOW_REFINEMENT_ALREADY_AUTHORED")
else:
    required = ["MaterialExpressionScalarParameter_3", "MaterialExpressionSaturate_1",
                "MaterialExpressionMultiply_4", "MaterialExpressionSaturate_3",
                "MaterialExpressionLinearInterpolate_1", "MaterialExpressionLinearInterpolate_4",
                "MaterialExpressionTextureSampleParameter2D_2", "MaterialExpressionTextureSampleParameter2D_6"]
    if any(k not in nodes for k in required):
        raise RuntimeError("地形图拓扑已改变，拒绝按旧连线盲改")

    def create(cls, x, y):
        return edit.create_material_expression(mat, cls, x, y)

    def scalar(name, value, x, y):
        n = create(unreal.MaterialExpressionScalarParameter, x, y)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", value)
        return n

    def wire(a, b, pin, output=""):
        if not edit.connect_material_expressions(a, output, b, pin):
            raise RuntimeError("新手村雪层接线失败: " + pin)

    full = scalar("VillageSnowFullCoverAmount", .55, -2550, -3440)
    opacity = recipe.build_opacity(mat, nodes["MaterialExpressionScalarParameter_3"],
                                   nodes["MaterialExpressionSaturate_1"], full, -2150, -3420)
    # 只替换原来直接乘雪量的节点，保留坡度/海拔和原有零雪量语义。
    wire(opacity, nodes["MaterialExpressionMultiply_4"], "A")
    mask = nodes["MaterialExpressionSaturate_3"]
    # 降低雪法线的高频强度，同时将最终混合法线重新归一，消除细碎高光。
    flat = create(unreal.MaterialExpressionConstant3Vector, -600, 3140)
    flat.set_editor_property("constant", unreal.LinearColor(0, 0, 1, 1))
    strength = scalar("VillageSnowNormalStrength", .20, -600, 3340)
    soft_normal = create(unreal.MaterialExpressionLinearInterpolate, -250, 3100)
    wire(flat, soft_normal, "A")
    wire(nodes["MaterialExpressionTextureSampleParameter2D_2"], soft_normal, "B", "RGB")
    wire(strength, soft_normal, "Alpha")
    wire(soft_normal, nodes["MaterialExpressionLinearInterpolate_4"], "B")
    normalized = create(unreal.MaterialExpressionNormalize, 680, 2720)
    wire(nodes["MaterialExpressionLinearInterpolate_4"], normalized, "VectorInput")
    if not edit.connect_material_property(normalized, "", unreal.MaterialProperty.MP_NORMAL):
        raise RuntimeError("雪层法线未连接最终输出")
    # 雪覆盖后解除草纹理AO，防止雪面继续显示草丛暗斑。
    snow_ao = create(unreal.MaterialExpressionConstant, 100, 3600)
    snow_ao.set_editor_property("r", 1.0)
    ao = create(unreal.MaterialExpressionLinearInterpolate, 450, 3500)
    wire(nodes["MaterialExpressionTextureSampleParameter2D_6"], ao, "A", "R")
    wire(snow_ao, ao, "B")
    wire(mask, ao, "Alpha")
    edit.connect_material_property(ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    # 以中性雪色为基准保留低对比微观变化，纹理sRGB/线性解码保持原资产合同。
    original_snow = nodes["MaterialExpressionMultiply_6"]
    neutral = create(unreal.MaterialExpressionConstant3Vector, -700, 770)
    neutral.set_editor_property("constant", unreal.LinearColor(.78, .80, .83, 1))
    detail = scalar("VillageSnowAlbedoDetailContrast", .28, -700, 1000)
    clean_snow = create(unreal.MaterialExpressionLinearInterpolate, -200, 750)
    wire(neutral, clean_snow, "A")
    wire(original_snow, clean_snow, "B")
    wire(detail, clean_snow, "Alpha")
    wire(clean_snow, nodes["MaterialExpressionLinearInterpolate_1"], "B")
    diagnostics = [str(d) for d in edit.recompile_material(mat)]
    if any("error" in d.lower() for d in diagnostics):
        raise RuntimeError("雪材质Shader编译失败: " + str(diagnostics))
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError("新手村雪层母材质保存失败")
    print("VILLAGE_GROUND_SNOW_REFINED", mat.get_path_name(), "nodes", edit.get_num_material_expressions(mat))
print("PLATFORM_SNOW_OPACITY_FUNCTION", function.get_path_name())
