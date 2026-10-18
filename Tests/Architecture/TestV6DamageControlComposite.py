#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V6 伤害+控制组合权威规则源码门禁（不会启动UE、不会编译）。

验证伤害先行、单次独立控制事件、Commit后复核、原生控制能力边界与设计台账；
所有V6正式授予、GA/GE/资产制作仍必须经过独立原生验证。
"""
import json
import re
import unittest
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime"
H = BASE / "Public/Abilities/DivineBeastsV6GameplayAbility.h"
CPP = BASE / "Private/Abilities/DivineBeastsV6GameplayAbility.cpp"
REG = ROOT / "Docs/Implementation/GamePlatformAbilitySystem/V6SkillImplementationRegistry_20261018.json"
PLATFORM = ROOT / "Game/Plugins/GamePlatform/Gameplay/GamePlatformCombat/Source/GamePlatformCombat/Public/Types/GamePlatformCombatTypes.h"


def control_contract(header: str, cpp: str) -> dict[str, bool]:
    return {
        "modes_exact": all(item in header for item in (
            "DamageAndStun", "DamageAndSilence", "RadialDamageAndStun")),
        "two_action_gate": all(item in cpp for item in (
            "bDamageControlMode ? 2 : 1", "Definition.DraftActions.Num()",
            "const FDivineBeastsEightSlotDraftAction& ControlAction = Definition.DraftActions[1]")),
        "control_type_allowed": all(item in cpp for item in (
            'ControlAction.ModifierId == TEXT("stun")',
            'ControlAction.ModifierId == TEXT("silence")',
            "ControlAction.DurationSeconds <= MaxV6ControlSeconds",
            'ControlAction.ActionKindId == TEXT("control")',
            "ControlAction.HitCount == 1")),
        "source_damage_first": all(item in cpp for item in (
            "const FGamePlatformCombatResult DamageResult = Combat->ApplyDamage(Spec)",
            "if (!DamageResult.IsSuccess()) return false",
            "const FGamePlatformCombatResult ControlResult = Combat->ApplyControl(",
        )) and cpp.find("const FGamePlatformCombatResult DamageResult") < cpp.find(
            "const FGamePlatformCombatResult ControlResult"),
        "second_event_distinct": all(item in cpp for item in (
            "ControlSpec.EventId = FGuid::NewGuid()",
            "ControlSpec.SourceAbilityId",
            "ControlSpec.Magnitude = ControlAction.DurationSeconds")),
        "recheck_after_damage_callback": all(item in cpp for item in (
            "IsCurrentActivation(Token, Source, Combat, SourceGeneration)",
            "IsCurrentTarget(Source, Hero, Target, Snapshot.AvatarGeneration, false)",
            "IsV6WithinRange(Source, Target, Balance.CastRangeCm)",
            "IsV6AreaCenterValid(Source, FrozenAreaCenter, Balance.CastRangeCm)")),
        "post_damage_component_identity": all(item in cpp for item in (
            "Target->FindComponentByClass<UGamePlatformCombatComponent>() != TargetCombat",
            "!IsValid(TargetCombat)",
        )),
        "partial_success_truthful": all(item in cpp for item in (
            "控制失败或目标死亡不能撤销已提交的真实伤害",
            "return true; // 伤害已经执行，后续控制失败不得伪造全技能未命中")),
        "control_boundary": ("Slow" not in header and
             "EGamePlatformControlType::Slow" not in cpp),
        "explicit_target_vs_area": all(item in cpp for item in (
            "ExecutionMode == EDivineBeastsV6NativeMode::RadialDamageAndStun",
            'Definition.DraftTargetModeId == TEXT("area_or_target")',
            'Definition.DraftTargetModeId == TEXT("enemy")')),
    }



def control_matrix_errors(matrix: dict, registry: dict) -> list[str]:
    errors = []
    by_id = {skill["abilityId"]: skill for skill in registry["skills"]
             if skill["displayOrder"] == 7}
    items = matrix.get("items", [])
    if len(items) != 12 or len({item["abilityId"] for item in items}) != 12 or (
            set(item["abilityId"] for item in items) != set(by_id)):
        errors.append("12控制技能清单不完整或技能编号重复")
    if (matrix.get("approvedV6ControlSkills") != 0 or
        matrix.get("editorBlueprintsAuthored") != 0 or
        not matrix.get("noSlowPrimitive")):
        errors.append("未验收的控制技能不能宣称已上线或Slow已实现")
    modes = Counter(item["mode"] for item in items)
    if modes != Counter({"Unsupported": 4, "RadialDamageAndStun": 4,
                         "DamageAndStun": 2, "DamageAndSilence": 2}):
        errors.append("V2控制原语归类数量错误")
    for item in items:
        source = by_id.get(item["abilityId"])
        if source is None:
            continue
        effects = [a for a in source["v2Actions"] if a["kind"] == "control"]
        if len(effects) != 1:
            errors.append("一个控制槽不得伪造多个控制规则")
            continue
        typ = effects[0]["controlType"]
        expected_mode = ("Unsupported" if typ == "slow" else
                         "RadialDamageAndStun" if source["level1Candidate"]["areaRadiusCm"] > 0
                         else "DamageAndStun" if typ == "stun" else "DamageAndSilence")
        if (item["mode"] != expected_mode or
            item["heroId"] != source["heroId"] or
            item["displayNameV6"] != source["displayNameV6"] or
            item["controlType"] != typ or
            item["mechanismV6"] != source["mechanismV6"]):
            errors.append("控制行为／稳定ID／历史设计不一致："+item["abilityId"])
        if (item["canGrantToServer"] or
            item["historicalMechanismMatchesNativeMode"] or
            item["approvalStatus"] != "PENDING_USER_REVIEW" or
            item["nativeGameplayAbilityClassPath"] is not None or
            item["gameplayEffectClassPath"] is not None or
            item["editorAssetState"] != "NOT_AUTHORED"):
            errors.append("不能把未审的控制候选冒充已实现："+item["abilityId"])
    return errors


class V6DamageControlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = H.read_text(encoding="utf-8-sig")
        cls.cpp = CPP.read_text(encoding="utf-8-sig")
        cls.registry = json.loads(REG.read_text(encoding="utf-8-sig"))
        cls.platform = PLATFORM.read_text(encoding="utf-8-sig")

    def test_01_real_native_contract(self):
        checks = control_contract(self.header, self.cpp)
        self.assertTrue(all(checks.values()), checks)

    def test_02_12_control_skills_have_known_design_behaviour(self):
        controls = [x for x in self.registry["skills"] if x["displayOrder"] == 7]
        self.assertEqual(len(controls), 12)
        counts = Counter(next((a["controlType"] for a in x["v2Actions"]
                               if a["kind"] == "control"), "") for x in controls)
        self.assertEqual(dict(counts), {"slow": 4, "stun": 6, "silence": 2})
        self.assertTrue(all(x["canGrantToServer"] is False for x in controls))
        self.assertTrue(all(x["nativeDefinitionAssetPath"] is None for x in controls))

    def test_03_platform_only_has_stun_and_silence(self):
        self.assertRegex(self.platform, r"enum class EGamePlatformControlType")
        self.assertIn("    Stun,", self.platform)
        self.assertIn("    Silence", self.platform)
        self.assertNotIn("    Slow", self.platform)

    def test_04_never_mark_controls_as_fully_implemented(self):
        controls = [x for x in self.registry["skills"] if x["displayOrder"] == 7]
        self.assertTrue(all(x["reviewState"] == "PENDING_USER_REVIEW" for x in controls))
        self.assertTrue(all(x["vfxState"] == "NOT_APPROVED" for x in controls))
        self.assertEqual(self.registry["serverApprovedAbilityCount"], 0)

    def test_05_removing_second_unique_event_fails(self):
        altered = self.cpp.replace("ControlSpec.EventId = FGuid::NewGuid()",
                                   "ControlSpec.EventId = Spec.EventId", 1)
        self.assertFalse(control_contract(self.header, altered)["second_event_distinct"])

    def test_06_applying_control_before_damage_fails(self):
        altered = self.cpp.replace(
            "const FGamePlatformCombatResult ControlResult = Combat->ApplyControl(",
            "const FGamePlatformCombatResult ControlResult = Combat->ApplyDamage(", 1)
        self.assertFalse(control_contract(self.header, altered)["source_damage_first"])

    def test_07_disabling_partial_success_contract_fails(self):
        altered = self.cpp.replace(
            "return true; // 伤害已经执行，后续控制失败不得伪造全技能未命中",
            "return false;")
        self.assertFalse(control_contract(self.header, altered)["partial_success_truthful"])

    def test_07b_reject_stale_combat_component_after_damage_callback(self):
        altered = self.cpp.replace(
            "Target->FindComponentByClass<UGamePlatformCombatComponent>() != TargetCombat",
            "false", 1)
        self.assertFalse(control_contract(self.header, altered)["post_damage_component_identity"])

    def test_08_do_not_silently_add_slow_support(self):
        altered = self.header.replace("DamageAndStun,", "DamageAndStun,\n    Slow,", 1)
        self.assertFalse(control_contract(altered, self.cpp)["control_boundary"])
    def test_09_native_positive_and_negative_control_action_tests_exist(self):
        native = (ROOT / "Game/Plugins/DivineBeasts/DBAGameplay/Source/"
                  "DivineBeastsAbilitiesRuntime/Private/Tests/"
                  "DivineBeastsV6AbilityContractTests.cpp").read_text(encoding="utf-8-sig")
        self.assertIn("DivineBeasts.Abilities.V6.DamageControlPairContract", native)
        for token in ("一次短眩晕合法", "一次短沉默合法",
                      "平台当前不支持Slow", "两段重复控制须专门授权",
                      "超3秒控制拒绝"):
            self.assertIn(token, native)

    def test_10_existing_v6_enum_ordinals_preserved(self):
        block = self.header.split("enum class EDivineBeastsV6NativeMode")[1].split("};")[0]
        names = re.findall(r"^\s*(Unsupported|ForwardDamage|SingleProjectile|RadialDamage|"
                           r"SingleHealing|SingleShield|SingleStun|SingleSilence|SafeDash|"
                           r"DamageAndStun|DamageAndSilence|RadialDamageAndStun)\s*,?\s*$",
                           block, flags=re.MULTILINE)
        self.assertEqual(names, [
            "Unsupported", "ForwardDamage", "SingleProjectile", "RadialDamage",
            "SingleHealing", "SingleShield", "SingleStun", "SingleSilence",
            "SafeDash", "DamageAndStun", "DamageAndSilence", "RadialDamageAndStun"])

    def test_11_12_control_skill_approval_matrix_matches_true_v6(self):
        matrix_path = (ROOT / "Docs/Implementation/GamePlatformAbilitySystem/"
                       "V6ControlSkillImplementationMatrix_20261018.json")
        matrix = json.loads(matrix_path.read_text(encoding="utf-8-sig"))
        self.assertEqual(control_matrix_errors(matrix, self.registry), [])

    def test_12_registry_review_flags_cannot_be_automatically_promoted(self):
        matrix_path = (ROOT / "Docs/Implementation/GamePlatformAbilitySystem/"
                       "V6ControlSkillImplementationMatrix_20261018.json")
        matrix = json.loads(matrix_path.read_text(encoding="utf-8-sig"))
        tampered = json.loads(json.dumps(matrix, ensure_ascii=False))
        tampered["items"][0]["canGrantToServer"] = True
        self.assertNotEqual(control_matrix_errors(tampered, self.registry), [])
        tampered = json.loads(json.dumps(matrix, ensure_ascii=False))
        tampered["items"][0]["mode"] = "DamageAndStun"
        self.assertNotEqual(control_matrix_errors(tampered, self.registry), [])




if __name__ == "__main__":
    unittest.main(verbosity=2)
