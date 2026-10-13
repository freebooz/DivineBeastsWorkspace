#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""创建介绍源门禁回归：离线读取正式JSON/配置，对突变输入要求真实拒绝。

调用方为main的只读验证流程；不导入UE、连接Monolith、创建资产或判断Cook结果。
生成器用例仅运行其真实ensure_hero_definition函数与明确的字段记录替身，验证重建写入合同，
不是UE反射/保存验收。运行入口：python -B Tests/Assets/TestCharacterCreationIntroductionSources.py。
"""
import ast
import copy
import sys
import types
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = ROOT / "Tools/Unreal/Characters"
sys.path.insert(0, str(SOURCE_DIR))
from CharacterCreationIntroductionSource import load_creation_sources, validate_creation_sources


class CharacterCreationIntroductionSourcesTests(unittest.TestCase):
    """正向使用实际12/60项源数据，负向分别隔离身份、文案、引用和数值污染。"""

    def setUp(self):
        self.profiles, self.heroes, self.settings, self.draft = load_creation_sources(ROOT)

    def validate(self):
        validate_creation_sources(self.profiles, self.heroes, self.settings, self.draft)

    def test_actual_sources_have_twelve_five_entry_profiles(self):
        self.validate()
        self.assertEqual(len(self.profiles["Profiles"]), 12)
        self.assertEqual(sum(len(p["Entries"]) for p in self.profiles["Profiles"]), 60)

    def test_empty_or_whitespace_hero_introduction_rejected(self):
        for value in (None, "", " \t\r\n", "\u3000"):
            with self.subTest(value=value):
                self.heroes["heroes"][0]["IntroductionZh"] = value
                with self.assertRaisesRegex(ValueError, "IntroductionZh"):
                    self.validate()

    def test_numeric_draft_or_authorization_field_rejected(self):
        original = copy.deepcopy(self.profiles)
        for field in ("Level1BalanceDraft", "BaseDamage", "DefaultAbilitySetId", "GameplayClass"):
            with self.subTest(field=field):
                self.profiles = copy.deepcopy(original)
                self.profiles["Profiles"][0]["Entries"][0][field] = 99
                with self.assertRaisesRegex(ValueError, "纯介绍字段"):
                    self.validate()

    def test_development_profile_mapping_rejected(self):
        self.settings = self.settings.replace(
            "/DBAUIPack_Core/UI/Profiles/CharacterCreation/DA_DBA_CharacterCreation_Rat.DA_DBA_CharacterCreation_Rat",
            "/Game/Development/DivineBeasts/Abilities/UI/Profiles/DA_Rat.DA_Rat")
        with self.assertRaisesRegex(ValueError, "软映射"):
            self.validate()

    def test_profile_identity_duplicates_rejected(self):
        self.profiles["Profiles"][1]["HeroDefinitionId"] = "Hero.Zodiac.Rat"
        with self.assertRaisesRegex(ValueError, "重复"):
            self.validate()

    def test_draft_marker_required_on_each_entry(self):
        self.profiles["Profiles"][0]["Entries"][1]["DescriptionZh"] = "开发验证，但遗漏批准边界。"
        with self.assertRaisesRegex(ValueError, "未批准声明"):
            self.validate()

    def test_development_id_cannot_be_formal_or_other_hero(self):
        self.profiles["Profiles"][0]["bDescriptionsAreDraft"] = False
        with self.assertRaisesRegex(ValueError, "草案"):
            self.validate()
        self.profiles["Profiles"][0]["bDescriptionsAreDraft"] = True
        self.profiles["Profiles"][0]["Entries"][0]["AbilityId"] = "dba.ability.ox_dev_basicattack@1"
        with self.assertRaisesRegex(ValueError, "技能身份"):
            self.validate()

    def test_icon_must_belong_to_same_hero_ui_pack(self):
        self.profiles["Profiles"][0]["Entries"][0]["IconAssetPath"] = "/Game/Development/Abilities/DA_Ability"
        with self.assertRaisesRegex(ValueError, "图标"):
            self.validate()

    def test_five_slot_order_cannot_change(self):
        entries = self.profiles["Profiles"][0]["Entries"]
        entries[2], entries[3] = entries[3], entries[2]
        with self.assertRaisesRegex(ValueError, "五槽顺序"):
            self.validate()


class HeroIntroductionGeneratorContractTests(unittest.TestCase):
    """引擎操作均为记录替身；验证正式函数设置作者FText，缺正文时在任何资产操作前失败。"""

    def setUp(self):
        tree = ast.parse((SOURCE_DIR / "GenerateZodiacPrototypeCharacters.py").read_text(encoding="utf-8-sig"))
        function = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "ensure_hero_definition")
        self.fields, self.saves, self.created = {}, [], []
        definition = types.SimpleNamespace(set_editor_property=lambda key, value: self.fields.__setitem__(key, value))
        factory = types.SimpleNamespace(set_editor_property=lambda key, value: None)
        unreal = types.SimpleNamespace(Name=str, Text=str, DivineBeastsHeroDefinition=object,
            DivineBeastsZodiacIdentity=object, DataAssetFactory=lambda: factory,
            EditorAssetLibrary=types.SimpleNamespace(save_loaded_asset=lambda asset, **kwargs: self.saves.append(asset)))

        def get_asset(*args):
            self.created.append(args)
            return definition

        def fail(message):
            raise RuntimeError(message)

        namespace = {"unreal": unreal, "fail": fail, "HERO_DEFINITION_ROOT": "/DBAGameplay/Definitions",
            "get_or_create_asset": get_asset, "enum_value": lambda *args: "Rat",
            "make_gameplay_tag": lambda tag: tag, "set_appearance_schema": lambda asset: None}
        exec(compile(ast.Module(body=[function], type_ignores=[]), "ensure_hero_definition", "exec"), namespace)
        self.ensure_hero = namespace["ensure_hero_definition"]
        _, heroes, _, _ = load_creation_sources(ROOT)
        self.hero = copy.deepcopy(heroes["heroes"][0])

    def test_rebuild_writes_author_text_without_granting_abilities(self):
        self.ensure_hero(self.hero)
        self.assertEqual(self.fields["introduction"], self.hero["IntroductionZh"])
        self.assertNotIn("default_ability_set_id", self.fields)
        self.assertEqual(len(self.saves), 1)

    def test_missing_introduction_stops_before_asset_creation(self):
        for value in (None, "", " \t\n", "\u3000"):
            with self.subTest(value=value):
                self.hero["IntroductionZh"] = value
                with self.assertRaisesRegex(RuntimeError, "IntroductionZh"):
                    self.ensure_hero(self.hero)
                self.assertEqual(self.created, [])
                self.assertEqual(self.saves, [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
