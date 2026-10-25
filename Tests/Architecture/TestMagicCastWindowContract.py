#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""火球/冰箭服务器起手与复制反馈的源码回归，不代替UE原生GAS时序验收。

检查两条真实执行路径都将同一个0.5秒窗口传给Cast事实与首次释放Timer，
并通过故障注入证明立即发射、反馈分叉、取消清理缺失都会被拒绝。
只读取源码，不启动编辑器、不修改资产或运行中的客户端/服务器。
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime"


def cast_window_contract(definition: str, ability: str) -> dict[str, bool]:
    """限定Activate到Release之间，避免其它Timer或注释误满足真实首次释放门禁。"""
    body = ability.split("::ActivateAbility(", 1)[1].split("::ReleaseNextProjectile()", 1)[0]
    compact = re.sub(r"\s+", "", body)
    release = ability.split("::ReleaseNextProjectile()", 1)[1].split("::EndAbility(", 1)[0]
    end = ability.split("::EndAbility(", 1)[1]
    return {
        "shared_half_second": bool(re.search(
            r"DefaultCastDurationSeconds\s*=\s*0\.5f\s*;", definition)),
        "both_fire_and_ice_delayed": all(token in compact for token in (
            "::Fireball||", "::Frostbolt", "constfloatCastDurationSeconds=",
            "UDivineBeastsDevelopmentMagicDefinition::DefaultCastDurationSeconds",
            "if(CastDurationSeconds>0.0f)",
            "CastDurationSeconds,false);return;}ReleaseNextProjectile();")),
        "facts_use_same_duration": bool(re.search(
            r"PublishConfirmedCast\([^;]*CastDurationSeconds\s*>\s*0\.0f\s*\?\s*CastDurationSeconds",
            body, re.S)),
        "late_source_rejected_before_spawn": (
            "GetAvatarActorFromActorInfo()" in release.split("SpawnActor<", 1)[0]
            and "IsCombatDead()" in release.split("SpawnActor<", 1)[0]
            and "bIsTearingDown" in release.split("SpawnActor<", 1)[0]
            and "GetCombatAvatarGeneration()" in release.split("SpawnActor<", 1)[0]),
        "cancel_clears_owned_timer": "ClearTimer(BurstTimer)" in end,
    }


class MagicCastWindowContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.definition = (MODULE / "Public/Definitions/DivineBeastsDevelopmentMagicDefinition.h").read_text(encoding="utf-8-sig")
        cls.abilities = [(MODULE / ("Private/Abilities/" + name + ".cpp")).read_text(encoding="utf-8-sig")
                         for name in ("DivineBeastsDevelopmentMagicAbility", "DivineBeastsPlaceholderGameplayAbility")]

    def test_real_source_uses_one_half_second_authoritative_window(self):
        for ability in self.abilities:
            with self.subTest(path=ability.splitlines()[0]):
                actual = cast_window_contract(self.definition, ability)
                self.assertTrue(all(actual.values()), actual)

    def test_feedback_only_fireball_change_is_rejected(self):
        for ability in self.abilities:
            changed = ability.replace("::Fireball ||", "::Fireball &&")
            self.assertFalse(cast_window_contract(self.definition, changed)["both_fire_and_ice_delayed"])

    def test_different_feedback_duration_is_rejected(self):
        for ability in self.abilities:
            changed = re.sub(r"CastDurationSeconds > 0\.0f \? CastDurationSeconds", "0.7f", ability, count=1)
            self.assertFalse(cast_window_contract(self.definition, changed)["facts_use_same_duration"])

    def test_cancel_without_timer_cleanup_is_rejected(self):
        for ability in self.abilities:
            self.assertFalse(cast_window_contract(self.definition, ability.replace(
                "ClearTimer(BurstTimer)", "ClearTimer(UnownedTimer)"))["cancel_clears_owned_timer"])

    def test_source_avatar_change_must_be_checked_before_spawn(self):
        for ability in self.abilities:
            start = ability.index("::ReleaseNextProjectile()")
            before, after = ability[:start], ability[start:]
            # 只破坏释放入口的Avatar门禁；Spawn返回后的同类检查仍在，不能误记为足够。
            changed = re.sub(r"GetAvatarActorFromActorInfo\(\)\s*!=\s*Source\s*\|\|", "", after, count=1)
            self.assertFalse(cast_window_contract(self.definition, before + changed)["late_source_rejected_before_spawn"])

    def test_non_half_second_shared_default_is_rejected(self):
        changed = self.definition.replace("DefaultCastDurationSeconds = 0.5f", "DefaultCastDurationSeconds = 0.7f")
        for ability in self.abilities:
            self.assertFalse(cast_window_contract(changed, ability)["shared_half_second"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
