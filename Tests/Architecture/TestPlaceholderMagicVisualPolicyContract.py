#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 占位法术视觉准入反向回归。

仅在内存中破坏源码字符串，确认白名单、Village唯一地图、DedicatedServer排除、
PIE实例后缀以及开发许可任一缺失时，生产级静态门禁会失败。
不启动UE、不创建美术资源、不更改现有真实源码。
"""
from __future__ import annotations
import unittest
import ValidateZodiacAbilityIntegration as audit

VISUAL = audit.read(audit.BASE / "DBAClient/Source/DivineBeastsPresentationClient/Private/"
                    "Adapters/DivineBeastsDevelopmentMagicPresentation.cpp")
POLICY = audit.read(audit.BASE / "DBAClient/Source/DivineBeastsPresentationClient/Private/"
                    "Development/DivineBeastsPlaceholderMagicVisualPolicy.h")
PLACEHOLDER = audit.read(audit.ABILITIES / "Private/Abilities/DivineBeastsPlaceholderGameplayAbility.cpp")


class PlaceholderVisualPolicyContractTests(unittest.TestCase):
    def assert_rejects(self, visual=VISUAL, policy=POLICY, placeholder=PLACEHOLDER, key="village_identity"):
        result = audit.placeholder_visual_contract(visual, policy, placeholder)
        self.assertFalse(result[key], "视觉策略关键拒绝条件被删除却仍通过：" + key)

    def test_current_village_and_development_controls(self):
        self.assertTrue(all(audit.placeholder_visual_contract(VISUAL, POLICY, PLACEHOLDER).values()))

    def test_wrong_village_rejected(self):
        self.assert_rejects(policy=POLICY.replace(
            "/DBAWorldPack_Village/Maps/L_Village_Start",
            "/DBAWorldPack_Village/Maps/L_UnsafeVillage", 1))

    def test_non_numeric_pie_map_rejected(self):
        self.assert_rejects(policy=POLICY.replace(
            "PieTail.Left(PieTail.Len() - MapSuffix.Len()).IsNumeric()",
            "true", 1))

    def test_one_of_twelve_hero_ids_missing_rejected(self):
        self.assert_rejects(policy=POLICY.replace('TEXT("rabbit"),', 'TEXT("rabbitt"),', 1))

    def test_any_suffix_prefix_matching_rejected(self):
        self.assert_rejects(policy=POLICY.replace(
            'Value == FString(TEXT("dba.ability.")) + Hero + Suffix',
            'Value.StartsWith(FString(TEXT("dba.ability.")) + Hero)', 1))

    def test_spell_whitelist_relaxed_rejected(self):
        self.assert_rejects(policy=POLICY.replace(
            'Magic != TEXT("ArcaneMissiles")', 'Magic != TEXT("AnySpell")', 1))

    def test_client_visual_does_not_run_on_dedicated_server(self):
        self.assert_rejects(visual=VISUAL.replace(
            "World->GetNetMode() == NM_DedicatedServer", "false", 1))

    def test_village_world_guard_removed_rejected(self):
        self.assert_rejects(visual=VISUAL.replace(
            "(!bDevelopment && !bPlaceholderVillage)", "false", 1))

    def test_development_gate_removed_rejected(self):
        self.assert_rejects(visual=VISUAL.replace(
            "!UDivineBeastsDevelopmentMagicDefinition::IsDevelopmentEnabledForCurrentProcess() &&",
            "false &&", 1), key="development_gate")

    def test_ability_id_check_removed_rejected(self):
        self.assert_rejects(visual=VISUAL.replace(
            "!DivineBeastsPlaceholderMagicVisualPolicy::IsApprovedPlaceholderSpell(Snapshot.AbilityId, Name)",
            "!true", 1), key="development_gate")

    def test_server_commit_style_removed_rejected(self):
        self.assert_rejects(placeholder=PLACEHOLDER.replace(
            'CastStyle = TEXT("DBA.Development.Frostbolt")',
            'CastStyle = TEXT("DBA.Development.Unknown")', 1))


if __name__ == "__main__":
    unittest.main(verbosity=2)
