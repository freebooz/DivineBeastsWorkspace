#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V6 P0：GAS冷却/提交后命中/范围中心/冲刺终态防回归。

只检查当前工作树真实C++源代码的失败关闭合同；不编译、不载入UE、不验证多人实战。
通过故障注入验证删去关键限制后不能继续通过。
"""
from __future__ import annotations
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GAMEPLAY = (ROOT / "Game/Plugins/DivineBeasts/DBAGameplay/Source/"
            "DivineBeastsAbilitiesRuntime/Private")
LOADOUT = GAMEPLAY / "Components/DivineBeastsAbilityLoadoutComponent.cpp"
V6 = GAMEPLAY / "Abilities/DivineBeastsV6GameplayAbility.cpp"


def contracts(grants: str, ability: str) -> dict[str, bool]:
    # 必须针对发射体的Commit之后分支判定，不能被同文件其它Actor朝向代码误满足。
    projectile_body = ability.split("AActor* FrozenTarget = SingleTarget.Actor.Get();")[-1]
    result = {
        "cooldown_effect_actual_granted_tags": all(x in grants for x in (
            "CooldownEffect->GetGrantedTags().HasAllExact(*CooldownTags)",
            "CooldownTags->HasAllExact(CooldownEffect->GetGrantedTags())",
            "V6Ability",
        )),
        "post_commit_cast_distance": all(x in ability for x in (
            "IsV6WithinRange(Source, Target, Balance.CastRangeCm)",
            "if (!IsCurrentActivation(Token, Source, Combat, SourceGeneration)",
            "if (!Result.IsSuccess()) return false",
        )),
        "post_commit_area_center_frozen": all(x in ability for x in (
            "FVector FrozenAreaCenter",
            "BuildV6AreaTargets(Source, Hero, Combat, Balance, AreaTargets, FrozenAreaCenter)",
            "FVector::DistSquared(Target->GetActorLocation(), FrozenAreaCenter)",
            "FMath::Square(Balance.AreaRadiusCm)",
        )),
        "post_commit_area_center_cast_range": all(x in ability for x in (
            "IsV6AreaCenterValid(Source, FrozenAreaCenter, Balance.CastRangeCm)",
            "Balance.CastRangeCm",
        )),
        "post_commit_retrace_first_forward_target": all(x in ability for x in (
            "TraceV6ForwardEnemy(Source, Hero, Balance.CastRangeCm, UpdatedHit)",
            "UpdatedHit.Actor.Get() != Target",
        )),
        "post_commit_fresh_hitpoint": all(x in ability for x in (
            "GetCombatTargetPoint()",
            "Actual.ImpactLocation",
            "MakeV6Spec(Source, Combat, Actual, Balance, Magnitude)",
        )),
        "area_partial_success_not_rolled_back": all(x in ability for x in (
            "bool bAnyApplied = false",
            "if (ApplyToTarget(Target))",
            "bAnyApplied = true",
            "bSuccess = bAnyApplied",
        )) and "if (!ApplyToTarget(Target)) { bSuccess = false; break; }" not in ability,
        "projectile_frozen_target_never_becomes_unowned_shot": all(x in ability for x in (
            "bool bProjectileHadFrozenTarget = false",
            "bProjectileHadFrozenTarget = bHasIntent",
            "const bool bCurrentFrozenTarget = bProjectileHadFrozenTarget",
            "(!bProjectileHadFrozenTarget ||",
            "bCurrentFrozenTarget && IsValid(TargetCombat)",
        )),
        "projectile_aim_refresh_after_commit": all(x in projectile_body for x in (
            "TargetCombat->GetCombatTargetPoint()",
            "const FVector FreshAim",
            "const FVector Direction = Source->GetActorForwardVector().GetSafeNormal()",
            "IsV6WithinRange(Source, FrozenTarget, Balance.CastRangeCm)",
            "HasV6LineOfSight(Source->GetWorld(), Source, FrozenTarget)",
        )),
        "dash_partial_movement_reported_truthfully": all(x in ability for x in (
            "const FVector DashStart = Hero->GetActorLocation()",
            "const bool bMoved = Hero->SetActorLocation(",
            "FVector::DistSquared(Hero->GetActorLocation(), DashStart)",
            "bSuccess = true",
        )) and "!MoveHit.bBlockingHit)" not in ability,
    }
    return result


class V6CombatSafetySourceRegression(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.grants = LOADOUT.read_text(encoding="utf-8-sig")
        cls.ability = V6.read_text(encoding="utf-8-sig")

    def test_current_source_contracts(self):
        found = contracts(self.grants, self.ability)
        self.assertTrue(all(found.values()), found)

    def test_removing_granted_cooldown_tag_evidence_fails(self):
        altered = self.grants.replace(
            "CooldownEffect->GetGrantedTags().HasAllExact(*CooldownTags)", "true", 1)
        self.assertFalse(contracts(altered, self.ability)["cooldown_effect_actual_granted_tags"])

    def test_removing_post_commit_range_fails(self):
        altered = self.ability.replace(
            "IsV6WithinRange(Source, Target, Balance.CastRangeCm)", "true")
        self.assertFalse(contracts(self.grants, altered)["post_commit_cast_distance"])

    def test_removing_frozen_center_fails(self):
        altered = self.ability.replace(
            "FVector::DistSquared(Target->GetActorLocation(), FrozenAreaCenter)",
            "0.0f")
        self.assertFalse(contracts(self.grants, altered)["post_commit_area_center_frozen"])

    def test_removing_area_cast_range_fails(self):
        altered = self.ability.replace(
            "IsV6AreaCenterValid(Source, FrozenAreaCenter, Balance.CastRangeCm)",
            "true")
        self.assertFalse(contracts(self.grants, altered)["post_commit_area_center_cast_range"])

    def test_removing_forward_retrace_fails(self):
        altered = self.ability.replace(
            "UpdatedHit.Actor.Get() != Target", "false", 1)
        self.assertFalse(contracts(self.grants, altered)["post_commit_retrace_first_forward_target"])

    def test_removing_fresh_impact_fails(self):
        altered = self.ability.replace(
            "MakeV6Spec(Source, Combat, Actual, Balance, Magnitude)",
            "MakeV6Spec(Source, Combat, Snapshot, Balance, Magnitude)", 1)
        self.assertFalse(contracts(self.grants, altered)["post_commit_fresh_hitpoint"])

    def test_area_partial_damage_stays_truthful_when_another_target_expires(self):
        altered = self.ability.replace(
            "bSuccess = bAnyApplied;",
            "bSuccess = false;", 1)
        self.assertFalse(contracts(self.grants, altered)["area_partial_success_not_rolled_back"])

    def test_destroyed_selected_projectile_target_never_downgrades_to_free_shot(self):
        altered = self.ability.replace(
            "(!bProjectileHadFrozenTarget ||",
            "(!IsValid(FrozenTarget) ||", 1)
        self.assertFalse(contracts(self.grants, altered)[
            "projectile_frozen_target_never_becomes_unowned_shot"])

    def test_projectile_never_uses_old_precommit_position_as_launch_direction(self):
        before, sep, projectile = self.ability.partition(
            "AActor* FrozenTarget = SingleTarget.Actor.Get();")
        self.assertTrue(sep)
        altered = before + sep + projectile.replace(
            "const FVector Direction = Source->GetActorForwardVector().GetSafeNormal()",
            "const FVector Direction = (SingleTarget.ImpactLocation - Origin).GetSafeNormal()", 1)
        self.assertFalse(contracts(self.grants, altered)[
            "projectile_aim_refresh_after_commit"])

    def test_projectile_homing_uses_current_target_center_each_server_substep(self):
        platform = (ROOT / "Game/Plugins/GamePlatform/Gameplay/GamePlatformCombat/"
                    "Source/GamePlatformCombat/Private/Actors/GamePlatformCombatProjectile.cpp"
                    ).read_text(encoding="utf-8-sig")
        for token in ("HasCurrentHomingTarget()", "GetCombatTargetPoint()",
                      "AdvanceHomingPosition(Step, TargetCenter, TurnRate)",
                      "SweepSingleByChannel(", "SweepSingleByObjectType("):
            self.assertIn(token, platform)

    def test_reintroducing_partial_dash_failure_fails(self):
        altered = self.ability.replace(
            "const bool bMoved = Hero->SetActorLocation(",
            "const bool bMoved = !MoveHit.bBlockingHit) && Hero->SetActorLocation(", 1)
        self.assertFalse(contracts(self.grants, altered)["dash_partial_movement_reported_truthfully"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
