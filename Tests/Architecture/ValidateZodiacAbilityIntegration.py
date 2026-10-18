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
           "AcquireDefinition(" in runtime and "GetLeaseState(" in runtime
           # 撤销时先摘除组件当前租约，外部回调不得释放后继请求的新租约。
           and "const FGamePlatformDataLease LeaseToRelease = DefinitionLease" in runtime
           and "DefinitionLease = {}" in runtime
           and "ReleaseDefinition(LeaseToRelease)" in runtime)
    verify("UI只读取复制快照而不授予技能",
           "GetLoadoutStateRef()" in ui and "AcquireResources(" in ui
           and "ReleaseResources(" in ui
           and not any(x in ui for x in ("GiveAbility(", "ApplyGameplayEffectToTarget(",
                                       "ApplyGameplayEffectToSelf(", "CalculateDamageMagnitude(")))
    verify("图标只在客户端表现主资产中引用",
           "TSoftObjectPtr<UTexture2D>" in ui_profile and "FText DisplayName" in ui_profile)


def check_wiring() -> None:
    """确认实际竞技出生使用带 ASC 的项目角色，不只是定义未引用的孤立类。"""
    arena = read(ARENA / "Private/Server/DivineBeastsArenaGameplayLifecycleAdapter.cpp")
    build = read(ARENA / "DivineBeastsArenaServer.Build.cs")
    pawn = read(ABILITIES / "Private/Characters/DivineBeastsGameplayCharacter.cpp")
    base_pawn = read(GAMEPLAY / "Source/DivineBeastsCharactersRuntime/Private/Characters/DivineBeastsCharacter.cpp")
    pawn_header = read(ABILITIES / "Public/Characters/DivineBeastsGameplayCharacter.h")
    panel = read(CLIENT_UI / "Private/Panels/Combat/DivineBeastsAbilityBarPanel.cpp")
    verify("竞技服务器真实出生路径装配项目可玩角色",
           "ADivineBeastsGameplayCharacter::StaticClass()" in arena
           and "DivineBeastsAbilitiesRuntime" in build)
    verify("同一角色继承唯一平台ASC和身份，仅追加技能装配组件",
           "CreateDefaultSubobject<UGamePlatformAbilitySystemComponent>" in base_pawn
           and "CreateDefaultSubobject<UDivineBeastsCharacterComponent>" in base_pawn
           and ": public ADivineBeastsCharacter" in pawn_header
           and "CreateDefaultSubobject<UDivineBeastsAbilityLoadoutComponent>" in pawn
           and "CreateDefaultSubobject<UGamePlatformAbilitySystemComponent>" not in pawn
           and "CreateDefaultSubobject<UDivineBeastsCharacterComponent>" not in pawn)
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
    # 用户批准的Development Client/Server验证必须配置与启动参数双重启用；Shipping/Test始终拒绝。
    config = read(ROOT / "Game/Config/DefaultGame.ini")
    development_policy = read(ABILITIES / "Private/Development/DivineBeastsDevelopmentAbilityPolicy.h")
    verify("开发能力集双重显式启用且Shipping/Test强制拒绝",
           "Set.bDevelopmentOnly && !DivineBeasts::DevelopmentAbilities::IsEnabledForCurrentProcess()" in grant
           and "#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST" in development_policy
           and "bAllowDevelopmentAbilitySets" in development_policy
           and "DBADevelopmentSkills" in development_policy
           and "bEligibleBuild && bConfigured && bCommandLineOptIn" in development_policy
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

    # 清空服务器Ready后先撤旧技能，最后发布新身份，避免换角色期间保留上一身份的GAS授权。
    binding = source.split("bool UDivineBeastsCharacterComponent::AuthorityBindTrustedContext", 1)[-1]
    invalidation = binding.find("bServerReady = false")
    notification = binding.find("ReadinessChanged.Broadcast(false)")
    identity_changed = binding.find("IdentityChanged.Broadcast()")
    verify("可信角色重绑先撤销旧技能再发布新身份",
           0 <= invalidation < notification < identity_changed
           and "IsOriginalOperationCurrent()" in binding[notification:identity_changed])

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
    panel_cpp = read(CLIENT_UI / "Private/Panels/Combat/DivineBeastsAbilityBarPanel.cpp")
    verify("同数值冷却重置仍刷新材质截止时钟",
           "!FMath::IsNearlyEqual(A.CooldownEndsAtAppSeconds, B.CooldownEndsAtAppSeconds" in ui
           and "Binding.Widget->ApplyOverlayTiming(EndsAtAppSeconds, DurationSeconds)" in panel_cpp)

    verify("技能界面阻止错误AbilityDefinitionId跨技能冒用",
           "Configured->AbilityDefinitionId.PrimaryAssetName != Grant.AbilityId" in ui)

    verify("服务器授权前检查气势成本与冷却效果存在",
           "GetCostGameplayEffect()" in grant and
           "GetCooldownGameplayEffect()" in grant and
           "GetMomentumAttribute()" in grant and
           "Balance.MomentumCost > 0.0f" in grant and
           "Balance.CooldownSeconds > 0.0f" in grant)

    # 启动效果与GAS技能成本不同：启动集合必须能仅靠它自己的效果句柄进行回滚。
    verify("技能集合启动效果拒绝瞬时和周期/执行计算",
           "IsStartupEffectReversible(" in grant and
           "Effect.DurationPolicy == EGameplayEffectDurationType::Instant" in grant and
           "Effect.Period.GetValueAtLevel(EffectLevel)" in grant and
           "!Effect.Executions.IsEmpty()" in grant)
    verify("启动效果拒绝和其他技能来源的效果堆叠",
           "Effect.GetStackingType() != EGameplayEffectStackingType::None" in grant)
    verify("技能集启动效果用UE公开API拒绝隐式GameplayEffectComponent副作用",
           "Effect.FindComponent(UGameplayEffectComponent::StaticClass())" in grant and
           'GameplayEffectComponent.h' in grant and
           "!Effect.GEComponents.IsEmpty()" not in grant)

    # 先验证所有效果，再按平台原始承诺先效果、后技能授予；错误时原路撤销句柄。
    effect_begin = grant.find("    // 依照既有平台授权合同：启动可回滚效果在先")
    grant_effect = grant.find("ASC->ApplyGameplayEffectToSelf(", effect_begin)
    grant_ability = grant.find("ASC->GiveAbility(Spec)", effect_begin)
    verify("启动效果先于技能Spec授予且仅撤销本组件持有句柄",
           effect_begin >= 0 and effect_begin < grant_effect < grant_ability and
           "OwnedEffectHandles.Add(Handle)" in grant and
           "ASC->RemoveActiveGameplayEffect(Handle)" in grant)
    startup_tests = read(ABILITIES /
        "Private/Tests/DivineBeastsAbilityAssemblyTests.cpp")
    verify("可回滚启动效果具有无世界的正反向UE自动化测试",
           "IsStartupEffectReversible(" in startup_tests and
           "EGameplayEffectDurationType::Instant" in startup_tests and
           "Period.SetValue(2.0f)" in startup_tests and
           "Executions.AddDefaulted()" in startup_tests and
           "FindFProperty<FEnumProperty>" in startup_tests and
           "EGameplayEffectStackingType::AggregateBySource" in startup_tests)



    verify("服务端按真实等级核对GAS成本/冷却实际数值",
           "GetStaticMagnitudeIfPossible" in grant and
           "ActualMomentumChange, -Balance.MomentumCost, 0.01f" in grant and
           "ActualCooldownSeconds, Balance.CooldownSeconds, 0.01f" in grant and
           "EGameplayEffectDurationType::Instant" in grant and
           "EGameplayEffectDurationType::HasDuration" in grant)
    verify("服务器二次核对技能集合UClass与执行策略",
           "Set.GetPrimaryAssetId() != ExpectedSetAssetId" in grant
           and "NewAbilityClasses.Contains(AbilityClass)" in grant
           and "AbilityClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)" in grant
           and "GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::NonInstanced" in grant
           and "GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly" in grant)
    verify("技能成本与冷却GE禁止未声明副作用",
           "!CostEffect->Executions.IsEmpty()" in grant
           and "CostEffect->FindComponent(UGameplayEffectComponent::StaticClass())" in grant
           and "!CooldownEffect->Modifiers.IsEmpty()" in grant
           and "!CooldownEffect->Executions.IsEmpty()" in grant
           and "!FMath::IsNearlyZero(CooldownPeriod)" in grant)

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
           # 同项目另一会话已将Hit改为指针，新增Pawn对象射线补偿；两种实现均须服务端
           # bHasValidatedHit、真实命中Actor与唯一Combat结算，不能因局部变量命名升级误报失败。
           and ('AuthorityApplyConfiguredDamage(Hit.GetActor()' in ability or
                'AuthorityApplyConfiguredDamage(Hit->GetActor()' in ability))



def placeholder_visual_contract(visual: str, policy: str, placeholder: str) -> dict[str, bool]:
    """按真实的表现桥＋独立白名单合同审查；不能靠旧CPP的字面字符串位置报通过。

    这里只证明源码门禁仍存在；运行时World包装、GAS资格和VFX资产仍须UE原生验证。
    """
    visible = " ".join(visual.split())
    permitted = " ".join(policy.split())
    heroes_match = re.search(
        r"static const TCHAR\* const Heroes\[\]\s*=\s*\{(.*?)\};", policy, re.S)
    heroes = re.findall(r'TEXT\("([a-z]+)"\)', heroes_match.group(1)) if heroes_match else []
    expected_heroes = [
        "rat", "ox", "tiger", "rabbit", "dragon", "snake",
        "horse", "goat", "monkey", "rooster", "dog", "boar"
    ]
    village = all(text in visible for text in (
        '#include "Development/DivineBeastsPlaceholderMagicVisualPolicy.h"',
        "DivineBeastsPlaceholderMagicVisualPolicy::IsVillageWorld(World)",
        "(!bDevelopment && !bPlaceholderVillage)",
        "World->GetNetMode() == NM_DedicatedServer",
    )) and all(text in permitted for text in (
        'TEXT("/DBAWorldPack_Village/Maps/L_Village_Start")',
        'TEXT("/DBAWorldPack_Village/Maps/UEDPIE_")',
        'TEXT("_L_Village_Start")',
        "PieTail.Left(PieTail.Len() - MapSuffix.Len()).IsNumeric()",
        "PackageName == VillagePath",
    ))
    identity = heroes == expected_heroes and all(text in permitted for text in (
        "inline bool IsApprovedPlaceholderSpell(FName AbilityId, FName Magic)",
        'Magic != TEXT("Fireball")',
        'Magic != TEXT("Frostbolt")',
        'Magic != TEXT("ArcaneMissiles")',
        'FString(TEXT("_placeholder_"))',
        'TEXT("@1")',
        'Value == FString(TEXT("dba.ability.")) + Hero + Suffix',
    )) and all(text in visible for text in (
        "DivineBeastsPlaceholderMagicVisualPolicy::IsApprovedPlaceholderSpell(Snapshot.AbilityId, Name)",
        "Snapshot.AbilityId",
    )) and all(text in placeholder for text in (
        'CastStyle = TEXT("DBA.Development.Fireball")',
        'CastStyle = TEXT("DBA.Development.Frostbolt")',
        'CastStyle = TEXT("DBA.Development.ArcaneMissiles")',
        "EGameplayAbilityNetExecutionPolicy::ServerOnly",
    ))
    development = all(text in visible for text in (
        "const bool bDevelopment = UDivineBeastsDevelopmentMagicDefinition::IsDevelopmentEnabledForCurrentProcess()",
        "(!bDevelopment && !bPlaceholderVillage)",
        "!UDivineBeastsDevelopmentMagicDefinition::IsDevelopmentEnabledForCurrentProcess() &&",
        "!DivineBeastsPlaceholderMagicVisualPolicy::IsApprovedPlaceholderSpell(Snapshot.AbilityId, Name)",
    ))
    return {"village_identity": village and identity, "development_gate": development}


def check_assets_and_policy() -> None:
    """只检查路径与禁令，真实资产缺失是待办，不应伪造PASS。"""
    cfg = read(ROOT / "Game/Config/DefaultGame.ini")
    verify("GamePlatformDefinition 扫描项目技能逻辑目录",
           '(Path="/DBAGameplay/Abilities")' in cfg)
    # 村地图身份和精确生肖白名单已迁至独立策略头；只验证真实调用链，不复活旧位置断言。
    visual = read(BASE / "DBAClient/Source/DivineBeastsPresentationClient/Private/"
                  "Adapters/DivineBeastsDevelopmentMagicPresentation.cpp")
    policy = read(BASE / "DBAClient/Source/DivineBeastsPresentationClient/Private/"
                  "Development/DivineBeastsPlaceholderMagicVisualPolicy.h")
    placeholder = read(ABILITIES / "Private/Abilities/DivineBeastsPlaceholderGameplayAbility.cpp")
    visual_contract = placeholder_visual_contract(visual, policy, placeholder)
    verify("正式Village占位冰火奥术仅以明确12生肖技能身份触发客户端Niagara",
           visual_contract["village_identity"])
    client_cook = read(ROOT / "Game/Config/Custom/FrontEndClient/DefaultGame.ini")
    server_cook = read(ROOT / "Game/Config/Custom/VillageServer/DefaultGame.ini")
    verify("临时法术Niagara和图标仅客户端显式Cook，Village服务器仍排除视觉资源",
           '+DirectoriesToAlwaysCook=(Path="/DBAPresentationPack_Core/VFX/DevelopmentMagic")' in client_cook
           and '+DirectoriesToAlwaysCook=(Path="/DBAUIPack_Core/UI/Abilities/MagicTests")' in client_cook
           and '+DirectoriesToNeverCook=(Path="/DBAPresentationPack_Core/VFX/DevelopmentMagic")' in server_cook)
    verify("开发魔法授权开关未因正式占位技能而删除",
           visual_contract["development_gate"])
    old_rules = read(ROOT / "AGENTS.md")
    verify("尊重现行已取消玩法禁令",
           "禁止恢复" in old_rules and "Element" in old_rules and "共鸣" in old_rules)
    ui_assets = list((BASE / "ContentPacks/Presentation/DBAUIPack_Core/Content").rglob(
        "WBP_DBA_UI_AbilityBar.uasset"))
    gameplay_assets = list((GAMEPLAY / "Content").rglob("*Ability*.uasset"))
    print(f"NOTE | 真实技能栏蓝图现存数量={len(ui_assets)}；"
          f"按名称检索的技能资产现存数量={len(gameplay_assets)}。"
          "此观察不属于静态代码测试通过的证明。")


def check_momentum_p0() -> None:
    """气势P0静态合同核查；不冒充UE运行、Cook或双客户端。"""
    source = GAMEPLAY / "Source/DivineBeastsCharactersRuntime"
    definition = read(source / "Public/Definitions/DivineBeastsMomentumDefinition.h")
    policy = read(source / "Private/Definitions/DivineBeastsMomentumDefinition.cpp")
    owner = read(source / "Private/Components/DivineBeastsCharacterComponent.cpp")
    owner_h = read(source / "Public/Components/DivineBeastsCharacterComponent.h")
    effect = read(source / "Private/Effects/DivineBeastsMomentumGainGameplayEffect.cpp")
    placeholder = read(ABILITIES / "Private/Abilities/DivineBeastsPlaceholderGameplayAbility.cpp")
    ui = read(CLIENT_UI / "Private/ViewModels/Combat/DivineBeastsAbilityBarViewModel.cpp")
    verify("气势0～100和每英雄普攻收益配置",
           "float BasicAttackHitGain = 10.0f" in definition and
           "float MaxMomentum = 100.0f" in definition and
           "MaxMomentum > 100.0f" in policy and "CalculateBasicAttackHitGain" in policy)
    verify("仅GAS正式提交的占位普攻真实命中产生增势",
           "if (!bProjectile)" in placeholder and
           "const bool bHit = AuthorityTraceForwardAndApplyDamage" in placeholder and
           "if (bHit && ActivationToken == Token" in placeholder and
           "Identity->AuthorityAwardMomentumForConfirmedBasicHit(Result, AvatarGeneration)" in placeholder)
    verify("增势鉴权、角色代次、实际伤害和事件去重",
           all(x in owner for x in (
               "Owner->HasAuthority()", "IsCharacterReady()", "ConfirmedHit.EventId.IsValid()",
               "ConfirmedHit.AppliedToHealth <= 0.0f", "ConfirmedHit.AppliedToShield <= 0.0f",
               "GetCombatAvatarGeneration() != ExpectedAvatarGeneration",
               "AwardedMomentumHitIds.Contains", "AwardedMomentumHitOrder.Num() > 128")) and
           "TSet<FGuid> AwardedMomentumHitIds" in owner_h)
    verify("奖励通过项目GAS即时GameplayEffect单一写入",
           all(x in effect for x in ("EGameplayEffectDurationType::Instant", "FSetByCallerFloat",
               "SSLM.Momentum.BasicHitGain",
               "UDivineBeastsMomentumAttributeSet::GetMomentumAttribute()")) and
           "ASC->ApplyGameplayEffectSpecToSelf" in owner)
    verify("气势不足提示只读GAS成本，不直接写属性",
           'TEXT("气势不足")' in ui and
           "Spec->Ability->CheckCost(" in ui and
           not any(x in ui for x in ("InitMomentum(", "SetMomentum(", "ApplyGameplayEffectSpecToSelf(")))


def main() -> int:
    check_module()
    check_ownership()
    check_wiring()
    check_runtime_safety()
    check_momentum_p0()
    check_assets_and_policy()
    print(f"RESULT | StaticAssertions={PASS + FAIL} Passed={PASS} Failed={FAIL}")
    print("LIMITS | 本脚本不验证UE5.8 UHT/编译、技能数值真实资产、"
          "Monolith蓝图、双客户端联机或客户端/专服Cook。")
    return 1 if FAIL else 0


if __name__ == "__main__":
    sys.exit(main())
