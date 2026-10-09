#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
《神兽联盟》技能系统三层归属与主数据静态审计（不需要 UE Editor）。
目的：让未完成的资产/编译验收与源码结构门禁分离，避免把存在文件误报为可运行技能。
用法：python Tests/Architecture/ValidateZodiacAbilityIntegration.py
副作用：只读；退出码0代表本脚本检查通过，不代表UHT、编译、AssetRegistry或联机通过。
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "Game/Plugins/DivineBeasts"
GAMEPLAY = BASE / "DBAGameplay"
ABILITIES = GAMEPLAY / "Source/DivineBeastsAbilitiesRuntime"
CLIENT_UI = BASE / "DBAClient/Source/DivineBeastsUIClient"
ARENA = BASE / "DBAArena/Source/DivineBeastsArenaServer"
PASS = 0
FAIL = 0


def verify(label: str, condition: bool, explanation: str = "") -> None:
    """逐项打印可复核结论，不吞掉异常；缺失代码应导致非零退出。"""
    global PASS, FAIL
    if condition:
        PASS += 1
    else:
        FAIL += 1
    print(("PASS" if condition else "FAIL") + " | " + label
          + (f" | {explanation}" if explanation else ""))


def read(path: Path) -> str:
    """按 UTF-8 精确读取目标源码，缺失时由调用方独立形成失败断言。"""
    return path.read_text(encoding="utf-8") if path.is_file() else ""


def check_module() -> None:
    """校验新模块属于原有项目插件，不创建额外 .uplugin 或反向依赖。"""
    desc = GAMEPLAY / "DBAGameplay.uplugin"
    plugin = json.loads(read(desc))
    modules = [part["Name"] for part in plugin.get("Modules", [])]
    verify("DBAGameplay 只新增一个真实技能运行模块",
           modules == ["DivineBeastsRuntime",
                       "DivineBeastsCharactersRuntime",
                       "DivineBeastsAbilitiesRuntime"],
           "不得恢复旧插件树")
    verify("技能运行模块只依赖既有低层/项目角色",
           "DivineBeastsCharactersRuntime" in read(ABILITIES / "DivineBeastsAbilitiesRuntime.Build.cs")
           and "DivineBeastsAbilitiesRuntime" not in read(GAMEPLAY / "Source/DivineBeastsCharactersRuntime/DivineBeastsCharactersRuntime.Build.cs"))
    verify("新模块无客户端视觉依赖",
           not any(token in read(ABILITIES / "DivineBeastsAbilitiesRuntime.Build.cs")
                   for token in ["DivineBeastsUIClient", "MobaPresentation", "GamePlatformVFX",
                                 "GamePlatformUIClient", "Niagara", "UMG", "DBAArena"]))


def check_ownership() -> None:
    """技能数据和UI投影必须通过身份/复制分层，避免项目层出现第二套权威数据。"""
    hero = read(GAMEPLAY / "Source/DivineBeastsCharactersRuntime/Public/Definitions/DivineBeastsHeroDefinition.h")
    definition = read(ABILITIES / "Public/Definitions/DivineBeastsAbilityDefinition.h")
    runtime = read(ABILITIES / "Private/Components/DivineBeastsAbilityLoadoutComponent.cpp")
    state = read(ABILITIES / "Public/Components/DivineBeastsAbilityLoadoutComponent.h")
    ui = read(CLIENT_UI / "Private/ViewModels/Combat/DivineBeastsAbilityBarViewModel.cpp")
    ui_profile = read(CLIENT_UI / "Public/Definitions/DivineBeastsAbilityUIProfile.h")
    verify("英雄只持有技能集合逻辑ID",
           "FName DefaultAbilitySetId" in hero and "TSubclassOf<UGameplayAbility>" not in hero
           and "TSoftObjectPtr<UTexture" not in hero)
    verify("技能逻辑定义继承平台Definition且不存图标",
           "UGamePlatformDefinitionBase" in definition
           and "TSoftObjectPtr<UDataTable>" in definition
           and all(term not in definition for term in ("TSoftObjectPtr<UTexture2D>", "TSoftObjectPtr<UNiagaraSystem>", "TSoftObjectPtr<USoundBase>")))
    verify("每级伤害/成本由技能数值行集中配置",
           all(name in read(ABILITIES / "Public/Types/DivineBeastsAbilityBalanceRow.h")
               for name in ["BaseDamage", "CooldownSeconds", "MomentumCost", "CastRangeCm"])
           and all(name not in read(ABILITIES / "Public/Types/DivineBeastsAbilityBalanceRow.h")
                   for name in ["AttackPowerCoefficient", "AbilityPowerCoefficient"]))
    verify("服务端唯一授予写入和来源授权撤销",
           "ASC->GiveAbility(Spec)" in runtime and "ASC->ClearAbility(Handle)" in runtime
           and "OwnedAbilityHandles" in state)
    verify("技能授权结果仅对主人复制",
           "DOREPLIFETIME_CONDITION(" in runtime and "COND_OwnerOnly" in runtime
           and "ReplicatedUsing=OnRep_LoadoutState" in state)
    verify("异步主资产获取沿用平台GamePlatformData租约",
           "AcquireDefinition(" in runtime and "ReleaseDefinition(DefinitionLease)" in runtime
           and "GetLeaseState(" in runtime)
    verify("UI只读取复制快照而不授予技能",
           "GetLoadoutStateRef()" in ui and "LoadPrimaryAsset(" in ui
           and not any(x in ui for x in ("GiveAbility(", "ApplyGameplayEffectToTarget(",
                                       "ApplyGameplayEffectToSelf(", "CalculateDamageMagnitude(")))
    verify("图标只在客户端表现主资产中引用",
           "TSoftObjectPtr<UTexture2D>" in ui_profile and "FText DisplayName" in ui_profile)


def check_wiring() -> None:
    """确认实际竞技出生使用带 ASC 的项目角色，不只是定义未引用的孤立类。"""
    arena = read(ARENA / "Private/Server/DivineBeastsArenaGameplayLifecycleAdapter.cpp")
    build = read(ARENA / "DivineBeastsArenaServer.Build.cs")
    pawn = read(ABILITIES / "Private/Characters/DivineBeastsGameplayCharacter.cpp")
    panel = read(CLIENT_UI / "Private/Panels/Combat/DivineBeastsAbilityBarPanel.cpp")
    verify("竞技服务器真实出生路径装配项目可玩角色",
           "ADivineBeastsGameplayCharacter::StaticClass()" in arena
           and "DivineBeastsAbilitiesRuntime" in build)
    verify("同一角色只组合平台ASC、身份及技能装配组件",
           "CreateDefaultSubobject<UGamePlatformAbilitySystemComponent>" in pawn
           and "CreateDefaultSubobject<UDivineBeastsAbilityLoadoutComponent>" in pawn
           and "CreateDefaultSubobject<UDivineBeastsCharacterComponent>" in pawn)
    verify("技能栏订阅持有者Pawn改变且释放旧委托",
           "OnPossessedPawnChanged.AddUniqueDynamic" in panel
           and "OnPossessedPawnChanged.RemoveDynamic" in panel
           and "RefreshAbilitySourceFromOwningPawn" in panel)
    verify("已有技能栏通用输入绑定保留",
           "ApplyAbilitySlots(" in panel and "GetAbilitySlotsView" in
           read(CLIENT_UI / "Public/Panels/Combat/DivineBeastsAbilityBarPanel.h"))
    verify("技能栏对正式Widget蓝图开放技能详细提示只读接口",
           "GetAbilitySlotDetails() const" in read(
               CLIENT_UI / "Public/Panels/Combat/DivineBeastsAbilityBarPanel.h")
           and "AbilityBarViewModel->GetSlotDetails()" in panel)
    grant = read(ABILITIES / "Private/Components/DivineBeastsAbilityLoadoutComponent.cpp")
    verify("英雄技能集拒绝空技能与绕过数据驱动基类的授予",
           "Set.Abilities.IsEmpty()" in grant and
           "if (!Configured)" in grant and
           "项目英雄技能未继承统一数据驱动技能基类" in grant)
    # 任何正式Client/Server路径均不得依赖编辑器测试开关授予DevelopmentOnly能力集。
    config = read(ROOT / "Game/Config/DefaultGame.ini")
    verify("开发能力集显式Editor授权且正式构建强制拒绝",
           "if (Set.bDevelopmentOnly)" in grant
           and "#if WITH_EDITOR && !UE_BUILD_SHIPPING && !UE_BUILD_TEST" in grant
           and "bAllowDevelopmentAbilitySets" in grant
           and "bAllowDevelopmentAbilitySets=false" in config)




def check_runtime_safety() -> None:
    """检查最近补齐的权威授权/冷却/气势和UI事件边界，避免仅复制身份就假装可释放。"""
    source = read(GAMEPLAY / "Source/DivineBeastsCharactersRuntime/Private/Components/DivineBeastsCharacterComponent.cpp")
    ability = read(ABILITIES / "Private/Abilities/DivineBeastsConfiguredGameplayAbility.cpp")
    grant = read(ABILITIES / "Private/Components/DivineBeastsAbilityLoadoutComponent.cpp")
    ui = read(CLIENT_UI / "Private/ViewModels/Combat/DivineBeastsAbilityBarViewModel.cpp")
    platform_h = read(ROOT /
        "Game/Plugins/GamePlatform/Gameplay/GamePlatformAbilitySystem/Source/GamePlatformAbilitySystem/Public/Components/GamePlatformAbilitySystemComponent.h")
    platform_cpp = read(ROOT /
        "Game/Plugins/GamePlatform/Gameplay/GamePlatformAbilitySystem/Source/GamePlatformAbilitySystem/Private/Components/GamePlatformAbilitySystemComponent.cpp")

    # 必须在旧角色状态被重置之前通知授权组件，否则会漏掉 Ready -> NotReady 边沿。
    binding = source.split("bool UDivineBeastsCharacterComponent::AuthorityBindTrustedContext", 1)[-1]
    notification = binding.find("ReadinessChanged.Broadcast(false)")
    invalidation = binding.find("bServerReady = false")
    verify("可信角色重绑先广播旧技能失效再清空Ready",
           notification >= 0 and invalidation > notification)

    verify("平台GAS在原生技能列表复制完成后发布事件",
           "OnRep_ActivateAbilities() override" in platform_h
           and "Super::OnRep_ActivateAbilities()" in platform_cpp
           and "AbilitySpecListChanged.Broadcast()" in platform_cpp)

    verify("客户端订阅并注销技能授权复制变化",
           "OnAbilitySpecListChanged().AddUObject" in ui and
           "OnAbilitySpecListChanged().Remove" in ui)

    verify("客户端订阅并注销冷却 GameplayEffect 变化",
           "OnActiveGameplayEffectAddedDelegateToSelf.AddUObject" in ui and
           "OnActiveGameplayEffectAddedDelegateToSelf.Remove" in ui and
           "OnAnyGameplayEffectRemovedDelegate().AddUObject" in ui and
           "OnAnyGameplayEffectRemovedDelegate().Remove" in ui)

    verify("客户端订阅气势与控制状态但不逐帧扫描",
           "GetMomentumAttribute()" in ui and "Control_Stun" in ui and
           "Control_Silence" in ui and "State_Dead" in ui and
           "Tick(" not in ui and "FTSTicker" not in ui)

    verify("客户端先核对角色身份与代次清理旧技能槽位",
           "GetAvatarGeneration() == Snapshot.AvatarGeneration" in ui and
           "if (!bIdentityCurrent)" in ui and "Slots.Reset()" in ui)

    verify("技能栏只依据GAS原生Spec校验真实可激活性",
           "TryResolveGrantedSpec(" in ui and "CanActivateAbility(" in ui and
           "GetDynamicSpecSourceTags().HasTagExact(InputTag)" in ui and
           "Spec->Level != Grant.AbilityLevel" in ui)

    verify("技能栏冷却遮罩取自真实GAS效果查询而非固定倒计时",
           "GetActiveEffectsTimeRemainingAndDuration" in ui and
           "MakeQuery_MatchAnyOwningTags" in ui and
           "FMath::Clamp(Pair.Key / Pair.Value" in ui)

    verify("技能界面阻止错误AbilityDefinitionId跨技能冒用",
           "Configured->AbilityDefinitionId.PrimaryAssetName != Grant.AbilityId" in ui)

    verify("服务器授权前检查气势成本与冷却效果存在",
           "GetCostGameplayEffect()" in grant and
           "GetCooldownGameplayEffect()" in grant and
           "GetMomentumAttribute()" in grant and
           "Balance.MomentumCost > 0.0f" in grant and
           "Balance.CooldownSeconds > 0.0f" in grant)


    verify("服务端按真实等级核对GAS成本/冷却实际数值",
           "GetStaticMagnitudeIfPossible" in grant and
           "ActualMomentumChange, -Balance.MomentumCost, 0.01f" in grant and
           "ActualCooldownSeconds, Balance.CooldownSeconds, 0.01f" in grant and
           "EGameplayEffectDurationType::Instant" in grant and
           "EGameplayEffectDurationType::HasDuration" in grant)

    verify("UI额外以死亡眩晕和非普攻沉默标志禁用技能",
           "ASC->HasMatchingGameplayTag" in ui and
           "bDead || bStunned || (bSilenced && !bPrimaryAttack)" in ui)

    details_h = read(CLIENT_UI / "Public/ViewModels/Combat/DivineBeastsAbilityBarViewModel.h")
    verify("项目技能提示提供名称等级冷却及禁用原因而不改平台槽位",
           "FDivineBeastsAbilitySlotDetails" in details_h and
           "GetSlotDetails()" in details_h and
           "Detail.DisplayName = UI->DisplayName" in ui and
           "Detail.Description = UI->Description" in ui and
           "Detail.CooldownRemainingSeconds = Pair.Key" in ui and
           "SlotDetails = MoveTemp(NewDetails)" in ui)
    verify("技能伤害必须在GAS正式Commit之后",
           "CommitAbility(" in ability and
           "bCommittedForCurrentActivation = bSucceeded" in ability and
           "if (!IsActive() || !bCommittedForCurrentActivation)" in ability and
           "bCommittedForCurrentActivation = false" in ability)

    verify("服务器保留平台Combat伤害唯一真源",
           "SourceCombat->ApplyDamage(Spec)" in ability and
           "GamePlatformCombatComponent" in ability and
           "CalculateDamageMagnitude" not in ability)
    ability_header = read(
        ABILITIES / "Public/Abilities/DivineBeastsConfiguredGameplayAbility.h")
    verify("项目技能服务器射线判定以真实命中驱动唯一战斗结算",
           "AuthorityTraceForwardAndApplyDamage" in ability_header
           and "BlueprintAuthorityOnly" in ability_header
           and "LineTraceSingleByChannel" in ability
           and "Source->HasAuthority()" in ability
           and "HitContext.bHasValidatedHit = true" in ability
           and "AuthorityApplyConfiguredDamage(Hit.GetActor()" in ability)


def check_assets_and_policy() -> None:
    """只检查路径与禁令，真实资产缺失是待办，不应伪造PASS。"""
    cfg = read(ROOT / "Game/Config/DefaultGame.ini")
    verify("GamePlatformDefinition 扫描项目技能逻辑目录",
           '(Path="/DBAGameplay/Abilities")' in cfg)
    old_rules = read(ROOT / "AGENTS.md")
    verify("尊重现行已取消玩法禁令",
           "禁止恢复" in old_rules and "Element" in old_rules and "共鸣" in old_rules)
    ui_assets = list((BASE / "ContentPacks/Presentation/DBAUIPack_Core/Content").rglob(
        "WBP_DBA_UI_AbilityBar.uasset"))
    gameplay_assets = list((GAMEPLAY / "Content").rglob("*Ability*.uasset"))
    print(f"NOTE | 真实技能栏蓝图现存数量={len(ui_assets)}；"
          f"按名称检索的技能资产现存数量={len(gameplay_assets)}。"
          "此观察不属于静态代码测试通过的证明。")


def main() -> int:
    check_module()
    check_ownership()
    check_wiring()
    check_runtime_safety()
    check_assets_and_policy()
    print(f"RESULT | StaticAssertions={PASS + FAIL} Passed={PASS} Failed={FAIL}")
    print("LIMITS | 本脚本不验证UE5.8 UHT/编译、技能数值真实资产、"
          "Monolith蓝图、双客户端联机或客户端/专服Cook。")
    return 1 if FAIL else 0


if __name__ == "__main__":
    sys.exit(main())
