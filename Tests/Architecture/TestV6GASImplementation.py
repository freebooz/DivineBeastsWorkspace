#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V6技能执行合同非编译回归。

本文件验证已写入的原生C++接线、安全闸、96身份和覆盖台账。
它不是UE5.8编译、真正Actor运行、蓝图冷读或多人服务器验收。
"""
import copy
import ast
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REG = ROOT / "Docs/Implementation/GamePlatformAbilitySystem/V6SkillImplementationRegistry_20261018.json"
V6 = ROOT / "Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json"
SRC = ROOT / "Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime"
DOCUMENT = ROOT / "Docs/Implementation/GamePlatformAbilitySystem/V6SkillImplementation_20261018.md"
INDEX = ROOT / "Docs/Implementation/GamePlatformAbilitySystem/V6SkillDocumentIndex_20261018.md"
SUPPORTED = {"ForwardDamage", "SingleProjectile", "RadialDamage", "SingleHealing",
             "SingleShield", "SingleStun", "SingleSilence", "SafeDash"}
EARLY_PROHIBITED = ("五行克制", "相生", "王印结算", "三合阵", "六冲", "共鸣", "破元")


def valid_registry(registry: dict, v6: dict) -> list[str]:
    errors = []
    skills = registry.get("skills") or []
    heroes = v6.get("heroes") or []
    expected = {skill["abilityId"]: (hero["heroId"], skill)
                for hero in heroes for skill in hero["skills"]}
    ids = [skill["abilityId"] for skill in skills]
    if len(skills) != 96 or len(set(ids)) != 96:
        errors.append("96个技能稳定ID不唯一／数量错误")
    if len(heroes) != 12 or len(expected) != 96:
        errors.append("生肖源稿数量或稳定ID错误")
    if set(ids) != set(expected):
        errors.append("实施清单与历史设计稿编号不同")
    if registry.get("serverApprovedAbilityCount") != 0 or registry.get("nativeUEAssetCount") != 0:
        errors.append("尚未制作的原生技能不能声称已上线")
    # 候选数据的移动距离大于射程时必须在V6草案单独说明修正，不能悄悄覆盖V2。
    skills_by_id = {entry["abilityId"]: entry for entry in skills}
    conflicting_dash_ids = {
        entry["abilityId"] for entry in skills
        if entry["proposedPrimitiveCandidate"] == "SafeDash" and
           len(entry.get("v2Actions", [])) == 1 and
           entry["v2Actions"][0].get("kind") == "movement" and
           float(entry["v2Actions"][0].get("distanceCm", 0.0)) >
           float(entry["level1Candidate"]["castRangeCm"])
    }
    corrections = registry.get("v6DraftBalanceCorrections", [])
    if len(corrections) != len(conflicting_dash_ids) or (
            {entry.get("abilityId") for entry in corrections} != conflicting_dash_ids):
        errors.append("V6位移草案纠错项与真实不一致的输入距离集合不匹配")
    for correction in corrections:
        entry = skills_by_id.get(correction.get("abilityId"))
        if not entry:
            errors.append("纠错引用了不存在的技能ID")
            continue
        actions = entry.get("v2Actions", [])
        if (correction.get("status") != "PENDING_DESIGN_REVIEW" or
            correction.get("approvedForRuntime") is not False or
            not actions or
            float(correction["sourceCandidateCastRangeCm"]) !=
                float(entry["level1Candidate"]["castRangeCm"]) or
            float(correction["actionTravelDistanceCm"]) !=
                float(actions[0].get("distanceCm", 0)) or
            float(correction["proposedCastRangeCm"]) <
                float(correction["actionTravelDistanceCm"]) or
            float(correction["proposedCastRangeCm"]) > 950.0):
            errors.append("V6移动纠错与数值或人工待审门禁不一致")
    for entry in skills:
        identity = entry["abilityId"]
        if identity not in expected:
            continue
        hero_id, design = expected[identity]
        if entry["heroId"] != hero_id or entry["displayOrder"] != design["displayOrder"] or (
            entry["displayNameV6"] != design["proposedNameZh"]):
            errors.append(identity + ": 名称/英雄/槽位与V6源不一致")
        if entry["proposedPrimitiveCandidate"] not in SUPPORTED | {"Unsupported"}:
            errors.append(identity + ": 使用未知原生技能模式")
        if entry["canGrantToServer"] or entry["nativeDefinitionAssetPath"] is not None or (
            entry["stableGAClassPath"] is not None) or entry["reviewState"] != "PENDING_USER_REVIEW":
            errors.append(identity + ": 伪造已批准或原生资产")
        if entry["vfxState"] != "NOT_APPROVED":
            errors.append(identity + ": 效果图未经审核却被当成正式资源")
        if any(term in design["descriptionZh"] for term in EARLY_PROHIBITED):
            errors.append(identity + ": 恢复已废止的战斗规则")
    return errors


class V6GASImplementationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.registry = json.loads(REG.read_text(encoding="utf-8-sig"))
        cls.v6 = json.loads(V6.read_text(encoding="utf-8-sig"))
        cls.header = (SRC / "Public/Abilities/DivineBeastsV6GameplayAbility.h").read_text(encoding="utf-8-sig")
        cls.code = (SRC / "Private/Abilities/DivineBeastsV6GameplayAbility.cpp").read_text(encoding="utf-8-sig")
        cls.loadout = (SRC / "Private/Components/DivineBeastsAbilityLoadoutComponent.cpp").read_text(encoding="utf-8-sig")
        cls.definition = (SRC / "Public/Definitions/DivineBeastsAbilityDefinition.h").read_text(encoding="utf-8-sig")

    def test_01_registry_matches_96_user_review_skills(self):
        self.assertEqual(valid_registry(self.registry, self.v6), [])

    def test_02_fails_closed_if_approval_spoofed(self):
        altered = copy.deepcopy(self.registry)
        altered["skills"][0]["canGrantToServer"] = True
        self.assertNotEqual(valid_registry(altered, self.v6), [])

    def test_03_fails_closed_if_ability_id_changed(self):
        altered = copy.deepcopy(self.registry)
        altered["skills"][0]["abilityId"] = altered["skills"][1]["abilityId"]
        self.assertNotEqual(valid_registry(altered, self.v6), [])

    def test_04_native_modes_exist_but_unsupported_defaults(self):
        for mode in SUPPORTED:
            self.assertIn("    " + mode, self.header)
            self.assertIn("EDivineBeastsV6NativeMode::" + mode, self.code)
        self.assertIn("ExecutionMode = EDivineBeastsV6NativeMode::Unsupported", self.header)
        self.assertIn("Definition.DraftActions.Num() != (bDamageControlMode ? 2 : 1)", self.code)
        self.assertIn("ExecutionMode == EDivineBeastsV6NativeMode::Unsupported", self.code)
        self.assertIn("Action.HitCount == 1", self.code)
        self.assertIn("bSingleHit = Action.HitCount == 1", self.code)
        self.assertIn("ControlAction.ActionKindId == TEXT(\"control\")", self.code)
        self.assertIn("ControlAction.HitCount == 1", self.code)

    def test_05_authority_commit_and_avatar_generation(self):
        for token in ("NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly",
                      "CommitAbility(Handle, ActorInfo, ActivationInfo)",
                      "ActivationToken == ExpectedToken",
                      "Combat->GetCombatAvatarGeneration()",
                      "GetAvatarActorFromActorInfo() == ExpectedAvatar",
                      "bIsTearingDown", "AuthorityIsFriendlyCombatTarget"):
            self.assertIn(token, self.code)
        self.assertNotIn("UFUNCTION(Server", self.header)
        # C++中文说明可提到Niagara，但源码不能直接链接客户端特效系统。
        self.assertNotIn('#include "Niagara', self.code)
        self.assertNotIn("UNiagara", self.code)
        self.assertNotIn("ApplyGameplayEffectSpecToTarget", self.code)

    def test_06_native_platform_combat_only(self):
        for token in ("Combat->ApplyDamage(Spec)", "Combat->ApplyHealing(Spec)",
                      "Combat->ApplyShield(Spec", "Combat->ApplyControl(Spec",
                      "AGamePlatformCombatProjectile", "AuthorityInitialize(Shot",
                      "PrepareV6SafeDash", "ProjectPointToNavigation",
                      "SweepSingleByChannel"):
            self.assertIn(token, self.code)

    def test_07_grant_requires_strict_native_approval(self):
        for token in ("Definition->bEightSlotDraftOnly",
                      "Definition->bV6NativeApproved", "ValidateNativeContract(*Definition, Balance",
                      "V6批准资产必须绑定正式V6执行器"):
            self.assertIn(token, self.loadout)
        self.assertIn("bV6NativeApproved = false", self.definition)

    def test_08_multihit_and_mixed_behaviour_do_not_silently_activate(self):
        self.assertIn("Definition.DraftActions.Num() != (bDamageControlMode ? 2 : 1)", self.code)
        self.assertIn("bHasLegalRange", self.code)
        self.assertIn("MaxV6AffectedActors = 16", self.code)
        self.assertIn("Action.Magnitude <= MaxV6ShieldFraction", self.code)
        self.assertIn("Action.DurationSeconds <= MaxV6ControlSeconds", self.code)
        self.assertIn("GetSelectedProjectileTargetIntent", self.code)

    def test_09_stable_identity_and_user_approval_not_rewritten(self):
        approved = json.loads((ROOT / "Docs/Design/ApprovedSkillDisplayNames_V5.json").read_text(encoding="utf-8-sig"))
        self.assertEqual(len(approved["entries"]), 96)
        approved_ids = {a[0] for a in approved["entries"]}
        self.assertEqual(approved_ids, {x["abilityId"] for x in self.registry["skills"]})
        self.assertEqual(self.registry["reviewedV6Skills"], 0)

    def test_10a_snake_draft_candidate_correction_is_not_production(self):
        fixes = self.registry.get("v6DraftBalanceCorrections", [])
        self.assertEqual(len(fixes), 1)
        record = fixes[0]
        self.assertEqual(record["abilityId"], "dba.ability.snake_eight_sinuous_dash@1")
        self.assertEqual(record["sourceCandidateCastRangeCm"], 670)
        self.assertEqual(record["actionTravelDistanceCm"], 680)
        self.assertEqual(record["proposedCastRangeCm"], 680)
        self.assertEqual(record["status"], "PENDING_DESIGN_REVIEW")
        self.assertFalse(record["approvedForRuntime"])
        writer = (ROOT / "Tools/Unreal/Abilities/AuthorV6SkillDraftAssetsUE58.py"
                  ).read_text(encoding="utf-8-sig")
        self.assertIn("pending_fixes.get(skill", writer)
        self.assertIn('"CastRangeCm": draft_range_cm', writer)
        ast.parse(writer, filename="AuthorV6SkillDraftAssetsUE58.py")

    def test_10b_reject_unapproved_snake_range_smaller_than_move(self):
        altered = copy.deepcopy(self.registry)
        altered["v6DraftBalanceCorrections"][0]["proposedCastRangeCm"] = 650
        self.assertNotEqual(valid_registry(altered, self.v6), [])

    def test_10c_reject_claiming_snake_range_is_live_approved(self):
        altered = copy.deepcopy(self.registry)
        altered["v6DraftBalanceCorrections"][0]["approvedForRuntime"] = True
        self.assertNotEqual(valid_registry(altered, self.v6), [])

    def test_10_docs_index_is_present(self):
        self.assertTrue(DOCUMENT.is_file())
        self.assertTrue(INDEX.is_file())
        self.assertIn("V6", DOCUMENT.read_text(encoding="utf-8-sig"))
        self.assertIn("V6SkillImplementationRegistry_20261018.json", INDEX.read_text(encoding="utf-8-sig"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
