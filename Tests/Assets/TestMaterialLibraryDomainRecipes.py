#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""材质函数库60原生配方、12材质层和7项UI配方的统一静态门禁。

本测试不进入UE编辑器、不生成二进制，不执行Shader编译/Cook；仅检查源码
算法、三层所有权、命名、201项单一真源与已有第一/二批正式函数不冲突。
统一测试必须在代码、蓝图和资源制作完成后再由发布门禁执行。
"""
from __future__ import annotations

import importlib.util
import json
from collections import Counter
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
AUTHOR=ROOT/"Tools/Unreal/Surface/AuthorMaterialLibraryDomainUE58.py"
LAYERS=ROOT/"Tools/Unreal/Surface/MaterialLibraryLayerProfiles_V1.json"
UI=ROOT/"Tools/Unreal/UI/MaterialLibraryUIMonolithRecipes_V1.json"
REGISTRY=ROOT/"Docs/Implementation/Materials/MaterialCapabilityRegistry_V1.json"
FIRST=ROOT/"Docs/Implementation/Materials/MaterialReviewShaderEvidence_UE58_20261013.json"
SECOND=ROOT/"Docs/Implementation/Materials/MaterialBatch2ShaderAndUIEvidence_UE58_20261013.json"
DEMO=ROOT/"Docs/Implementation/Materials/MaterialLibraryBlueprintAndUIManifest_V1.json"


def read_json(path):
    """将每份来源与当前仓库文件对应，不接受来源不明的测试替身。"""
    return json.loads(path.read_text(encoding="utf-8"))


def load_author():
    """使用标准importlib只读加载真实项目作者模块，避免触发UE写入。"""
    spec=importlib.util.spec_from_file_location("domain_material_library_author",AUTHOR)
    if spec is None or spec.loader is None:
        raise RuntimeError("缺少真实材质领域函数源码")
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class MaterialLibraryDomainRecipes(unittest.TestCase):
    """完整覆盖真实能力清单，并保持资产生成与验收边界。"""

    @classmethod
    def setUpClass(cls):
        cls.author=load_author()
        cls.items=cls.author.load_catalog()
        cls.registry=read_json(REGISTRY)
        cls.rows={i["CapabilityId"]:i for i in cls.registry["items"]}

    def test_60_meaningful_recipes_have_unique_stable_paths(self):
        """60个必须真正有合法计算树，不允许同路径覆盖或生成直通占位。"""
        self.assertEqual(len(self.items),60)
        self.assertEqual(len({s["id"] for s in self.items}),60)
        paths=[self.author.asset_path(s) for s in self.items]
        self.assertEqual(len(paths),len(set(paths)))
        for entry in self.items:
            with self.subTest(capability=entry["id"]):
                self.author.assert_valid_recipe(entry)
                self.assertIn(entry["id"],self.rows)
                self.assertTrue(entry["outputs"])
                self.assertTrue(entry["zh"])
                self.assertFalse(any(x["expression"] in (None,[],{}) for x in entry["outputs"]))

    def test_three_layer_ownership_and_prefixes(self):
        """平台无硬引用项目技能/纹理；项目视觉函数只写第三层内容包。"""
        owners=Counter(s["owner"] for s in self.items)
        self.assertEqual(dict(owners),{"Surface":46,"VFX":4,"Project":10})
        self.assertEqual(sum(owners.values()),60)
        for item in self.items:
            path=self.author.asset_path(item)
            if item["owner"]=="Surface":
                self.assertTrue(path.startswith("/GamePlatformSurface/MaterialFunctions/"))
            elif item["owner"]=="VFX":
                self.assertTrue(path.startswith("/GamePlatformVFX/Materials/Functions/"))
            elif item["owner"]=="Project":
                self.assertTrue(path.startswith("/DBAPresentationPack_Core/MaterialFunctions/Character/"))
            else:
                self.fail("未知跨层资源Owner")

    def test_no_collision_with_verified_first_and_second_buckets(self):
        """新函数不得覆盖之前实际编译过的8+18项。"""
        old=read_json(FIRST)
        second=read_json(SECOND)
        previously={x["materialFunctionPath"] for x in old["records"]}
        previously.update(x["assetPath"] for x in second["newSecondBatch"]["records"])
        now={self.author.asset_path(s) for s in self.items}
        self.assertEqual(len(previously),26)
        self.assertFalse(previously & now,"新配方会与历史正式资产冲突")

    def test_material_layer_and_ui_delegation_contracts(self):
        """12ML与7项待做UI必须归既有插件，不得为凑数创建第48个新插件。"""
        layers=read_json(LAYERS)
        ui=read_json(UI)
        self.assertEqual(len(layers["items"]),12)
        self.assertEqual(len(ui["items"]),7)
        observed=set()
        for layer in layers["items"]:
            row=self.rows[layer["capabilityId"]]
            self.assertEqual(row["ImplementationMode"],"ML")
            self.assertEqual(row["OwnerPlugin"],"GamePlatformSurface")
            self.assertIn(layer["capabilityId"],self.rows)
            self.assertTrue(layer["materialLayerAssetPath"].startswith("/GamePlatformSurface/MaterialLayers/ML_GP_"))
            observed.add(layer["capabilityId"])
        for item in ui["items"]:
            row=self.rows[item["capabilityId"]]
            self.assertEqual(row["ImplementationMode"],"MF")
            self.assertEqual(row["OwnerPlugin"],"GamePlatformUI")
            self.assertEqual(row["NameEN"],item["name"])
            self.assertEqual(len(item["graphSpec"]["outputs"]),1 if item["name"]!="UITransitionDissolve" else 2)
            self.assertGreaterEqual(len(item["graphSpec"]["connections"]),3)
            observed.add(item["capabilityId"])
        self.assertEqual(len(observed),19)

    def test_demo_assets_are_isolated_and_appropriately_owned(self):
        """开发Review场景不得替代正式Village、MainArena或客户端UI生产资产。"""
        demo=read_json(DEMO)
        self.assertEqual(len(demo["maps"]),4)
        for item in demo["maps"]:
            self.assertTrue(item["map"].startswith("/Game/Development/MaterialLibrary/"))
            self.assertIn(item["owner"],{
                "GamePlatformSurface","GamePlatformVFX","GamePlatformUI","DBAPresentationPack_Core"})
        self.assertTrue(demo["uiWidget"]["assetPath"].startswith("/DBAUIPack_Core/"))
        self.assertTrue(demo["standBlueprint"]["assetPath"].startswith("/Game/Development/MaterialLibrary/"))


if __name__=="__main__":
    unittest.main(verbosity=2)
