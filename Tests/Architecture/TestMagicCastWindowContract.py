#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""火球/冰箭服务器起手与复制反馈的源码回归，不代替UE原生GAS时序验收。

检查两个薄数据适配经唯一公共执行将同一个0.5秒窗口传给Cast事实与首次释放Timer，
并通过故障注入证明立即发射、反馈分叉、取消清理缺失都会被拒绝。
只读取源码，不启动编辑器、不修改资产或运行中的客户端/服务器。
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime"


def function_body(source: str, name: str) -> str:
    """只提取真实项目类函数定义，括号平衡限定范围；缺函数返回空串使门禁失败。

    不把Super调用或注释当成定义；执行重定位后读取公共类，不能要求适配重新持有状态机。
    """
    source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
    match = re.search(r"\b(?:void|bool)\s+UDivineBeasts\w+::" + re.escape(name) + r"\s*\(", source)
    if not match:
        return ""
    start = source.find("{", match.end())
    if start < 0:
        return ""
    depth = 1
    for end in range(start + 1, len(source)):
        depth += (source[end] == "{") - (source[end] == "}")
        if depth == 0:
            return source[start + 1:end]
    return ""


def cast_window_contract(definition: str, execution: str, adapter: str) -> dict[str, bool]:
    """适配只映射窗口，公共Activate/Release/End唯一计时；原有时序与清理条件全部保留。"""
    body = function_body(execution, "ActivateAbility")
    compact = re.sub(r"\s+", "", body)
    parameters = re.sub(r"\s+", "", function_body(adapter, "TryBuildCombatExecution"))
    release = function_body(execution, "ReleaseNextProjectile")
    end = function_body(execution, "EndAbility")
    before_spawn = release.split("SpawnActor<", 1)[0]
    source_gate = re.sub(r"\s+", "", before_spawn)
    return {
        "shared_half_second": bool(re.search(
            r"DefaultCastDurationSeconds\s*=\s*0\.5f\s*;", definition)),
        "both_fire_and_ice_delayed": bool(re.search(
            r"OutExecution\.CastDurationSeconds=[^;]*::Fireball\|\|[^;]*::Frostbolt\?"
            r"UDivineBeastsDevelopmentMagicDefinition::DefaultCastDurationSeconds:0(?:\.0?)?f;",
            parameters)) and all(token in compact for token in (
            "constfloatCastDurationSeconds=Execution.CastDurationSeconds;",
            "if(CastDurationSeconds>0.0f)",
            "CastDurationSeconds,false);return;}ReleaseNextProjectile();")),
        "adapter_parameters_reach_execution":
            "TryBuildCombatExecution(Balance,Execution,Error)" in compact,
        "facts_use_same_duration": bool(re.search(
            r"PublishConfirmedCast\([^;]*CastDurationSeconds\s*>\s*0\.0f\s*\?\s*CastDurationSeconds",
            body, re.S)),
        "late_source_rejected_before_spawn": (
            "SpawnActor<" in release and "GetAvatarActorFromActorInfo()!=Source||" in source_gate
            and bool(re.search(r"\bCombat->IsCombatDead\(\)\|\|", source_gate))
            and "World->bIsTearingDown||" in source_gate
            # 必须是来源Combat与来源代次比较，不能让TargetCombat的同名API冒充来源门禁。
            and bool(re.search(r"\bCombat->GetCombatAvatarGeneration\(\)!="
                               r"CastSpec.SourceAvatarGeneration\|\|", source_gate))),
        "cancel_clears_owned_timer": "ClearTimer(BurstTimer)" in end,
    }


class MagicCastWindowContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.definition = (MODULE / "Public/Definitions/DivineBeastsDevelopmentMagicDefinition.h").read_text(encoding="utf-8-sig")
        cls.execution = (MODULE / "Private/Abilities/DivineBeastsCombatGameplayAbility.cpp").read_text(encoding="utf-8-sig")
        cls.abilities = [(MODULE / ("Private/Abilities/" + name + ".cpp")).read_text(encoding="utf-8-sig")
                         for name in ("DivineBeastsDevelopmentMagicAbility", "DivineBeastsPlaceholderGameplayAbility")]

    def test_real_source_uses_one_half_second_authoritative_window(self):
        for ability in self.abilities:
            with self.subTest(path=ability.splitlines()[0]):
                actual = cast_window_contract(self.definition, self.execution, ability)
                self.assertTrue(all(actual.values()), actual)

    def test_feedback_only_fireball_change_is_rejected(self):
        for ability in self.abilities:
            changed = ability.replace("::Fireball ||", "::Fireball &&")
            self.assertNotEqual(changed, ability, "故障注入必须实际破坏火球/冰箭OR条件")
            self.assertFalse(cast_window_contract(self.definition, self.execution, changed)["both_fire_and_ice_delayed"])

    def test_different_feedback_duration_is_rejected(self):
        changed = re.sub(r"CastDurationSeconds > 0\.0f \? CastDurationSeconds", "0.7f", self.execution, count=1)
        self.assertNotEqual(changed, self.execution, "故障注入必须实际破坏公共事实时长")
        for ability in self.abilities:
            self.assertFalse(cast_window_contract(self.definition, changed, ability)["facts_use_same_duration"])

    def test_cancel_without_timer_cleanup_is_rejected(self):
        changed = self.execution.replace("ClearTimer(BurstTimer)", "ClearTimer(UnownedTimer)")
        self.assertNotEqual(changed, self.execution, "故障注入必须实际破坏自有Timer清理")
        for ability in self.abilities:
            self.assertFalse(cast_window_contract(self.definition, changed, ability)["cancel_clears_owned_timer"])

    def test_source_avatar_change_must_be_checked_before_spawn(self):
        start = self.execution.index("::ReleaseNextProjectile()")
        before, after = self.execution[:start], self.execution[start:]
        # 只破坏释放入口的Avatar门禁；Spawn返回后的同类检查仍在，不能误记为足够。
        changed = re.sub(r"GetAvatarActorFromActorInfo\(\)\s*!=\s*Source\s*\|\|", "", after, count=1)
        self.assertNotEqual(changed, after, "故障注入必须实际移除Spawn前Avatar检查")
        for ability in self.abilities:
            self.assertFalse(cast_window_contract(self.definition, before + changed, ability)["late_source_rejected_before_spawn"])

    def test_non_half_second_shared_default_is_rejected(self):
        changed = self.definition.replace("DefaultCastDurationSeconds = 0.5f", "DefaultCastDurationSeconds = 0.7f")
        self.assertNotEqual(changed, self.definition, "故障注入必须实际破坏共享0.5秒常量")
        for ability in self.abilities:
            self.assertFalse(cast_window_contract(changed, self.execution, ability)["shared_half_second"])

    def test_source_generation_change_must_be_checked_before_spawn(self):
        start = self.execution.index("::ReleaseNextProjectile()")
        before, after = self.execution[:start], self.execution[start:]
        # Spawn返回后仍有代次复核；删除释放前的代次门禁必须被单独识别。
        changed = re.sub(r"Combat->GetCombatAvatarGeneration\(\)\s*!=\s*"
                         r"CastSpec.SourceAvatarGeneration\s*\|\|", "", after, count=1)
        self.assertNotEqual(changed, after, "故障注入必须实际移除Spawn前来源代次检查")
        for ability in self.abilities:
            self.assertFalse(cast_window_contract(self.definition, before + changed, ability)["late_source_rejected_before_spawn"])

    def test_shared_execution_cannot_skip_adapter_snapshot(self):
        changed = self.execution.replace("TryBuildCombatExecution(Balance, Execution, Error)", "true")
        self.assertNotEqual(changed, self.execution, "故障注入必须实际切断适配参数流")
        for ability in self.abilities:
            self.assertFalse(cast_window_contract(self.definition, changed, ability)["adapter_parameters_reach_execution"])

    def test_bypassing_shared_windup_is_rejected(self):
        changed = self.execution.replace("if (CastDurationSeconds > 0.0f)", "if (false)")
        self.assertNotEqual(changed, self.execution, "故障注入必须实际绕过首次释放延迟")
        for ability in self.abilities:
            self.assertFalse(cast_window_contract(self.definition, changed, ability)["both_fire_and_ice_delayed"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
