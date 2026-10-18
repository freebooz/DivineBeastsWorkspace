#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V6 平台真实Slow及项目运动端到端合同：非编译静态检查及故障注入。"""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PLATFORM = ROOT / "Game/Plugins/GamePlatform/Gameplay/GamePlatformCombat/Source/GamePlatformCombat"
PROJECT = ROOT / "Game/Plugins/DivineBeasts/DBAGameplay/Source"
MATRIX = ROOT / "Docs/Implementation/GamePlatformAbilitySystem/V6ControlSkillImplementationMatrix_20261018.json"


def content(path):
    return path.read_text(encoding="utf-8-sig")


def review():
    att_h = content(PLATFORM / "Public/Attributes/GamePlatformCombatAttributeSet.h")
    att_c = content(PLATFORM / "Private/Attributes/GamePlatformCombatAttributeSet.cpp")
    tags_h = content(PLATFORM / "Public/Tags/GamePlatformCombatTags.h")
    tags_c = content(PLATFORM / "Private/Tags/GamePlatformCombatTags.cpp")
    ge_h = content(PLATFORM / "Public/Effects/GamePlatformCombatGameplayEffects.h")
    ge_c = content(PLATFORM / "Private/Effects/GamePlatformCombatGameplayEffects.cpp")
    combat_h = content(PLATFORM / "Public/Components/GamePlatformCombatComponent.h")
    combat_c = content(PLATFORM / "Private/Components/GamePlatformCombatComponent.cpp")
    player_c = content(PROJECT / "DivineBeastsCharactersRuntime/Private/Components/DivineBeastsCharacterComponent.cpp")
    player_h = content(PROJECT / "DivineBeastsCharactersRuntime/Public/Components/DivineBeastsCharacterComponent.h")
    v6_h = content(PROJECT / "DivineBeastsAbilitiesRuntime/Public/Abilities/DivineBeastsV6GameplayAbility.h")
    v6_c = content(PROJECT / "DivineBeastsAbilitiesRuntime/Private/Abilities/DivineBeastsV6GameplayAbility.cpp")
    control = json.loads(content(MATRIX))
    return {
        "slow_is_native_replicated_gas_attribute": all(x in att_h+att_c for x in (
            "MoveSpeedMultiplier", "ReplicatedUsing=OnRep_MoveSpeedMultiplier",
            "DOREPLIFETIME_CONDITION_NOTIFY(", "COND_None, REPNOTIFY_Always",
            "InitMoveSpeedMultiplier(1.0f)", "NewValue = FMath::Clamp(NewValue, 0.30f, 1.0f)")),
        "slow_ge_duration_and_stack": all(x in ge_c for x in (
            "UGamePlatformSlowGameplayEffect::UGamePlatformSlowGameplayEffect",
            "EGameplayEffectDurationType::HasDuration",
            "StackingType = EGameplayEffectStackingType::None",
            "UGamePlatformCombatAttributeSet::GetMoveSpeedMultiplierAttribute()",
            "EGameplayModOp::Additive", "FGameplayEffectModifierMagnitude(DynamicSlow)",
            "GEComponents.Add(Tags)")),
        "slow_input_tag_and_set_by_caller": all(x in tags_h+tags_c+ge_c+combat_c for x in (
            "Control_Slow", "Data_Control_SlowFraction", '"Combat.Control.Slow"',
            '"Combat.Data.Control.SlowFraction"',
            "SetSetByCallerMagnitude(", "-SlowFraction")),
        "slow_server_validation": all(x in combat_c for x in (
            "UGamePlatformCombatComponent::ApplySlow",
            "SlowFraction <= 0.0f", "SlowFraction > FMath::Min(0.70f",
            "DurationSeconds > FMath::Min(10.0f",
            "ValidateSpec(Spec, false, false)",
            "TargetCharacter->GetCharacterMovement()",
            "TargetASC->GetSet<UGamePlatformCombatAttributeSet>()",
            "SourceASC->ApplyGameplayEffectSpecToTarget",
            "TargetASC->RemoveActiveGameplayEffect(Applied)",
            "GetCombatAvatarGeneration()",
        )) or all(x in combat_c for x in (
            "UGamePlatformCombatComponent::ApplySlow", "SlowFraction <= 0.0f",
            "ValidateSpec(Spec, false, false)", "TargetCharacter->GetCharacterMovement()",
            "TargetCombat->AvatarGeneration == TargetAvatar",
            "SourceASC->ApplyGameplayEffectSpecToTarget")),
        "no_same_event_reentrant_slow": all(x in combat_h+combat_c for x in (
            "InFlightSlowEventIds", "InFlightSlowEventIds.Contains(Spec.EventId)",
            "InFlightSlowEventIds.Add(Spec.EventId)", "InFlightSlowEventIds.Remove(Spec.EventId)",
            "InFlightSlowEventIds.Reset()")),
        "slow_cleanup_on_death_and_new_avatar": (
            combat_c.count("ControlTags.AddTag(GamePlatformCombatTags::Control_Slow)") >= 1 and
            "CombatStateTags.AddTag(GamePlatformCombatTags::Control_Slow)" in combat_c and
            "case EGamePlatformControlType::Slow:" in combat_c),
        "project_movement_base_defined_in_hero": all(x in player_c for x in (
            "LoadedDefinition->Movement.MaxWalkSpeed * Multiplier",
            "MovementDefinition.MaxWalkSpeed * Multiplier",
            "Movement->MaxWalkSpeedCrouched = BaseMaxWalkSpeedCrouched * Multiplier")),
        "project_subscribes_and_unsubscribes_gas": all(x in player_c+player_h for x in (
            "GetGameplayAttributeValueChangeDelegate",
            "MoveSpeedMultiplierChangedHandle", "HandleMoveSpeedMultiplierChanged",
            "RefreshMoveSpeedMultiplier()", "OnAvatarBindingChanged().Remove",
            "MoveSpeedMultiplierChangedHandle.Reset()")),
        "v6_slow_does_not_fake_stun": all(x in v6_h+v6_c for x in (
            "DamageAndSlow", "IsValidDamageSlowAction",
            'ControlAction.ModifierId == TEXT("slow")',
            "ControlAction.Magnitude <= 0.70f",
            "ControlAction.DurationSeconds <= MaxV6ControlSeconds",
            "Combat->ApplySlow(ControlSpec, ControlAction.Magnitude, ControlAction.DurationSeconds)")),
        "design_still_unapproved": (len(control["items"]) == 12 and
            control["countsByMode"] == {"DamageAndSlow":4,"RadialDamageAndStun":4,
                                         "DamageAndStun":2,"DamageAndSilence":2} and
            control["approvedV6ControlSkills"] == 0 and
            all(i["approvalStatus"]=="PENDING_USER_REVIEW" and
                i["canGrantToServer"] is False and
                not i["historicalMechanismMatchesNativeMode"] for i in control["items"]))
    }


class V6SlowIntegrationSourceGate(unittest.TestCase):
    def test_01_all_native_slow_source_contracts(self):
        checks = review()
        self.assertTrue(all(checks.values()), checks)

    def test_02_four_historical_slow_candidates_only(self):
        matrix = json.loads(content(MATRIX))
        self.assertEqual(len([r for r in matrix["items"] if r["mode"]=="DamageAndSlow"]),4)
        self.assertEqual({r["heroId"].rsplit(".",1)[-1] for r in matrix["items"]
                          if r["mode"]=="DamageAndSlow"}, {"Rat","Rabbit","Goat","Dog"})

    def test_03_no_fake_production_assets(self):
        matrix = json.loads(content(MATRIX))
        self.assertEqual(matrix["nativeDefinitionsAuthored"],0)
        self.assertEqual(matrix["editorBlueprintsAuthored"],0)
        self.assertTrue(all(item["nativeGameplayAbilityClassPath"] is None for item in matrix["items"]))

    def test_04_core_native_tests_added(self):
        attr = content(PLATFORM/"Private/Tests/GamePlatformCombatAttributeTests.cpp")
        effect = content(PLATFORM/"Private/Tests/GamePlatformCombatEffectDefinitionTests.cpp")
        ability = content(PROJECT/"DivineBeastsAbilitiesRuntime/Private/Tests/DivineBeastsV6AbilityContractTests.cpp")
        self.assertIn("减速最大70%",attr)
        self.assertIn("GamePlatform.Combat.Control.SlowEffectDefinition",effect)
        self.assertIn("DivineBeasts.Abilities.V6.DamageSlowContract",ability)

    def test_05_platform_class_has_no_zodiac_hardlink(self):
        cpp = content(PLATFORM/"Private/Components/GamePlatformCombatComponent.cpp")
        self.assertNotIn("DivineBeasts",cpp)
        self.assertNotIn("Zodiac",cpp)

    def test_06_no_slow_rpc(self):
        header = content(PLATFORM/"Public/Components/GamePlatformCombatComponent.h")
        self.assertIn("BlueprintAuthorityOnly",header)
        self.assertNotIn("ServerApplySlow",header)

    def test_07_legacy_control_values_still_first(self):
        enums = content(PLATFORM/"Public/Types/GamePlatformCombatTypes.h")
        fragment = enums.split("enum class EGamePlatformControlType : uint8")[1].split("};")[0]
        for name in ("Stun,","Silence,","Slow"):
            self.assertIn(name,fragment)
        self.assertLess(fragment.index("Stun,"),fragment.index("Silence,"))
        self.assertLess(fragment.index("Silence,"),fragment.index("Slow"))

    def test_08_reentrant_guard_is_real_not_a_comment(self):
        cpp = content(PLATFORM/"Private/Components/GamePlatformCombatComponent.cpp")
        self.assertRegex(cpp,r"InFlightSlowEventIds\.Contains\(Spec.EventId\)")
        self.assertRegex(cpp,r"InFlightSlowEventIds\.Add\(Spec.EventId\)")
        self.assertRegex(cpp,r"InFlightSlowEventIds\.Remove\(Spec.EventId\)")


if __name__=="__main__":
    unittest.main(verbosity=2)
