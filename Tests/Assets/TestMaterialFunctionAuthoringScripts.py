#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""平台Surface与VFX材质函数作者脚本的无引擎结构测试。

该测试只运行Python内存假编辑器，检查目标身份、节点数量、函数输出和接线完整性；
不创建.uasset、不加载引擎，不报告Shader/Cook/视觉通过。
当真实UE5.8具备时，必须额外执行实际图制作、编译、保存回读。
"""
from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SURFACE_AUTHOR = ROOT / "Tools" / "Unreal" / "Surface" / "AuthorMaterialCoreFunctionsUE58.py"
VFX_AUTHOR = ROOT / "Tools" / "Unreal" / "VFX" / "AuthorVFXMaterialFunctionsUE58.py"


def load_script(path: Path):
    """按工作区文件路径加载作者模块，不创建字节码文件及测试资源。"""
    spec = importlib.util.spec_from_file_location("material_author_" + path.stem, path)
    if spec is None or spec.loader is None:
        raise RuntimeError("无法读入材质函数作者脚本：" + str(path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class FakeExpression:
    """仅记录表达式类型、输入连接与属性，不模拟真正Shader节点。"""
    def __init__(self, expression_type: str, index: int):
        self.expression_type = expression_type
        self.index = index
        self.properties: dict[str, object] = {}
        self.inputs: list[tuple[object, str, str]] = []

    def set_editor_property(self, name: str, value: object) -> None:
        self.properties[name] = value

    def get_name(self) -> str:
        return f"{self.expression_type}_{self.index}"


class FakeFunction:
    """只属于测试进程的内存表达式集合，不具有UE资产身份。"""
    def __init__(self):
        self.expressions: list[FakeExpression] = []


class FakeMaterialEditingLibrary:
    """最小UE编辑器函数编辑接口的拓扑测试替身。"""
    @staticmethod
    def create_material_expression_in_function(function, expression_type, x: int, y: int):
        result = FakeExpression(expression_type, len(function.expressions))
        function.expressions.append(result)
        return result

    @staticmethod
    def connect_material_expressions(source, output_name, target, input_name):
        target.inputs.append((source, output_name, input_name))
        return True


class FakeFunctionInputType:
    FUNCTION_INPUT_SCALAR = "Scalar（标量）"
    FUNCTION_INPUT_VECTOR2 = "Vector2（二维向量）"
    FUNCTION_INPUT_VECTOR3 = "Vector3（三维向量）"


class FakeUnreal:
    MaterialEditingLibrary = FakeMaterialEditingLibrary
    FunctionInputType = FakeFunctionInputType

    @staticmethod
    def LinearColor(r: float, g: float, b: float, a: float):
        return r, g, b, a

    def __getattr__(self, name: str):
        if name.startswith("MaterialExpression"):
            return name
        raise AttributeError(name)


class TestMaterialFunctionAuthoringScripts(unittest.TestCase):
    """检查计划函数均包含真实计算构建逻辑而非恒定返回/空白资产。"""

    def test_surface_graphs_have_expected_outputs(self):
        author = load_script(SURFACE_AUTHOR)
        build = author.create_author(FakeUnreal())
        self.assertEqual(len(author.FUNCTIONS), 5)
        expected = {
            "height": {"HeightBlendMask"},
            "macro": {"BaseColor"},
            "biome": {"BiomeMask"},
            "distance": {"DetailColor", "FarWeight"},
            "frost": {"BaseColor", "Roughness", "Normal"},
        }
        seen_paths = set()
        for area, name, recipe, minimum, chinese in author.FUNCTIONS:
            package = author.target_path(area, name)
            self.assertNotIn(package, seen_paths)
            seen_paths.add(package)
            self.assertTrue(package.startswith("/GamePlatformSurface/MaterialFunctions/"))
            function = FakeFunction()
            with self.subTest(resource=chinese, recipe=recipe):
                build(function, recipe)
                self.assertGreaterEqual(len(function.expressions), minimum)
                outputs = {x.properties["output_name"] for x in function.expressions
                           if x.expression_type == "MaterialExpressionFunctionOutput"}
                self.assertEqual(outputs, expected[recipe])
                for node in function.expressions:
                    if node.expression_type == "MaterialExpressionFunctionOutput":
                        self.assertGreater(len(node.inputs), 0)

    def test_vfx_graphs_have_expected_outputs(self):
        author = load_script(VFX_AUTHOR)
        self.assertEqual(len(author.FUNCTIONS), 3)
        expected = {
            "uvflow": {"UV"},
            "dissolve": {"VisibilityMask", "EdgeMask"},
            "radial": {"RadialMask"},
        }
        for name, recipe, minimum, chinese in author.FUNCTIONS:
            package = author.FOLDER + "/" + name
            self.assertTrue(package.startswith("/GamePlatformVFX/Materials/Functions/"))
            function = FakeFunction()
            with self.subTest(resource=chinese, recipe=recipe):
                author.build_graph(FakeUnreal(), function, recipe)
                self.assertGreaterEqual(len(function.expressions), minimum)
                outputs = {x.properties["output_name"] for x in function.expressions
                           if x.expression_type == "MaterialExpressionFunctionOutput"}
                self.assertEqual(outputs, expected[recipe])
                self.assertTrue(all(node.inputs for node in function.expressions
                                    if node.expression_type == "MaterialExpressionFunctionOutput"))

    def test_no_collision_with_frozen_surface_assets(self):
        """新作者资产与七MF、两M、一个MPC合同互不重名。"""
        from json import loads
        frozen_path = ROOT / "Docs" / "Implementation" / "Materials" / "MaterialAssetInventory.json"
        frozen = loads(frozen_path.read_text(encoding="utf-8"))
        known = {item["PackagePath"] for item in frozen["contractEntries"]}
        surface = load_script(SURFACE_AUTHOR)
        vfx = load_script(VFX_AUTHOR)
        proposed = {surface.target_path(area, name) for area, name, *_ in surface.FUNCTIONS}
        proposed |= {vfx.FOLDER + "/" + name for name, *_ in vfx.FUNCTIONS}
        self.assertEqual(len(proposed), 8)
        self.assertFalse(proposed.intersection(known), "禁止覆盖既有发布材质资产")

    def test_reject_unknown_recipe(self):
        """无支持的配方必须明确失败，不产生固定成功的空白实现。"""
        with self.assertRaises(ValueError):
            load_script(SURFACE_AUTHOR).create_author(FakeUnreal())(FakeFunction(), "invalid")
        with self.assertRaises(ValueError):
            load_script(VFX_AUTHOR).build_graph(FakeUnreal(), FakeFunction(), "invalid")


if __name__ == "__main__":
    unittest.main(verbosity=2)
