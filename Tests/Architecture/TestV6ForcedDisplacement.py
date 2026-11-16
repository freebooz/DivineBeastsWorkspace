#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V6推离/牵引静态TDD门禁。测试原生服务器合同，不替代UE编译及多人运动验收。"""
import copy
import json
import re
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
PLATFORM=ROOT/"Game/Plugins/GamePlatform/Gameplay/GamePlatformCombat/Source/GamePlatformCombat"
V6=ROOT/"Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime"
INDEX=ROOT/"Docs/Implementation/GamePlatformAbilitySystem/V6ControlSkillImplementationMatrix_20261018.json"


def checks(header, cpp, native, v6h, v6c):
    return {
        "platform_server_only": all(s in header+cpp for s in (
            "ApplyForcedDisplacement(", "BuildDisplacementVelocity(",
            "HasAuthority()", "ValidateSpec(Spec, false, false)",
            "IsMovingOnGround()", "Launch(LaunchVelocity)",
            "ForceNetUpdate()", "EGamePlatformCombatEventType::DisplacementQueued")),
        "bounded_push_pull": all(s in cpp for s in (
            "FMath::IsFinite(HorizontalSpeedCmPerSecond)",
            "HorizontalSpeedCmPerSecond > MaxDisplacementSpeedCmPerSecond",
            "FMath::Abs(TargetPosition.Z - Epicenter.Z)",
            "Direction *= -1.0f",
            "FVector::DistSquared2D(",
            "EGamePlatformDisplacementMode::Pull")),
        "authority_target_and_avatar": all(s in cpp for s in (
            "Spec.Source == Spec.Target", "Spec.Target->HasAuthority()",
            "TargetCombat->GetCombatAvatarGeneration() != ExpectedTargetAvatar",
            "GetCombatAvatarGeneration() != ExpectedSourceAvatar",
            "IsValid(Movement)", "Movement->IsRegistered()",
            "IsValid(Capsule)", "Spec.Target->GetWorld() != GetWorld()",
        )),
        "cannot_phase_through_immediate_wall": all(s in cpp for s in (
            "SweepSingleByChannel(", "FCollisionShape::MakeCapsule(",
            "BlockingHit.bBlockingHit", "ECC_Pawn",
        )),
        "unique_queued_event_no_false_landing_claim": all(s in cpp for s in (
            "MarkEventCompleted(Spec.EventId)", "DisplacementQueued",
            "Result_DisplacementQueued", "排队速度不是最终位移距离")),
        "v6_requires_two_actions": all(s in v6c for s in (
            "bDamageControlMode ? 2 : 1",
            "IsValidDamageDisplacementAction",
            'DisplacementAction.ActionKindId == TEXT("displacement")',
            'DisplacementAction.ModifierId == TEXT("push")',
            'DisplacementAction.ModifierId == TEXT("pull")',
        )),
        "v6_damage_first_target_recheck": all(s in v6c for s in (
            "const FGamePlatformCombatResult DamageResult = Combat->ApplyDamage(Spec)",
            "DamageResult.IsSuccess()", "IsCurrentActivation(Token, Source, Combat, SourceGeneration)",
            "IsCurrentTarget(Source, Hero, Target, Snapshot.AvatarGeneration, false)",
            "Combat->ApplyForcedDisplacement(",
            "ControlSpec.EventId = FGuid::NewGuid()",
        )),
        "v6_not_preapproved": "ExecutionMode = EDivineBeastsV6NativeMode::Unsupported" in v6h,
        "native_positive_negative": all(s in native for s in (
            "BuildDisplacementVelocity(", "Displacement.V6.Geometry",
            "不移动Actor的纯方向数值测试", "归零中心", "超出速度上限")),
    }


class ForcedDisplacementSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        def load(r): return r.read_text(encoding="utf-8-sig")
        cls.h=load(PLATFORM/"Public/Components/GamePlatformCombatComponent.h")
        cls.c=load(PLATFORM/"Private/Components/GamePlatformCombatComponent.cpp")
        cls.gh=load(PLATFORM/"Public/Types/GamePlatformCombatTypes.h")
        cls.nt=load(PLATFORM/"Private/Tests/GamePlatformCombatDisplacementTests.cpp")
        cls.v6h=load(V6/"Public/Abilities/DivineBeastsV6GameplayAbility.h")
        cls.v6c=load(V6/"Private/Abilities/DivineBeastsV6GameplayAbility.cpp")
        cls.matrix=json.loads(load(INDEX))

    def test_01_complete_pipeline(self):
        verdict=checks(self.h,self.c,self.nt,self.v6h,self.v6c)
        self.assertTrue(all(verdict.values()),verdict)

    def test_02_enum_append_only(self):
        block=self.gh.split("enum class EGamePlatformCombatEventType")[1].split("};")[0]
        self.assertTrue(block.index("RespawnReset")<block.index("DisplacementQueued"))
        modes=self.gh.split("enum class EGamePlatformDisplacementMode")[1].split("};")[0]
        self.assertIn("PushAway",modes)
        self.assertIn("PullToward",modes)

    def test_03_old_ga_mode_ordinals_preserved(self):
        text=self.v6h.split("enum class EDivineBeastsV6NativeMode")[1].split("};")[0]
        all_modes=["Unsupported","ForwardDamage","SingleProjectile","RadialDamage",
                   "SingleHealing","SingleShield","SingleStun","SingleSilence","SafeDash",
                   "DamageAndStun","DamageAndSilence","RadialDamageAndStun","DamageAndSlow",
                   "DamageAndPush","RadialDamageAndPush","RadialDamageAndPull"]
        positions=[text.index("\n    "+v+"\n") if "\n    "+v+"\n" in text else text.index("\n    "+v+",") for v in all_modes]
        self.assertEqual(positions,sorted(positions))

    def test_04_cannot_drop_pawn_authority(self):
        altered=self.c.replace("Spec.Target->HasAuthority()","true")
        self.assertFalse(checks(self.h,altered,self.nt,self.v6h,self.v6c)["authority_target_and_avatar"])

    def test_05_cannot_skip_blocking_sweep(self):
        altered=self.c.replace("BlockingHit.bBlockingHit","false")
        self.assertFalse(checks(self.h,altered,self.nt,self.v6h,self.v6c)["cannot_phase_through_immediate_wall"])

    def test_06_damage_must_precede_control(self):
        altered=self.v6c.replace("DamageResult.IsSuccess()","false")
        self.assertFalse(checks(self.h,self.c,self.nt,self.v6h,altered)["v6_damage_first_target_recheck"])

    def test_07_approval_remains_off(self):
        self.assertEqual(self.matrix["approvedV6ControlSkills"],0)
        self.assertTrue(all(x["canGrantToServer"] is False for x in self.matrix["items"]))
        self.assertTrue(all(x["nativeGameplayAbilityClassPath"] is None for x in self.matrix["items"]))


    def test_07b_historical_push_pull_never_promoted_without_real_rules(self):
        review=self.matrix["forcedDisplacementReview"]
        self.assertEqual(review["approvedForServerGrants"],0)
        entries=review["candidates"]
        self.assertEqual(len(entries),4)
        expected={
            "dba.ability.ox_eight_seismic_stun@1",
            "dba.ability.dragon_eight_thunder_cloud_lock@1",
            "dba.ability.horse_eight_hoof_stun@1",
            "dba.ability.boar_eight_roar_stun@1"
        }
        self.assertEqual({x["abilityId"] for x in entries},expected)
        self.assertTrue(all(x["approved"] is False for x in entries))
        self.assertTrue(all(x["missingMechanicsZh"] for x in entries))
        dragon=next(x for x in entries if x["nameZh"]=="龙压")
        self.assertEqual(set(dragon["modesSuggested"]),
                         {"RadialDamageAndPush","RadialDamageAndPull"})

    def test_08_displacement_is_not_conflated_with_stun(self):
        self.assertNotIn("EGamePlatformControlType::PushAway",self.c)
        self.assertNotIn("EGamePlatformControlType::PullToward",self.c)
        self.assertIn("EGamePlatformDisplacementMode::PullToward",self.c)


if __name__=="__main__":
    unittest.main(verbosity=2)
