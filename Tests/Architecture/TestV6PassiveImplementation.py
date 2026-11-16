#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V6固有被动：历史12生肖数据、仅2项真实GAS源码和审查隔离的非编译门禁。

注意：代码静态检查不代表UE编译、GE实际Stack行为、服务器多人测试或资产已存在。
"""
from __future__ import annotations
import copy
import json
import re
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
MODULE=ROOT/"Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime"
REG=ROOT/"Docs/Implementation/GamePlatformAbilitySystem/V6PassiveImplementationMatrix_20261018.json"
V6=ROOT/"Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json"

def read(path):
    return path.read_text(encoding="utf-8-sig")

def validate_registry(reg, design):
    problems=[]
    items=reg.get("passives",[])
    source={h["heroId"]:(h["earlyCharacterFullNameZh"],h["passive"]["nameZh"]) for h in design["heroes"]}
    if len(source)!=12 or len(items)!=12 or len({x["heroId"] for x in items})!=12:
        problems.append("必须准确对齐12生肖及12固有被动")
    if reg["serverApprovedPassives"]!=0 or reg["nativePassiveDataAssets"]!=0 or reg["nativeGameplayAbilityAssets"]!=0:
        problems.append("不能伪造已经批准的被动及原生资产")
    for x in items:
        hero=source.get(x["heroId"])
        if not hero or hero!=(x["heroNameZh"],x["passiveNameZh"]):
            problems.append("历史名称或角色身份不匹配:"+x["heroId"])
        kind="TigerFracture" if x["heroId"]=="Hero.Zodiac.Tiger" else (
            "BoarBattleWill" if x["heroId"]=="Hero.Zodiac.Boar" else "Unsupported")
        if x["nativeSourceMode"]!=kind or x["hasSourceMechanismCandidate"]!=(kind!="Unsupported"):
            problems.append("虚构生肖被动原生实现能力")
        if (x["approvedForRuntime"] or x["historicSkillFullyImplemented"] or
            x["nativeDataAssetPath"] is not None or x["nativeGameEffectAssetPath"] is not None or
            x["compiledEvidence"] or x["networkEvidence"]):
            problems.append("被动签审或UE效果完成状态不真实")
    return problems

def native_contract(def_h,def_cpp,ge_cpp,component_h,component_cpp,hero_cpp,ga_cpp):
    return {
      "asset_default_disabled":all(s in def_h+def_cpp for s in (
          "bApprovedForRuntime = false","Mode = EDivineBeastsV6PassiveMode::Unsupported",
          "DurationSeconds = 0.0f", "MaxStacks = 0")),
      "approval_expected_hero":all(s in def_cpp for s in (
          "HeroDefinitionId != ExpectedHero", 'FName(TEXT("Hero.Zodiac.Tiger"))',
          'FName(TEXT("Hero.Zodiac.Boar"))', "Rules.MaxStacks > 5")),
      "passive_effect_uses_platform_gas":all(s in ge_cpp for s in (
          "GetDamageReductionAttribute()", "GetDamageBonusAttribute()",
          "EGameplayEffectStackingType::AggregateBySource",
          "EGameplayEffectStackingType::AggregateByTarget",
          "Effect.StackLimitCount = 5","EGameplayModOp::Additive",
          "FGameplayEffectModifierMagnitude(Caller)",
          "EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication")),
      "reactive_not_tick":("PrimaryComponentTick.bCanEverTick = false" in component_cpp and
          "TickComponent(" not in component_cpp and "SetTimer(" not in component_cpp),
      "source_identity_lease":all(s in component_cpp for s in (
          "GetTrustedContextOperationId()", "GetCombatAvatarGeneration() == ActiveAvatarGeneration",
          "ASC->GetAvatarActor() == Owner", "IsCharacterReady()","GetHeroDefinitionId() == ActiveHeroId")),
      "tiger_actual_hit_only":all(s in component_cpp for s in (
          "AuthorityOnConfirmedDamageHit(", "!Result.IsSuccess()", "Result.AppliedToHealth",
          "AppliedToShield", "IsCurrentOwnerContext()", "AuthorityIsFriendlyCombatTarget(Target)",
          "FractureTarget.Get() != Target", "GetCurrentStackCount(FractureEffect) >= Rules.MaxStacks",
          "ApplyGameplayEffectSpecToTarget(")) and "AuthorityOnConfirmedDamageHit(" in ga_cpp,
      "boar_actual_damage_only":all(s in component_cpp for s in (
          "Event.EventType != EGamePlatformCombatEventType::Damage",
          "Event.TargetActor != GetOwner()","Event.TargetAvatarGeneration != ActiveAvatarGeneration",
          "TryRememberEvent(Event.EventId)", "GetCurrentStackCount(BattleWillEffect) >= Rules.MaxStacks",
          "ApplyGameplayEffectSpecToSelf(")),
      "start_stop_and_cleanup":all(s in component_cpp for s in (
          "OnReadinessChanged().Remove(ReadinessHandle)",
          "OnIdentityChanged().Remove(IdentityHandle)",
          "OnCombatEvent.RemoveDynamic(", "AuthorityDeactivate()",
          "RemoveActiveGameplayEffect(BattleWillEffect)",
          "RemoveActiveGameplayEffect(FractureEffect)",
          "SeenCombatEvents.Remove(Oldest)")),
      "mounted_but_off":all(s in hero_cpp for s in (
          "CreateDefaultSubobject<UDivineBeastsV6PassiveComponent>",
          "V6正式批准DataAsset尚不存在")),
      "no_forged_v6_grants":("AuthorityConfigureFromApprovedDefinition" not in hero_cpp and
          "AuthorityConfigureFromApprovedDefinition" not in ga_cpp and
          "AuthorityConfigureFromApprovedDefinition(" in component_cpp)
    }

class V6PassiveSourceGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.design=json.loads(read(V6))
        cls.reg=json.loads(read(REG))
        cls.dh=read(MODULE/"Public/Definitions/DivineBeastsV6PassiveDefinition.h")
        cls.dc=read(MODULE/"Private/Definitions/DivineBeastsV6PassiveDefinition.cpp")
        cls.gc=read(MODULE/"Private/Effects/DivineBeastsV6PassiveGameplayEffects.cpp")
        cls.ch=read(MODULE/"Public/Components/DivineBeastsV6PassiveComponent.h")
        cls.cc=read(MODULE/"Private/Components/DivineBeastsV6PassiveComponent.cpp")
        cls.hero=read(MODULE/"Private/Characters/DivineBeastsGameplayCharacter.cpp")
        cls.ga=read(MODULE/"Private/Abilities/DivineBeastsV6GameplayAbility.cpp")

    def test_01_historical_passives_are_12_and_unique(self):
        self.assertEqual(validate_registry(self.reg,self.design),[])

    def test_02_no_passive_grants_without_review(self):
        altered=copy.deepcopy(self.reg)
        altered["passives"][2]["approvedForRuntime"]=True
        self.assertNotEqual(validate_registry(altered,self.design),[])

    def test_03_does_not_fake_all_12_implemented(self):
        altered=copy.deepcopy(self.reg)
        altered["passives"][0]["nativeSourceMode"]="RatSense"
        self.assertNotEqual(validate_registry(altered,self.design),[])

    def test_04_true_native_event_and_gas_wiring_contract(self):
        evidence=native_contract(self.dh,self.dc,self.gc,self.ch,self.cc,self.hero,self.ga)
        self.assertTrue(all(evidence.values()),evidence)

    def test_05_source_cannot_process_unconfirmed_damage(self):
        altered=self.cc.replace(
            "!Result.IsSuccess()", "false")
        self.assertFalse(native_contract(self.dh,self.dc,self.gc,self.ch,altered,self.hero,self.ga)["tiger_actual_hit_only"])

    def test_06_source_cannot_skip_avatar_identity(self):
        altered=self.cc.replace(
            "GetCombatAvatarGeneration() == ActiveAvatarGeneration",
            "GetCombatAvatarGeneration() != ActiveAvatarGeneration",1)
        self.assertFalse(native_contract(self.dh,self.dc,self.gc,self.ch,altered,self.hero,self.ga)["source_identity_lease"])

    def test_07_no_tiger_stack_without_limit(self):
        altered=self.cc.replace(
            "GetCurrentStackCount(FractureEffect) >= Rules.MaxStacks",
            "GetCurrentStackCount(FractureEffect) < Rules.MaxStacks",1)
        self.assertFalse(native_contract(self.dh,self.dc,self.gc,self.ch,altered,self.hero,self.ga)["tiger_actual_hit_only"])

    def test_08_boar_can_only_gain_from_actual_damage(self):
        altered=self.cc.replace(
            "Event.EventType != EGamePlatformCombatEventType::Damage",
            "Event.EventType == EGamePlatformCombatEventType::Damage",1)
        self.assertFalse(native_contract(self.dh,self.dc,self.gc,self.ch,altered,self.hero,self.ga)["boar_actual_damage_only"])

    def test_09_teardown_must_revoke_owned_handles(self):
        altered=self.cc.replace(
            "RemoveActiveGameplayEffect(FractureEffect)",
            "GetCurrentStackCount(FractureEffect)")
        self.assertFalse(native_contract(self.dh,self.dc,self.gc,self.ch,altered,self.hero,self.ga)["start_stop_and_cleanup"])

    def test_10_no_client_side_damage_or_second_combat_system(self):
        for text in (self.gc,self.cc,self.hero):
            self.assertNotIn("NiagaraSystem",text)
            self.assertNotIn("UFUNCTION(Server",text)
            self.assertNotIn("BaseDamage = ",text)
        self.assertIn("ApplyGameplayEffectSpecToSelf",self.cc)

if __name__=="__main__":
    unittest.main(verbosity=2)
