#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM十二生肖八技能设计／输入／DataAsset制作源静态门禁。

只读取JSON、CSV、C++与Ini；不会导入/修改.uasset，不触发UHT/UBT或运行编辑器。
96份设计草案不等同于96个已执行GameplayAbility。
"""
from __future__ import annotations
import copy
import ast
import csv
import io
import json
import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/Unreal/Abilities"))
import ZodiacEightSlotDraftContract as contract_module
from ZodiacEightSlotDraftContract import ContractError, build_contract, build_manifest
CATALOG_PATH = ROOT / "Docs/Design/ZodiacEightSlotSkills_V2.json"
BALANCE_PATH = ROOT / ("Game/Plugins/DivineBeasts/DBAGameplay/SourceArt/Abilities/"
                        "DT_DBA_ZodiacEightSlotDraftBalance_V2.csv")
PLAN_PATH = ROOT / "Docs/Implementation/ZodiacEightSlotDraftAssetPlan_V2.json"
GAME = ROOT / "Game"
EXPECTED_HEROES = {
    "Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake",
    "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar"
}
SLOTS = ["Primary", "Slot1", "Slot2", "Slot4", "Slot3", "Slot5", "Slot6", "Slot7"]
ROLES = ["basic", "attack_a", "attack_b", "ultimate", "mobility", "defense", "control", "signature"]
HEALER_CODES = {"rabbit", "goat", "rooster", "dog"}
KINDS = {"damage", "movement", "shield", "healing", "control", "buff"}
ENGINES = {"Physical", "Magic", "Untyped"}


def source(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8-sig")


def load() -> dict:
    return json.loads(CATALOG_PATH.read_text(encoding="utf-8-sig"))


def all_skills(catalog: dict) -> list[dict]:
    return [row for hero in catalog["heroes"] for row in hero["slots"]]


def balanced_csv() -> dict[str, dict]:
    rows = list(csv.DictReader(io.StringIO(BALANCE_PATH.read_text(encoding="utf-8-sig"), newline="")))
    return {row["AbilityId"]: row for row in rows}


def audit(catalog: dict) -> list[str]:
    errors = []
    def fail(ok, message):
        if not ok:
            errors.append(message)

    fail(catalog.get("schemaVersion") == 2, "结构版本非法")
    fail(catalog.get("nativeUEAssetStatus") == "NOT_AUTHORED", "不能在未制作前标记真实.uasset完成")
    fail(catalog.get("designerApprovalStatus") == "PENDING_HUMAN_BALANCE_REVIEW", "禁止冒充数值正式获批")
    fail(catalog.get("balanceStatus") == "LEVEL1_CANDIDATE_NOT_BALANCED",
         "正式数值尚未平衡，不得误设为已批准")
    fail(catalog["slotPolicy"]["maxActiveSlots"] == 8, "不是八个可装载主动技能")
    fail(not catalog["slotPolicy"]["innatePassiveOccupiesSlot"], "固有被动不能挤占八技能槽")
    heroes = catalog["heroes"]
    fail(len(heroes) == 12, "不是12位生肖")
    fail({x["heroId"].rsplit(".", 1)[-1] for x in heroes} == EXPECTED_HEROES, "生肖目录不完整")
    all_ids = set()
    for hero in heroes:
        code = hero["heroCode"]
        rows = hero["slots"]
        fail(len(rows) == 8, f"{code}:槽数不为8")
        fail([x["slotId"] for x in rows] == SLOTS, f"{code}:显示槽顺序变更")
        fail([x["role"] for x in rows] == ROLES, f"{code}:攻防移动控制分类不完整")
        fail(bool(hero["innateTrait"]["nameZh"]) and
             hero["innateTrait"]["slotOccupancy"] == 0, f"{code}:独立固有被动不存在")
        fail(hero["proposedAbilitySetId"] == f"dba.abilityset.{code}_eight_draft@1",
             f"{code}:新版技能集草案ID不一致")
        for row in rows:
            identity = row["abilityId"]
            fail(re.fullmatch(r"dba\.ability\.[a-z0-9_]+@1", identity) is not None,
                 f"{code}:技能逻辑ID格式非法:{identity}")
            fail(identity.startswith(f"dba.ability.{code}_eight_"), f"{code}:技能ID跨英雄")
            fail(identity not in all_ids, f"{code}:重复ID")
            all_ids.add(identity)
            fail(not row["approvedForLiveGAS"] and
                 row["implementationStatus"] == "DESIGN_ONLY_UNIMPLEMENTED",
                 f"{identity}:没有实现不得授予")
            fail(row["level"] == 1, f"{identity}:必须只使用可验证的一级数值")
            fail(row["damageType"] in ENGINES, f"{identity}:伤害类型未知")
            fail(row["baseDamage"] >= 0 and 0 <= row["cooldownSeconds"] <= 120 and
                 row["energyCost"] == 0 and 0 <= row["momentumCost"] <= 100,
                 f"{identity}:数值越界或虚构蓝色能量成本")
            fail(row["damageType"] != "Magic" or row["momentumCost"] == 0,
                 f"{identity}:法术不得扣气势")
            fail(row["vfx"]["heroProfileId"] ==
                 "Presentation.VFX.Hero.Zodiac." + hero["heroId"].split(".")[-1] + ".Default",
                 f"{identity}:生肖VFX Profile串用")
            fail(row["vfx"]["assetPath"] is None and row["icon"]["assetPath"] is None,
                 f"{identity}:未制作VFX/UI不应伪造资产路径")
            actions = row["actions"]
            fail(bool(actions), f"{identity}:没有真实设计行为")
            fail(all(a["kind"] in KINDS for a in actions), f"{identity}:包含未知行为")
            damage_actions = [a for a in actions if a["kind"] == "damage"]
            heal_actions = [a for a in actions if a["kind"] == "healing"]
            fail(all(a.get("type") == row["damageType"] for a in damage_actions),
                 f"{identity}:行为明细伤害类型与数值行不一致")
            fail(not (damage_actions and heal_actions) or row["intentTarget"] == "area_center_mixed",
                 f"{identity}:混合伤害治疗必须声明明确范围中心目标")
            fail(all(a.get("baseMagnitude") == row["baseDamage"] for a in damage_actions),
                 f"{identity}:每段基础伤害与一级行不一致")
            fail(row["role"] != "mobility" or any(a["kind"] == "movement" for a in actions),
                 f"{identity}:移动槽没有合法位移")
            fail(row["role"] != "defense" or any(a["kind"] == "shield" for a in actions),
                 f"{identity}:防御槽没有护盾")
            fail(row["role"] != "control" or any(a["kind"] == "control" for a in actions),
                 f"{identity}:控制槽没有控制")
            fail(row["role"] != "basic" or row["baseDamage"] > 0, f"{identity}:普攻不能是治疗")
    skills = all_skills(catalog)
    fail(len(skills) == 96 and len(all_ids) == 96, "96技能数量或唯一性不满足")
    healers = {hero["heroCode"] for hero in heroes
               if any(a["kind"] == "healing" for row in hero["slots"] for a in row["actions"])}
    fail(healers == HEALER_CODES, "只有卯兔、未羊、酉鸡、戌狗具有明确治疗职责")
    try:
        build_contract(catalog, BALANCE_PATH.read_text(encoding="utf-8-sig"))
    except ContractError as error:
        errors.append("原生作者契约失败:" + str(error))
    return errors


class ZodiacEightSlotDraftTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = load()

    def test_01_full_catalog_contract(self):
        self.assertEqual(audit(self.catalog), [])

    def test_02_96_rows_match_csv(self):
        rows = list(csv.DictReader(io.StringIO(BALANCE_PATH.read_text(encoding="utf-8-sig"), newline="")))
        self.assertEqual(len(rows), 96)
        self.assertEqual(len({r["AbilityId"] for r in rows}), 96)
        for row in all_skills(self.catalog):
            record = next(r for r in rows if r["AbilityId"] == row["abilityId"])
            self.assertEqual(record["Name"], row["abilityId"].rsplit(".", 1)[-1].removesuffix("@1") + "_L1")
            for key, expected in (
                ("BaseDamage", row["baseDamage"]), ("CooldownSeconds", row["cooldownSeconds"]),
                ("MomentumCost", row["momentumCost"]), ("CastRangeCm", row["castRangeCm"]),
                ("AreaRadiusCm", row["areaRadiusCm"])
            ):
                self.assertAlmostEqual(float(record[key]), float(expected))
            self.assertEqual(record["DamageType"], row["damageType"])

    def test_03_8_slot_input_and_visible_widget_fields(self):
        input_source = source("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsInputClient/"
                              "Private/Input/DivineBeastsInputSemantics.cpp")
        panel = source("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/"
                       "Private/Panels/Combat/DivineBeastsAbilityBarPanel.cpp")
        panel_header = source("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/"
                              "Public/Panels/Combat/DivineBeastsAbilityBarPanel.h")
        tags = source("Game/Config/DefaultGameplayTags.ini")
        input_ini = source("Game/Config/DefaultInput.ini")
        for slot in self.catalog["slotPolicy"]["inputSemantics"]:
            self.assertIn(slot["tag"], tags)
            self.assertIn('ActionName="' + slot["action"] + '"', input_ini)
            self.assertIn(slot["tag"], panel)
            self.assertIn(slot["tag"], input_source if slot["slot"] in ("Primary", "Slot1",
                                                           "Slot2", "Slot3", "Slot4", "Slot5", "Slot6", "Slot7")
                          else panel)
        for name in ("MovementAbilitySlot", "DefenseAbilitySlot",
                     "ControlAbilitySlot", "SignatureAbilitySlot"):
            self.assertIn(name, panel_header)
            self.assertIn(name + ".Get()", panel)
        self.assertIn("DistinctActiveInputSlots.Num() == 8", panel)

    def test_04_draft_assets_fail_closed(self):
        definition = source("Game/Plugins/DivineBeasts/DBAGameplay/Source/"
                            "DivineBeastsAbilitiesRuntime/Public/Definitions/DivineBeastsAbilityDefinition.h")
        definition_cpp = source("Game/Plugins/DivineBeasts/DBAGameplay/Source/"
                                "DivineBeastsAbilitiesRuntime/Private/Definitions/DivineBeastsAbilityDefinition.cpp")
        grant = source("Game/Plugins/DivineBeasts/DBAGameplay/Source/"
                       "DivineBeastsAbilitiesRuntime/Private/Components/DivineBeastsAbilityLoadoutComponent.cpp")
        author = source("Tools/Unreal/Abilities/AuthorZodiacEightSlotDesignAssets.py")
        self.assertIn("bEightSlotDraftOnly = false", definition)
        self.assertIn("if (Definition->bEightSlotDraftOnly)", grant)
        self.assertIn("unreal.DataTableFactory", author)
        self.assertIn("unreal.DataAssetFactory", author)
        self.assertIn("build_contract(catalog, balance_csv)", author)
        self.assertIn("preflight_existing_assets(", author)
        self.assertIn("DraftAssetOwnerId", author)
        self.assertIn("DraftCatalogId", author)
        self.assertIn("DraftSourceSha256", definition)
        self.assertIn("SourcePayloadJson", definition)
        self.assertIn("DraftPresentationSemanticIds", definition)
        self.assertIn("EightSlotDraftMagnitudeSemanticInvalid", definition_cpp)
        self.assertIn("EightSlotDraftReviveNotApproved", definition_cpp)
        self.assertIn("DraftDesignPromptZh", definition)
        self.assertIn("WITH_EDITORONLY_DATA", definition)
        self.assertIn("DraftDesignPromptZh", author)
        ast.parse(author, filename="Tools/Unreal/Abilities/AuthorZodiacEightSlotDesignAssets.py")
        self.assertNotIn("compile_blueprint(", author)
        self.assertNotIn("set_editor_property(\"DefaultAbilitySetId\"", author)

    def test_05_reject_missing_skill(self):
        altered = copy.deepcopy(self.catalog)
        altered["heroes"][0]["slots"].pop()
        self.assertNotEqual(audit(altered), [])

    def test_06_reject_duplicate_ability_id(self):
        altered = copy.deepcopy(self.catalog)
        altered["heroes"][1]["slots"][0]["abilityId"] = altered["heroes"][0]["slots"][0]["abilityId"]
        self.assertNotEqual(audit(altered), [])

    def test_07_reject_false_live_approval(self):
        altered = copy.deepcopy(self.catalog)
        altered["heroes"][0]["slots"][0]["approvedForLiveGAS"] = True
        self.assertNotEqual(audit(altered), [])

    def test_08_reject_stealth_ability_without_movement(self):
        altered = copy.deepcopy(self.catalog)
        altered["heroes"][0]["slots"][4]["actions"] = [{"kind": "buff"}]
        self.assertNotEqual(audit(altered), [])

    def test_09_reject_missing_healing_capability(self):
        altered = copy.deepcopy(self.catalog)
        altered["heroes"][3]["slots"][7]["actions"] = [{"kind": "buff"}]
        altered["heroes"][3]["slots"][3]["actions"] = [{"kind": "damage"}]
        self.assertNotEqual(audit(altered), [])

    def test_10_preserves_legacy_primary_and_ultimate_tags(self):
        semantics = self.catalog["slotPolicy"]["inputSemantics"]
        self.assertEqual(semantics[0]["tag"], "Platform.Ability.Input.DivineBeasts.Primary")
        self.assertEqual(semantics[3]["tag"], "Platform.Ability.Input.DivineBeasts.Slot4")
        self.assertEqual([x["keyboard"] for x in semantics], list("12345678"))

    def test_11_contract_maps_every_action_and_owned_asset_path(self):
        balance_csv = BALANCE_PATH.read_text(encoding="utf-8-sig")
        contract = build_contract(self.catalog, balance_csv)
        self.assertEqual(len(contract["skills"]), 96)
        self.assertEqual(len(contract["balanceRows"]), 96)
        self.assertEqual(len(contract["heroes"]), 12)
        self.assertEqual(len({x["asset_path"] for x in contract["skills"]}), 96)
        for mapped in contract["skills"]:
            self.assertTrue(mapped["asset_path"].startswith(
                "/DBAGameplay/Abilities/EightSlotDrafts/DA_DBA_"))
            self.assertEqual(mapped["balance_row"]["AbilityId"],
                             mapped["entry"]["abilityId"])
            self.assertEqual(mapped["input"]["slot"], mapped["entry"]["slotId"])
            self.assertEqual(len(mapped["source_sha256"]), 64)
            self.assertEqual(len(mapped["actions"]), len(mapped["entry"]["actions"]))
            for converted, original in zip(mapped["actions"], mapped["entry"]["actions"]):
                self.assertEqual(json.loads(converted["SourcePayloadJson"]), original)
        manifest = build_manifest(self.catalog, balance_csv)
        self.assertEqual(manifest["nativeAssetStatus"], "NOT_AUTHORED")
        self.assertFalse(manifest["formalAbilitySetAssignmentPerformed"])
        self.assertFalse(manifest["liveGASAuthorizationPerformed"])

    def test_12_rejects_drifted_balance_rows(self):
        original = BALANCE_PATH.read_text(encoding="utf-8-sig")
        changed = original.replace(
            "rat_eight_fang_cut_L1,dba.ability.rat_eight_fang_cut@1,1,32,Physical",
            "rat_eight_fang_cut_L1,dba.ability.rat_eight_fang_cut@1,1,33,Physical",
            1,
        )
        self.assertNotEqual(changed, original)
        with self.assertRaises(ContractError):
            build_contract(self.catalog, changed)

    def test_13_rejects_unknown_action_fields_and_input_slot_drift(self):
        altered = copy.deepcopy(self.catalog)
        altered["heroes"][0]["slots"][0]["actions"][0]["unmappedFutureField"] = "fail-closed"
        with self.assertRaises(ContractError):
            build_contract(altered, BALANCE_PATH.read_text(encoding="utf-8-sig"))

        altered = copy.deepcopy(self.catalog)
        altered["slotPolicy"]["inputSemantics"][0]["tag"] = "Platform.Ability.Input.DivineBeasts.Slot1"
        with self.assertRaises(ContractError):
            build_contract(altered, BALANCE_PATH.read_text(encoding="utf-8-sig"))

    def test_14_checked_in_asset_plan_matches_generator(self):
        balance_csv = BALANCE_PATH.read_text(encoding="utf-8-sig")
        checked_in = json.loads(PLAN_PATH.read_text(encoding="utf-8-sig"))
        self.assertEqual(checked_in, build_manifest(self.catalog, balance_csv))
        for hero in checked_in["heroes"]:
            self.assertEqual(len(hero["skills"]), 8)
            self.assertTrue(hero["proposedAbilitySetIdNotApplied"].endswith("_eight_draft@1"))
            for skill in hero["skills"]:
                self.assertTrue(skill["draftOnly"])
                self.assertFalse(skill["approvedForLiveGAS"])
        self.assertEqual(checked_in["resourceStatus"], {
            "nativeDataAssets": "NOT_AUTHORED",
            "nativeBalanceDataTable": "NOT_AUTHORED",
            "vfxAssetsAuthored": False,
            "animationAssetsAuthored": False,
            "iconAssetsBound": False,
        })


    def test_03b_eight_legacy_native_action_bindings_match_catalog(self):
        # 工程默认EnhancedInput Profile仍未正式制作为原生资产，数字5—8必须保留
        # Pawn现有按键兼容通路，不能只在Ini中增加没有消费端的动作。
        character = source("Game/Plugins/DivineBeasts/DBAGameplay/Source/"
                           "DivineBeastsAbilitiesRuntime/Private/Characters/DivineBeastsGameplayCharacter.cpp")
        action_match = re.search(r'const TCHAR\* Actions\[\]\s*=\s*\{(.*?)\};', character, re.S)
        tag_match = re.search(r'const TCHAR\* AbilityInputTagNames\[\]\s*=\s*\{(.*?)\};', character, re.S)
        self.assertIsNotNone(action_match)
        self.assertIsNotNone(tag_match)
        actions = re.findall(r'TEXT\("([^"]+)"\)', action_match.group(1))
        tags = re.findall(r'TEXT\("([^"]+)"\)', tag_match.group(1))
        self.assertEqual(actions, [row["action"] for row in self.catalog["slotPolicy"]["inputSemantics"]])
        self.assertEqual(tags, [row["tag"] for row in self.catalog["slotPolicy"]["inputSemantics"]])
        self.assertIn("IE_Pressed", character)
        self.assertIn("IE_Released", character)
        self.assertIn("GetInputToken()", character)
        self.assertIn("ProcessAbilityInput()", character)

    def test_04b_reject_wrong_damage_action_type(self):
        altered = copy.deepcopy(self.catalog)
        control = altered["heroes"][0]["slots"][6]
        damage = next(a for a in control["actions"] if a["kind"] == "damage")
        damage["type"] = "Untyped"
        self.assertTrue(any("伤害类型" in err for err in audit(altered)))

    def test_04c_reject_mixed_heal_target_as_friendly_only(self):
        altered = copy.deepcopy(self.catalog)
        ultimate = altered["heroes"][3]["slots"][3]
        ultimate["intentTarget"] = "friendly_or_self"
        self.assertTrue(any("范围中心" in err for err in audit(altered)))

    def test_11_ability_set_manifest_matches_96_skills_without_false_grants(self):
        manifest = json.loads(source("Docs/Design/ZodiacEightSlotAbilitySetManifest_V2.json"))
        self.assertEqual(manifest["state"], "DESIGN_MANIFEST_ONLY")
        self.assertEqual(manifest["abilitySetsAuthored"], 0)
        self.assertEqual(manifest["gaBlueprintsAuthored"], 0)
        self.assertEqual(len(manifest["grantsByHero"]), 12)
        by_hero = {h["heroId"]: h for h in self.catalog["heroes"]}
        seen = set()
        for group in manifest["grantsByHero"]:
            design = by_hero[group["heroDefinitionId"]]
            self.assertEqual(group["abilityCount"], 8)
            self.assertTrue(group["doNotOverrideHeroDefaultAbilitySetId"])
            self.assertEqual(group["draftSetLogicalId"], design["proposedAbilitySetId"])
            self.assertEqual(len(group["grants"]), 8)
            for grant, skill in zip(group["grants"], design["slots"]):
                self.assertEqual(grant["logicalAbilityId"], skill["abilityId"])
                self.assertEqual(grant["slotId"], skill["slotId"])
                self.assertFalse(grant["approvedForGrant"])
                self.assertIsNone(grant["gameplayAbilityClassPath"])
                self.assertIsNone(grant["vfxDefinitionPath"])
                self.assertNotIn(skill["abilityId"], seen)
                seen.add(skill["abilityId"])
        self.assertEqual(len(seen), 96)

    def test_12_native_widget_authoring_spec_is_eight_slots_not_faked_assets(self):
        spec = json.loads(source("Tools/Unreal/UI/EightSlotAbilityBarWidgetSpec_V2.json"))
        self.assertEqual(spec["state"], "PENDING_UE58_MONOLITH_NATIVE_AUTHORING")
        self.assertEqual(len(spec["slots"]), 8)
        self.assertEqual([x["gameplayTag"] for x in spec["slots"]],
                         [s["tag"] for s in self.catalog["slotPolicy"]["inputSemantics"]])
        self.assertEqual([x["keyText"] for x in spec["slots"]], list("12345678"))
        self.assertEqual([x["widgetName"] for x in spec["slots"]],
                         ["PrimaryAbilitySlot", "ActiveAbilitySlot1", "ActiveAbilitySlot2",
                          "UltimateAbilitySlot", "MovementAbilitySlot", "DefenseAbilitySlot",
                          "ControlAbilitySlot", "SignatureAbilitySlot"])
        self.assertLessEqual(spec["layout"]["maxTotalHeightPx"], 65)
        self.assertTrue(spec["layout"]["castBarOverlayForbidden"])
        self.assertTrue(spec["layout"]["momentumBarSeparateFromBlueEnergy"])
        self.assertEqual(spec["layout"]["actualParentPanelName"],
                         "DISCOVER_FROM_REAL_WIDGET_TREE_BEFORE_EDIT")
        # 现有v02审核稿与历史65px约束不能偷换：其8个可见框须复用，
        # 当前只对其中5个做过NativeWidget绑定，重命名和重排待UE资产工具。
        self.assertEqual(spec["layout"]["approvedV02AssetWidthPx"], 727)
        self.assertEqual(spec["layout"]["approvedV02AssetHeightPx"], 94)
        self.assertTrue(spec["layout"]["heightContractConflict"])
        self.assertTrue(spec["layout"]["useEightExistingFramesNoNewNinthFrame"])
        self.assertEqual(len(spec["layout"]["existingIconFrameRectsPx"]), 8)
        self.assertEqual([r[0] for r in spec["layout"]["existingIconFrameRectsPx"]],
                         [67, 123, 178, 233, 288, 343, 398, 453])
        self.assertEqual(spec["layout"]["currentSavedWidgetSlots"],
                         ["PrimaryAbilitySlot", "ActiveAbilitySlot1", "UltimateAbilitySlot",
                          "ActiveAbilitySlot2", "PassiveAbilitySlot", None, None, None])

    def test_15_chinese_text_survives_contract_and_asset_manifest(self):
        """移除文本映射或清单中的中文字段时必须失败，防止再次只交付英文对象名。"""
        for row in all_skills(self.catalog):
            self.assertIn("designPromptZh", row, row["abilityId"] + ":缺少中文设计提示词")
        contract = build_contract(self.catalog, BALANCE_PATH.read_text(encoding="utf-8-sig"))
        manifest = build_manifest(self.catalog, BALANCE_PATH.read_text(encoding="utf-8-sig"))
        planned = {s["abilityId"]: s for h in manifest["heroes"] for s in h["skills"]}
        for skill in contract["skills"]:
            row = skill["entry"]
            self.assertIn("draft_text", skill, "作者转换必须显式保留三项中文文本")
            self.assertEqual(skill["draft_text"], {
                "DraftDisplayNameZh": row["nameZh"],
                "DraftDescriptionZh": row["descriptionZh"],
                "DraftDesignPromptZh": row["designPromptZh"],
            })
            for field in ("nameZh", "descriptionZh", "designPromptZh"):
                self.assertEqual(planned[row["abilityId"]][field], row[field])
            self.assertIn(row["nameZh"], row["designPromptZh"])
            self.assertIn(skill["hero"]["heroNameZh"], row["designPromptZh"])

    def test_16_reject_missing_blank_or_non_chinese_skill_text(self):
        """三个中文字段均拒绝缺失、空白和纯英文占位，保证审核内容可读。"""
        balance_csv = BALANCE_PATH.read_text(encoding="utf-8-sig")
        for field in ("nameZh", "descriptionZh", "designPromptZh"):
            for invalid in (None, "", "   ", "English placeholder"):
                with self.subTest(field=field, invalid=invalid):
                    altered = copy.deepcopy(self.catalog)
                    altered["heroes"][0]["slots"][0][field] = invalid
                    with self.assertRaises(ContractError):
                        build_contract(altered, balance_csv)
            with self.subTest(field=field, missing=True):
                altered = copy.deepcopy(self.catalog)
                altered["heroes"][0]["slots"][0].pop(field, None)
                with self.assertRaises(ContractError):
                    build_contract(altered, balance_csv)

    def test_17_reject_prompt_assigned_to_other_hero_or_skill(self):
        """提示词误贴到另一英雄或另一技能时拒绝制作，避免96项审核材料串用。"""
        first = self.catalog["heroes"][0]["slots"][0]
        self.assertIn("designPromptZh", first)
        balance_csv = BALANCE_PATH.read_text(encoding="utf-8-sig")
        for hero_index, slot_index in ((0, 1), (1, 0)):
            altered = copy.deepcopy(self.catalog)
            altered["heroes"][0]["slots"][0]["designPromptZh"] = (
                altered["heroes"][hero_index]["slots"][slot_index]["designPromptZh"])
            with self.subTest(hero_index=hero_index, slot_index=slot_index):
                with self.assertRaises(ContractError):
                    build_contract(altered, balance_csv)

    def test_18_text_change_changes_source_digest_without_mutating_source(self):
        """仅修改美术提示词也必须改变源摘要，不能将旧文本资产误记为本次制作。"""
        self.assertIn("designPromptZh", self.catalog["heroes"][0]["slots"][0])
        original = copy.deepcopy(self.catalog)
        balance_csv = BALANCE_PATH.read_text(encoding="utf-8-sig")
        before = build_contract(self.catalog, balance_csv)
        altered = copy.deepcopy(self.catalog)
        altered["heroes"][0]["slots"][0]["designPromptZh"] += " 保持主体轮廓清晰。"
        after = build_contract(altered, balance_csv)
        self.assertNotEqual(before["skills"][0]["source_sha256"], after["skills"][0]["source_sha256"])
        self.assertEqual(before["balanceRows"], after["balanceRows"])
        self.assertEqual(self.catalog, original)

    def test_19_chinese_review_document_matches_validated_source(self):
        """中文审核索引由同一契约生成；遗漏或手改任意技能说明/提示词时必须失败。"""
        self.assertTrue(hasattr(contract_module, "render_text_review"), "缺少中文审核索引生成器")
        review = contract_module.render_text_review(
            self.catalog, BALANCE_PATH.read_text(encoding="utf-8-sig"))
        checked_in = source("Docs/Design/ZodiacEightSlotSkillTextReview_V2.md")
        self.assertEqual(checked_in, review)
        self.assertEqual(len(re.findall(r"^### [1-8]\.", review, re.M)), 96)
        for hero in self.catalog["heroes"]:
            self.assertIn(hero["heroNameZh"], review)
            for skill in hero["slots"]:
                for field in ("nameZh", "descriptionZh", "designPromptZh"):
                    self.assertIn(skill[field], review)

    def test_20_self_buffs_do_not_require_selecting_an_enemy(self):
        """三项正文明确作用自身的特色增益不得错误保留敌方目标选择。"""
        ids = {"tiger_eight_battle_fervor", "monkey_eight_trickster_stride", "boar_eight_rage_focus"}
        found = [s for s in all_skills(self.catalog)
                 if s["abilityId"].rsplit(".", 1)[-1].removesuffix("@1") in ids]
        self.assertEqual(len(found), 3)
        self.assertTrue(all(s["intentTarget"] == "self" for s in found))
        altered = copy.deepcopy(self.catalog)
        altered["heroes"][2]["slots"][7]["intentTarget"] = "enemy"
        with self.assertRaises(ContractError):
            build_contract(altered, BALANCE_PATH.read_text(encoding="utf-8-sig"))


    def test_21_asset_package_paths_and_full_object_paths_are_distinct(self):
        """UE返回包名.对象名；包路径用于创建，完整对象路径用于保存后身份比较。"""
        manifest = build_manifest(self.catalog, BALANCE_PATH.read_text(encoding="utf-8-sig"))
        self.assertTrue("balanceTablePackagePath" in manifest, "缺少共享表包路径字段")
        table_package = manifest["balanceTablePackagePath"]
        self.assertEqual(manifest["balanceTableObjectPath"],
                         table_package + "." + table_package.rsplit("/", 1)[-1])
        for hero in manifest["heroes"]:
            for skill in hero["skills"]:
                self.assertIn("assetPackagePath", skill)
                package = skill["assetPackagePath"]
                self.assertEqual(skill["assetObjectPath"],
                                 package + "." + package.rsplit("/", 1)[-1])


    def test_22_all_96_asset_specs_include_cast_time_balance_and_placeholders(self):
        """制作清单必须可逐项读出用户要求的数值和占位，不只列96个资源名字。"""
        for row in all_skills(self.catalog):
            self.assertTrue("castTimeSeconds" in row, row["abilityId"] + ":缺少施法时间")
        contract = build_contract(self.catalog, BALANCE_PATH.read_text(encoding="utf-8-sig"))
        manifest = build_manifest(self.catalog, BALANCE_PATH.read_text(encoding="utf-8-sig"))
        planned = {s["abilityId"]: s for h in manifest["heroes"] for s in h["skills"]}
        for skill in contract["skills"]:
            row = skill["entry"]
            self.assertEqual(skill["balance_row"]["CastTimeSeconds"], row["castTimeSeconds"])
            self.assertEqual(planned[row["abilityId"]]["balanceRow"], skill["balance_row"])
            self.assertEqual(planned[row["abilityId"]]["hitCount"], row["hitCount"])
            self.assertEqual(planned[row["abilityId"]]["placeholders"], skill["placeholders"])
            self.assertTrue(skill["placeholders"]["bDraftPresentationUnbound"])
            self.assertIsNone(row["icon"]["assetPath"])
            self.assertIsNone(row["vfx"]["assetPath"])

    def test_23_cast_time_rejects_missing_negative_nonfinite_or_bool(self):
        """施法准备时间为有限秒数，不能把缺失字段当零或把布尔当合法数字。"""
        balance_csv = BALANCE_PATH.read_text(encoding="utf-8-sig")
        for value in (-1, 31, float("nan"), float("inf"), True, None, "0.5"):
            with self.subTest(value=repr(value)):
                changed = copy.deepcopy(self.catalog)
                changed["heroes"][0]["slots"][0]["castTimeSeconds"] = value
                with self.assertRaises(ContractError):
                    build_contract(changed, balance_csv)
        changed = copy.deepcopy(self.catalog)
        changed["heroes"][0]["slots"][0].pop("castTimeSeconds", None)
        with self.assertRaises(ContractError):
            build_contract(changed, balance_csv)

    def test_24_placeholder_identity_is_required_without_fake_asset_paths(self):
        """未绑定图标和VFX占位必须有明确身份；本轮不允许以虚构资源路径冒充已制作。"""
        balance_csv = BALANCE_PATH.read_text(encoding="utf-8-sig")
        for field in ("icon", "vfx"):
            with self.subTest(field=field):
                changed = copy.deepcopy(self.catalog)
                changed["heroes"][0]["slots"][0][field].pop("placeholderId", None)
                with self.assertRaises(ContractError):
                    build_contract(changed, balance_csv)

    def test_25_native_author_requires_cast_and_placeholder_reflection(self):
        """旧编辑器缺少新行字段或占位字段时必须预检拒绝；静态门禁不冒充原生编译。"""
        row_h = source("Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/"
                       "Public/Types/DivineBeastsAbilityBalanceRow.h")
        row_cpp = source("Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/"
                         "Private/Types/DivineBeastsAbilityBalanceRow.cpp")
        author = source("Tools/Unreal/Abilities/AuthorZodiacEightSlotDesignAssets.py")
        definition = source("Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/"
                            "Public/Definitions/DivineBeastsAbilityDefinition.h")
        self.assertIn("float CastTimeSeconds = 0.0f;", row_h)
        self.assertIn("CastTimeSeconds", row_cpp)
        self.assertIn("unreal.DivineBeastsAbilityBalanceRow()", author)
        for name in ("DraftIconPlaceholderId", "DraftVFXPlaceholderId", "bDraftPresentationUnbound"):
            self.assertIn(name, definition)
            self.assertIn(name, author)


    def test_26_native_table_export_detects_cast_time_loss_or_row_drift(self):
        """原生导入后的JSON必须保留全部候选数值；施法时间变零、重复/缺行均拒绝保存。"""
        self.assertTrue(hasattr(contract_module, "validate_exported_balance_rows"),
                        "缺少原生数据表逐行回读校验")
        contract = build_contract(self.catalog, BALANCE_PATH.read_text(encoding="utf-8-sig"))
        rows = copy.deepcopy(contract["balanceRows"])
        self.assertEqual(contract_module.validate_exported_balance_rows(
            contract, json.dumps(rows)), 96)
        bad = copy.deepcopy(rows)
        bad[0]["CastTimeSeconds"] = 0
        with self.assertRaises(ContractError):
            contract_module.validate_exported_balance_rows(contract, json.dumps(bad))
        for bad in (rows[:-1], rows[:-1] + [rows[0]]):
            with self.assertRaises(ContractError):
                contract_module.validate_exported_balance_rows(contract, json.dumps(bad))


if __name__ == "__main__":
    unittest.main(verbosity=2)

