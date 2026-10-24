/**
 * 神兽联盟开发魔法的原生资源与服务器 GAS 回归，属于 DBAGameplay 项目层。
 * 调用方为 Editor 自动化；仅在游戏线程创建隔离 GI/World，使用正式 Data 租约、
 * 真实子鼠 Hero/开发与占位魔法 Definition、数值表、GA 和 GE，不创建测试 UCLASS 或修改资产/CDO。
 * 夹具通过现有可信身份初始化与中立 GameplayEligibility 接入服务器技能资格；
 * 这里不验证后端准入、联网复制、目标伤害结算、客户端 VFX 或 Cook。
 * 连发测试要求启动时已有 bAllowDevelopmentAbilitySets=true 和 -DBADevelopmentSkills；
 * 缺资源、标签、角色 Ready 或开发资格均报错，不能以跳过或默认成功替代。
 * 测试所有者先解绑观察回调、取消技能并销毁借用角色，再释放自身租约和世界。
 */
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/DivineBeastsDevelopmentMagicAbility.h"
#include "Abilities/DivineBeastsPlaceholderGameplayAbility.h"
#include "Attributes/DivineBeastsMomentumAttributeSet.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Components/SphereComponent.h"
#include "Definitions/DivineBeastsDevelopmentMagicDefinition.h"
#include "Definitions/DivineBeastsAbilityDefinition.h"
#include "Definitions/GamePlatformAbilitySetDefinition.h"
#include "Effects/GamePlatformCombatGameplayEffects.h"
#include "Actors/GamePlatformCombatProjectile.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "HAL/PlatformTime.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/GarbageCollection.h"

namespace DivineBeastsDevelopmentMagicTests
{
/** 独立于生产算法的计划基线；伤害/气势为数值，距离为厘米，速度为厘米/秒，时间为秒。 */
struct FExpectedMagic
{
    const TCHAR* Name;                       // 真实作者资产的稳定英文后缀。
    const TCHAR* AbilityId;                  // 已登记逻辑 ID，不能由待测 Definition 反算期望值。
    const TCHAR* CooldownTag;                // 对应开发槽的真实字典标签。
    EDivineBeastsDevelopmentMagicKind Kind;  // 项目外观语义，不作为另一套技能执行器。
    float Damage, Cost, Cooldown, Speed;     // 计划批准的一级开发数值。
    int32 Count;                            // 一次 Commit 后的弹数。
    float Interval, HelixRadius, HelixTurns; // 连发间隔秒、半径厘米、频率转/秒。
};

static const FExpectedMagic ExpectedMagic[] =
{
    {TEXT("Fireball"), TEXT("dba.ability.rat_dev_fireball@1"), TEXT("DivineBeasts.Development.Cooldown.Rat.Active01"),
        EDivineBeastsDevelopmentMagicKind::Fireball, 40.f, 0.f, 2.f, 1200.f, 1, 0.f, 0.f, 0.f},
    {TEXT("Frostbolt"), TEXT("dba.ability.rat_dev_frostbolt@1"), TEXT("DivineBeasts.Development.Cooldown.Rat.Active02"),
        EDivineBeastsDevelopmentMagicKind::Frostbolt, 30.f, 0.f, 1.5f, 1600.f, 1, 0.f, 0.f, 0.f},
    {TEXT("ArcaneMissiles"), TEXT("dba.ability.rat_dev_arcanemissiles@1"), TEXT("DivineBeasts.Development.Cooldown.Rat.Ultimate"),
        EDivineBeastsDevelopmentMagicKind::ArcaneMissiles, 12.f, 0.f, 3.f, 1400.f, 3, 0.2f, 18.f, 3.f}
};

/** 定位已保存对象或蓝图生成类，仅构造软路径；不加载、注册、创建或写入资产。 */
static FSoftObjectPath MagicPath(const TCHAR* Prefix, const FExpectedMagic& Expected,
    const TCHAR* Suffix = TEXT(""), bool bGeneratedClass = false)
{
    const FString Name = FString::Printf(TEXT("%s_DBA_Rat_Dev%s%s"), Prefix, Expected.Name, Suffix);
    return FSoftObjectPath(FString::Printf(TEXT("/Game/Development/DivineBeasts/Abilities/MagicTests/%s.%s%s"),
        *Name, *Name, bGeneratedClass ? TEXT("_C") : TEXT("")));
}

/** 每条用例独立创建夹具，失败不继续执行后续步骤；不共享角色、计数或运行状态。 */
enum class EScenario : uint8
{
    AssetContracts, GameplayEffects, Burst, Cancel, StaleSource, CommitAvatarSwap,
    SpawnSuccessor, SpawnStaleSource, ZeroMomentum, FreshSpawnInput,
    TargetSelectionSwitch, TargetDestroyedGC, TargetAvatarChanged, Healing, FrostboltWindup, TargetRange
};

/** 来自真实 Actor 事件的只读证据；弱引用不延长弹体寿命，终态快照允许销毁后继续审计。 */
struct FObservedProjectile
{
    TWeakObjectPtr<AGamePlatformCombatProjectile> Actor;
    FGamePlatformCombatProjectileFacts Facts;
    FVector Origin = FVector::ZeroVector;
    FDelegateHandle FactsHandle;
    double StartedWorldSeconds = -1.0; // 首次真实bStarted事实的隔离世界时钟，独立检测Timer量化而非只数弹数。
};

/**
 * 有界异步资源夹具：30秒墙钟超时；普通运行段12帧，冰箭起手/取消/换代段49帧，每帧推进隔离世界0.05秒。
 * Data 自己完成资源加载；不手写 Ready、完成回调、GFrameCounter 或技能私有连发状态。
 * 返回 true 仅表示潜伏命令结束，成功与否由 Automation 的真实断言/错误决定。
 */
class FRealMagicCommand final : public IAutomationLatentCommand
{
public:
    FRealMagicCommand(FAutomationTestBase* InTest, EScenario InScenario, bool bInUsePlaceholder = false)
        : Test(InTest), Scenario(InScenario), bUsePlaceholder(bInUsePlaceholder), StartedSeconds(FPlatformTime::Seconds()) {}

    ~FRealMagicCommand() override
    {
        if (!Instance.IsValid()) { return; }
        UWorld* World = Instance->GetWorld();
        if (World && SpawnHandle.IsValid()) { World->RemoveOnActorSpawnedHandler(SpawnHandle); }
        for (auto& Observed : Projectiles)
        {
            if (Observed.Actor.IsValid()) { Observed.Actor->OnFlightFactsChanged().Remove(Observed.FactsHandle); }
        }
        if (ASC.IsValid())
        {
            ASC->AbilityCommittedCallbacks.Remove(CommitHandle);
            ASC->OnGameplayEffectAppliedDelegateToSelf.Remove(EffectHandle);
            if (AbilityHandle.IsValid()) { ASC->CancelAbilityHandle(AbilityHandle); ASC->ClearAbility(AbilityHandle); }
        }
        // 身份组件拥有自己的 Hero 租约；先结束借用者，不能提前释放本测试的预热需求。
        if (Pawn.IsValid()) { Pawn->Destroy(); }
        if (Controller.IsValid()) { Controller->Destroy(); }
        if (OtherAvatar.IsValid()) { OtherAvatar->Destroy(); }
        if (FrozenTarget.IsValid()) { FrozenTarget->Destroy(); }
        if (AlternateTarget.IsValid()) { AlternateTarget->Destroy(); }
        if (auto* Data = IGamePlatformDataService::Get(*Instance); Data && WarmupLease.IsValid())
        { Data->ReleaseResources(WarmupLease); }
        if (WarmupOwner.IsValid()) { WarmupOwner->Destroy(); }
        if (World) { World->EndPlay(EEndPlayReason::Quit); }
        Instance->Shutdown();
        if (World) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
    }

    bool Update() override
    {
        if (FPlatformTime::Seconds() - StartedSeconds > 30.0)
        { Test->AddError(TEXT("开发魔法真实资源/角色夹具超过30秒；不能用替身或跳过记为通过")); return true; }
        if (!Instance.IsValid()) { return !InitializeWorldAndLease(); }
        UWorld* World = Instance->GetWorld();
        auto* Data = IGamePlatformDataService::Get(*Instance);
        if (!World || !Data || World->bIsTearingDown)
        { Test->AddError(TEXT("开发魔法夹具的真实World/Data已失效")); return true; }
        if (!bAssetsChecked)
        {
            // 下一真实自动化帧驱动 Data 的延期通知；不在同帧忙等或阻塞引擎加载线程。
            World->Tick(LEVELTICK_All, 0.01f);
            const auto State = Data->GetLeaseState(WarmupLease);
            if (State == EGamePlatformDataRequestState::Loading) { return false; }
            if (!Test->TestEqual(TEXT("真实魔法资源租约成功"), State, EGamePlatformDataRequestState::Succeeded) ||
                !CheckAssetContracts()) { return true; }
            bAssetsChecked = true;
            if (Scenario == EScenario::AssetContracts) { return true; }
            if (!InitializeCharacter()) { return true; }
            // 留出真实调度帧处理角色自身需求；Identity的既有幂等配置保证迟到Hero回调不重置气势。
            return false;
        }
        if (!bCastStarted)
        {
            World->Tick(LEVELTICK_All, 0.01f);
            auto* Identity = Pawn.IsValid() ? Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>() : nullptr;
            if (!Identity || !Identity->IsCharacterReady())
            { Test->AddError(TEXT("真实Hero初始化未Ready，拒绝伪造服务器GAS夹具")); return true; }
            if (Scenario == EScenario::GameplayEffects) { CheckGameplayEffects(); return true; }
            if (Scenario == EScenario::FreshSpawnInput) { CheckFreshSpawnInput(); return true; }
            if (Scenario == EScenario::Healing) { CheckPrimaryHealing(); return true; }
            if (Scenario == EScenario::TargetRange)
            { bCastStarted = BeginSelectedTargetRange(); return !bCastStarted; }
            if (Scenario == EScenario::FrostboltWindup)
            { bCastStarted = BeginFrostboltWindup(); return !bCastStarted; }
            if (!BeginRealActivation()) { return true; }
            bCastStarted = true;
            return false;
        }
        World->Tick(LEVELTICK_All, 0.05f);
        ++FlightFrames;
        if (Scenario == EScenario::TargetRange)
        {
            // TimerManager按真实引擎帧去重；不能在同一Automation帧内循环Tick伪造0.6秒连发。
            if (FlightFrames < 12) return false;
            CheckSelectedTargetRangeTerminal(); return true;
        }
        if (Scenario == EScenario::FrostboltWindup) { return CheckFrostboltWindupFrame(); }
        if (Scenario == EScenario::Burst)
        {
            // 在间隔前及两个间隔后读取真实Spawn数，不能只在终态数三枚而漏掉错误释放时机。
            if (FlightFrames == 3) { Test->TestEqual(TEXT("0.15秒时第二枚尚未释放"), Projectiles.Num(), 1); }
            if (FlightFrames == 5) { Test->TestEqual(TEXT("0.25秒时恰好释放两枚"), Projectiles.Num(), 2); }
            // UE Timer以帧执行且到期比较为严格大于；每段0.2秒允许本夹具一帧0.05秒量化，不要求亚帧回调。
            if (FlightFrames == 11) { Test->TestEqual(TEXT("0.55秒时已完整释放三枚"), Projectiles.Num(), 3); }
        }
        if (Scenario == EScenario::Burst && FlightFrames == 1)
        {
            // 第一枚初相位为0：0.05秒应前进70厘米，并产生真实螺旋横向位移。
            // 期望值手算于计划的18厘米/3转每秒，不调用待测 PositionAt 或 Motion 来生成期望。
            if (!Projectiles.IsEmpty() && Projectiles[0].Actor.IsValid())
            {
                const FVector Displacement = Projectiles[0].Actor->GetActorLocation() - Projectiles[0].Origin;
                Test->TestTrue(TEXT("首枚弹体真实沿螺旋运动而非只播放尾迹"),
                    Displacement.Equals(FVector(70.0, -7.419865, 14.562306), 0.1));
            }
            else { Test->AddError(TEXT("螺旋运动断言缺少真实存活的第一枚弹体")); }
        }
        if (FlightFrames < 12) { return false; }
        CheckTerminalBehavior();
        return true;
    }

private:
    /** 初始化已存在的 GI 方案；World 只初始化一次，合法 GameMode 提供真实 PlayerState/物理生命周期。 */
    bool InitializeWorldAndLease()
    {
        if (!Test->TestNotNull(TEXT("真实引擎实例"), GEngine)) { return false; }
        Instance.Reset(NewObject<UGameInstance>(GEngine));
        Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
        UWorld* World = Instance->GetWorld();
        auto* Data = IGamePlatformDataService::Get(*Instance);
        if (!Test->TestNotNull(TEXT("真实GI世界"), World) || !Test->TestNotNull(TEXT("正式Data服务"), Data)) { return false; }
        FURL URL; URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
        if (!Test->TestTrue(TEXT("夹具基础GameMode创建"), World->SetGameMode(URL))) { return false; }
        World->InitializeActorsForPlay(URL); World->BeginPlay();
        if (!Test->TestTrue(TEXT("隔离世界具有真实BeginPlay及物理场景"), World->HasBegunPlay() && World->GetPhysicsScene()))
        { return false; }
        WarmupOwner = World->SpawnActor<AActor>();
        if (!Test->TestTrue(TEXT("真实预热租约Owner存在"), WarmupOwner.IsValid())) { return false; }
        const FSoftObjectPath HeroPath = FDivineBeastsHeroCatalog::GetDefinitionAssetPath(TEXT("Hero.Zodiac.Rat"));
        if (!Test->TestTrue(TEXT("子鼠Hero已真实登记"), HeroPath.IsValid())) { return false; }
        TArray<FSoftObjectPath> Paths = {HeroPath,
            FSoftObjectPath(TEXT("/Game/Development/DivineBeasts/Abilities/MagicTests/DT_DBA_DevMagicBalance.DT_DBA_DevMagicBalance"))};
        for (const auto& Expected : ExpectedMagic)
        {
            Paths.Add(MagicPath(TEXT("DA"), Expected));
            Paths.Add(MagicPath(TEXT("GA"), Expected, TEXT(""), true));
            Paths.Add(MagicPath(TEXT("GE"), Expected, TEXT("Cost"), true));
            Paths.Add(MagicPath(TEXT("GE"), Expected, TEXT("Cooldown"), true));
        }
        if (bUsePlaceholder)
        {
            // 真实占位执行器沿用已保存的独立GA/Definition/表及GE；不修改CDO来伪装成开发魔法。
            const FString Root(TEXT("/DBAGameplay/Abilities/Placeholders/"));
            Paths.Add(FSoftObjectPath(Root + TEXT("DT_DBA_PlaceholderBalance.DT_DBA_PlaceholderBalance")));
            Paths.Add(FSoftObjectPath(Root + TEXT("DA_DBA_Rat_PlaceholderArcaneMissiles.DA_DBA_Rat_PlaceholderArcaneMissiles")));
            Paths.Add(FSoftObjectPath(Root + TEXT("GA_DBA_Rat_PlaceholderArcaneMissiles.GA_DBA_Rat_PlaceholderArcaneMissiles_C")));
            Paths.Add(FSoftObjectPath(Root + TEXT("GE_DBA_Rat_PlaceholderArcaneMissilesCost.GE_DBA_Rat_PlaceholderArcaneMissilesCost_C")));
            Paths.Add(FSoftObjectPath(Root + TEXT("GE_DBA_Rat_PlaceholderArcaneMissilesCooldown.GE_DBA_Rat_PlaceholderArcaneMissilesCooldown_C")));
            if (Scenario == EScenario::Healing)
            {
                Paths.Add(FSoftObjectPath(Root + TEXT("DA_DBA_Rat_PlaceholderAbilitySet.DA_DBA_Rat_PlaceholderAbilitySet")));
                Paths.Add(FSoftObjectPath(Root + TEXT("DA_DBA_Rat_PlaceholderDirectedMissile.DA_DBA_Rat_PlaceholderDirectedMissile")));
                Paths.Add(FSoftObjectPath(Root + TEXT("GA_DBA_Rat_PlaceholderDirectedMissile.GA_DBA_Rat_PlaceholderDirectedMissile_C")));
                Paths.Add(FSoftObjectPath(Root + TEXT("DA_DBA_Rat_PlaceholderPrimary.DA_DBA_Rat_PlaceholderPrimary")));
                Paths.Add(FSoftObjectPath(Root + TEXT("GA_DBA_Rat_PlaceholderPrimary.GA_DBA_Rat_PlaceholderPrimary_C")));
                Paths.Add(FSoftObjectPath(Root + TEXT("GE_DBA_Rat_PlaceholderPrimaryCooldown.GE_DBA_Rat_PlaceholderPrimaryCooldown_C")));
            }
            if (Scenario == EScenario::FrostboltWindup)
            {
                Paths.Add(FSoftObjectPath(Root + TEXT("DA_DBA_Rat_PlaceholderFrostbolt.DA_DBA_Rat_PlaceholderFrostbolt")));
                Paths.Add(FSoftObjectPath(Root + TEXT("GA_DBA_Rat_PlaceholderFrostbolt.GA_DBA_Rat_PlaceholderFrostbolt_C")));
                Paths.Add(FSoftObjectPath(Root + TEXT("GE_DBA_Rat_PlaceholderFrostboltCooldown.GE_DBA_Rat_PlaceholderFrostboltCooldown_C")));
            }
        }
        FGamePlatformResult Accepted;
        WarmupLease = Data->AcquireResources(Paths, EGamePlatformDataLifetime::World, WarmupOwner.Get(),
            [WeakOwner = WarmupOwner, TestOwner = Test](const FGamePlatformDataLease&, const FGamePlatformResult& Result)
            {
                if (WeakOwner.IsValid() && !Result.IsSuccess())
                { TestOwner->AddError(FString::Printf(TEXT("真实魔法资源加载失败：%s"), *Result.Code.ToString())); }
            }, Accepted);
        return Test->TestTrue(TEXT("Data受理真实魔法预热租约"), Accepted.IsSuccess() && WarmupLease.IsValid());
    }

    /** 缺任何真实对象即失败；验证读取合同与 GA 消费的身份，不修改 Definition/CDO 来迁就测试。 */
    bool CheckAssetContracts()
    {
        bool bValid = true;
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedMagic); ++Index)
        {
            const auto& Expected = ExpectedMagic[Index];
            const FString Label = FString::Printf(TEXT("%s："), Expected.Name);
            const auto* Definition = Cast<UDivineBeastsDevelopmentMagicDefinition>(MagicPath(TEXT("DA"), Expected).ResolveObject());
            auto* AbilityClass = Cast<UClass>(MagicPath(TEXT("GA"), Expected, TEXT(""), true).ResolveObject());
            if (!Test->TestNotNull(Label + TEXT("真实Definition"), Definition) ||
                !Test->TestNotNull(Label + TEXT("真实GA生成类"), AbilityClass)) { return false; }
            const auto* Ability = Cast<UDivineBeastsDevelopmentMagicAbility>(AbilityClass->GetDefaultObject());
            if (!Test->TestNotNull(Label + TEXT("GA实际派生原生魔法执行器"), Ability)) { return false; }
            AbilityClasses[Index] = AbilityClass;
            const FPrimaryAssetId ExpectedId(FPrimaryAssetType(TEXT("GamePlatformDefinition")), FName(Expected.AbilityId));
            bValid &= Test->TestTrue(Label + TEXT("静态Definition合同合法"), Definition->ValidateDefinition().IsSuccess());
            bValid &= Test->TestTrue(Label + TEXT("主资产扫描与真实对象一致"), UAssetManager::Get().GetPrimaryAssetObject(ExpectedId) == Definition);
            bValid &= Test->TestEqual(Label + TEXT("GA消费正确Definition身份"), Ability->AbilityDefinitionId, ExpectedId);
            bValid &= Test->TestEqual(Label + TEXT("Hero所属子鼠"), Definition->HeroDefinitionId, FName(TEXT("Hero.Zodiac.Rat")));
            bValid &= Test->TestEqual(Label + TEXT("魔法分类"), Definition->MagicKind, Expected.Kind);
            bValid &= Test->TestEqual(Label + TEXT("真实技能种类"), Definition->Kind,
                Index == 2 ? EDivineBeastsAbilityKind::Ultimate : EDivineBeastsAbilityKind::Active);
            bValid &= Test->TestTrue(Label + TEXT("GA为服务器独占且每Actor独立实例"),
                Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly &&
                Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor);
            bValid &= Test->TestTrue(Label + TEXT("真实GA声明开发伤害能力"), Ability->bDevelopmentDamageSampleSupported);
            bValid &= Test->TestEqual(Label + TEXT("一次提交弹数"), Definition->ProjectileCount, Expected.Count);
            bValid &= Test->TestEqual(Label + TEXT("连发间隔秒"), Definition->ProjectileIntervalSeconds, Expected.Interval);
            bValid &= Test->TestEqual(Label + TEXT("速度厘米每秒"), Definition->ProjectileMotion.SpeedCmPerSecond, Expected.Speed);
            bValid &= Test->TestEqual(Label + TEXT("最远施放距离50米"), Definition->ProjectileMotion.RangeCm, 5000.f);
            bValid &= Test->TestEqual(Label + TEXT("螺旋半径厘米"), Definition->ProjectileMotion.HelixRadiusCm, Expected.HelixRadius);
            bValid &= Test->TestEqual(Label + TEXT("螺旋频率转每秒"), Definition->ProjectileMotion.HelixTurnsPerSecond, Expected.HelixTurns);
            FDivineBeastsAbilityBalanceRow Balance; FString Error;
            const bool bReadBalance = Definition->TryGetLoadedBalance(1, Balance, Error);
            if (!Test->TestTrue(Label + TEXT("从真实已预热表读取一级数值：") + Error, bReadBalance)) { return false; }
            bValid &= Test->TestEqual(Label + TEXT("数值行逻辑身份"), Balance.AbilityId, FName(Expected.AbilityId));
            bValid &= Test->TestEqual(Label + TEXT("一级基础伤害"), Balance.BaseDamage, Expected.Damage);
            bValid &= Test->TestEqual(Label + TEXT("法术伤害类型"), Balance.DamageType, EGamePlatformDamageType::Magic);
            bValid &= Test->TestEqual(Label + TEXT("气势成本"), Balance.MomentumCost, Expected.Cost);
            bValid &= Test->TestEqual(Label + TEXT("冷却秒"), Balance.CooldownSeconds, Expected.Cooldown);
            bValid &= Test->TestEqual(Label + TEXT("数值表最远距离50米"), Balance.CastRangeCm, 5000.f);
            bValid &= Test->TestFalse(Label + TEXT("拒绝零级"), Definition->TryGetLoadedBalance(0, Balance, Error));
            bValid &= Test->TestFalse(Label + TEXT("拒绝未制作的第二级"), Definition->TryGetLoadedBalance(2, Balance, Error));
            bValid &= Test->TestNotNull(Label + TEXT("法术通过原生GAS能量成本路径"), Ability->GetCostGameplayEffect());
            const UClass* LegacyCostClass = Cast<UClass>(MagicPath(TEXT("GE"), Expected, TEXT("Cost"), true).ResolveObject());
            const UGameplayEffect* Cost = LegacyCostClass ? LegacyCostClass->GetDefaultObject<UGameplayEffect>() : nullptr;
            const UGameplayEffect* Cooldown = Ability->GetCooldownGameplayEffect();
            if (!Test->TestNotNull(Label + TEXT("真实成本GE"), Cost) || !Test->TestNotNull(Label + TEXT("真实冷却GE"), Cooldown))
            { return false; }
            bValid &= Test->TestTrue(Label + TEXT("成本GE来自当前真实资产"),
                Cost->GetClass() == MagicPath(TEXT("GE"), Expected, TEXT("Cost"), true).ResolveObject());
            bValid &= Test->TestTrue(Label + TEXT("冷却GE来自当前真实资产"),
                Cooldown->GetClass() == MagicPath(TEXT("GE"), Expected, TEXT("Cooldown"), true).ResolveObject());
            bValid &= Test->TestTrue(Label + TEXT("成本瞬时、冷却有时限"),
                Cost->DurationPolicy == EGameplayEffectDurationType::Instant && Cooldown->DurationPolicy == EGameplayEffectDurationType::HasDuration);
            bValid &= Test->TestEqual(Label + TEXT("保留稳定身份的旧成本GE没有资源修改"), Cost->Modifiers.Num(), 0);
            const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(Expected.CooldownTag), false);
            bValid &= Test->TestTrue(Label + TEXT("冷却字典已登记且GA仅声明本槽标签"), Tag.IsValid() &&
                Ability->GetCooldownTags() && Ability->GetCooldownTags()->Num() == 1 && Ability->GetCooldownTags()->HasTagExact(Tag));
        }
        if (bUsePlaceholder)
        {
            PlaceholderArcaneClass = Cast<UClass>(FSoftObjectPath(TEXT("/DBAGameplay/Abilities/Placeholders/GA_DBA_Rat_PlaceholderArcaneMissiles.GA_DBA_Rat_PlaceholderArcaneMissiles_C")).ResolveObject());
            const auto* Ability = PlaceholderArcaneClass ? Cast<UDivineBeastsPlaceholderGameplayAbility>(PlaceholderArcaneClass->GetDefaultObject()) : nullptr;
            if (!Test->TestNotNull(TEXT("真实占位奥术GA及原生执行器"), Ability)) return false;
            bValid &= Test->TestNotNull(TEXT("占位GA保留真实冷却GE"), Ability->GetCooldownGameplayEffect());
            // 法术零费用由原生GAS成本合同保证，不能只把表改零而继续消费旧蓝图成本GE。
            bValid &= Test->TestNotNull(TEXT("占位法术通过原生GAS能量成本路径"), Ability->GetCostGameplayEffect());
            bValid &= Test->TestEqual(TEXT("占位GA确实为三发奥术"), Ability->PlaceholderMode, EDivineBeastsPlaceholderAbilityMode::ArcaneMissiles);
            bValid &= Test->TestEqual(TEXT("占位真实三发数"), Ability->ProjectileCount, 3);
            bValid &= Test->TestEqual(TEXT("占位真实间隔0.2秒"), Ability->ProjectileIntervalSeconds, 0.2f);
            const auto* Definition = Cast<UDivineBeastsAbilityDefinition>(UAssetManager::Get().GetPrimaryAssetObject(Ability->AbilityDefinitionId));
            // 普通资源租约成功不等于数值行合法；分别定位主资产发现、软表加载和领域校验，不能吞掉OutError。
            if (!Test->TestNotNull(TEXT("占位正式Definition按GA主资产身份已预热：") + Ability->AbilityDefinitionId.ToString(), Definition))
            {
                Test->AddError(TEXT("该ID的实际主资产扫描路径：") + UAssetManager::Get().GetPrimaryAssetPath(Ability->AbilityDefinitionId).ToString());
                return false;
            }
            bValid &= Test->TestEqual(TEXT("占位Definition实际身份与GA引用一致"), Definition->GetPrimaryAssetId(), Ability->AbilityDefinitionId);
            if (!Test->TestTrue(TEXT("占位Definition真实软表已通过资源租约预热：") + Definition->BalanceTable.ToSoftObjectPath().ToString(),
                Definition->BalanceTable.Get() != nullptr)) return false;
            FDivineBeastsAbilityBalanceRow Balance; FString Error;
            const bool bLoadedBalance = Definition->TryGetLoadedBalance(1, Balance, Error);
            bValid &= Test->TestTrue(TEXT("占位正式Definition与数值表已预热且一级数值合法：") + Error, bLoadedBalance);
        }
        return bValid;
    }

    /** 复用已有角色的 ASC/Combat/Identity 及真实 Hero 租约，不直接写角色 Ready 或能力激活 Gate。 */
    bool InitializeCharacter()
    {
        UWorld* World = Instance->GetWorld();
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Pawn = World->SpawnActor<ADivineBeastsCharacter>(FVector(0, 0, 1000), FRotator::ZeroRotator, Spawn);
        Controller = World->SpawnActor<APlayerController>();
        if (!Test->TestTrue(TEXT("真实服务器角色和控制器存在"), Pawn.IsValid() && Controller.IsValid())) { return false; }
        Controller->Possess(Pawn.Get());
        Pawn->GetCharacterMovement()->DisableMovement();
        ASC = Pawn->GetGamePlatformAbilitySystemComponent();
        auto* Identity = Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>();
        if (!Test->TestTrue(TEXT("角色拥有唯一真实ASC及身份组件"), ASC.IsValid() && IsValid(Identity)) ||
            !Test->TestNotNull(TEXT("GameMode创建的真实PlayerState"), Controller->PlayerState.Get())) { return false; }
        FGamePlatformCharacterInitializationContext Context;
        Context.CharacterId = TEXT("Automation.DevelopmentMagic.Rat"); Context.HeroDefinitionId = TEXT("Hero.Zodiac.Rat");
        Context.SpawnGeneration = 1; Context.AvatarGeneration = 1;
        FString Error;
        if (!Test->TestTrue(TEXT("可信角色身份受理"), Identity->AuthorityBindTrustedContext(Context, Error)) ||
            !Test->TestTrue(TEXT("真实Hero成功租约交接"), Identity->TryUsePreloadedDefinition(WarmupLease, *WarmupOwner.Get(), Error)))
        { Test->AddError(Error); return false; }
        return Test->TestTrue(TEXT("角色已真实Ready且拥有权威ActorInfo"),
            Identity->IsCharacterReady() && Pawn->HasAuthority() && ASC->GetAvatarActor() == Pawn.Get());
    }

    /**
     * 数字1飞弹及数字5治疗真实GAS输入回归；友方关系替身仅限此隔离测试。
     * 验证20%目标最大生命、上限、0气势、10秒GAS冷却、敌方不获治疗，以及死亡/超距/旧代次拒绝且不自疗。
     * 关系替身不进入生产；竞技Roster桥另由真实装配负责。本测试不声称联机/UI/VFX已经验收。
     */
    void CheckPrimaryHealing()
    {
        UWorld* World = Instance->GetWorld();
        UClass* HealingClass = Cast<UClass>(FSoftObjectPath(TEXT("/DBAGameplay/Abilities/Placeholders/GA_DBA_Rat_PlaceholderPrimary.GA_DBA_Rat_PlaceholderPrimary_C")).ResolveObject());
        const auto* Ability = HealingClass ? HealingClass->GetDefaultObject<UDivineBeastsPlaceholderGameplayAbility>() : nullptr;
        if (!Test->TestNotNull(TEXT("已迁移真实Primary GA"), Ability)) return;
        if (!Test->TestEqual(TEXT("稳定Primary身份追加治疗模式"), Ability->PlaceholderMode, EDivineBeastsPlaceholderAbilityMode::Healing) ||
            !Test->TestNotNull(TEXT("Primary真实冷却GE"), Ability->GetCooldownGameplayEffect())) return;
        Test->TestNotNull(TEXT("治疗使用独立能量成本GE"), Ability->GetCostGameplayEffect());
        const FGameplayTag CooldownTag = FGameplayTag::RequestGameplayTag(TEXT("DivineBeasts.Placeholder.Cooldown.Primary"));
        auto* SourceCombat = Pawn->GetGamePlatformCombatComponent();
        auto* Eligibility = Pawn->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>();
        Eligibility->SetServerPlayerActive(true);
        ASC->SetNumericAttributeBase(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute(), 0.f);
        const auto* Set = Cast<UGamePlatformAbilitySetDefinition>(FSoftObjectPath(TEXT("/DBAGameplay/Abilities/Placeholders/DA_DBA_Rat_PlaceholderAbilitySet.DA_DBA_Rat_PlaceholderAbilitySet")).ResolveObject());
        if (!Test->TestNotNull(TEXT("真实技能授权集"), Set)) return;
        const FGameplayTag HealingInput = FGameplayTag::RequestGameplayTag(TEXT("Platform.Ability.Input.DivineBeasts.Slot3"));
        const FGameplayTag MissileInput = FGameplayTag::RequestGameplayTag(TEXT("Platform.Ability.Input.DivineBeasts.Primary"));
        const auto* HealGrant = Set->Abilities.FindByPredicate([&](const auto& G) { return G.InputTag == HealingInput; });
        const auto* MissileGrant = Set->Abilities.FindByPredicate([&](const auto& G) { return G.InputTag == MissileInput; });
        if (!Test->TestNotNull(TEXT("数字5真实治疗授权"), HealGrant) || !Test->TestNotNull(TEXT("数字1真实飞弹授权"), MissileGrant)) return;
        if (!Test->TestTrue(TEXT("数字5保留原治疗类"), HealGrant->AbilityClass.Get() == HealingClass)) return;
        const auto* MissileClass = MissileGrant->AbilityClass.Get();
        const auto* MissileCDO = MissileClass ? Cast<UDivineBeastsPlaceholderGameplayAbility>(MissileClass->GetDefaultObject()) : nullptr;
        if (!Test->TestNotNull(TEXT("定向飞弹真实类已预热"), MissileCDO)) return;
        Test->TestEqual(TEXT("独立定向飞弹表现语义"), MissileCDO->PresentationStyleOverride, FName(TEXT("DBA.Development.DirectedMissile")));
        LocalInputPlayer.Reset(NewObject<ULocalPlayer>(GEngine)); Controller->SetPlayer(LocalInputPlayer.Get());
        FGameplayAbilitySpec HealSpec(HealingClass, 1); HealSpec.GetDynamicSpecSourceTags().AddTag(HealingInput);
        AbilityHandle = ASC->GiveAbility(HealSpec);
        FGameplayAbilitySpec MissileSpec(MissileGrant->AbilityClass.Get(), 1); MissileSpec.GetDynamicSpecSourceTags().AddTag(MissileInput);
        const auto MissileHandle = ASC->GiveAbility(MissileSpec);
        if (!InitializeBurstTargets()) return;
        Pawn->SetActorRotation(FRotator(0, 90, 0));
        SpawnHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateRaw(this, &FRealMagicCommand::ObserveProjectile));
        Test->TestTrue(TEXT("按键1平台输入受理"), ASC->AbilityInputPressed(MissileInput, ASC->GetInputToken()).IsSuccess());
        ASC->ProcessAbilityInput(); ASC->AbilityInputReleased(MissileInput, ASC->GetInputToken());
        Test->TestEqual(TEXT("按键1只创建一枚权威弹体"), Projectiles.Num(), 1);
        if (Projectiles.Num()==1)
        {
            Test->TestEqual(TEXT("弹体复制正确表现身份"), Projectiles[0].Facts.PresentationStyleId, FName(TEXT("DBA.Development.DirectedMissile")));
            if (auto* Missile = Projectiles[0].Actor.Get())
            {
                Test->TestTrue(TEXT("真实按键1沿角色施法朝向起飞而非直接对准侧方目标"),
                    FVector::DotProduct(Missile->GetActorForwardVector(), Pawn->GetActorForwardVector()) > .999);
                const FVector Start = Missile->GetActorLocation();
                Missile->Tick(1.f / 120.f);
                Test->TestTrue(TEXT("真实技能入口首段保持初始切线并开始转弯"),
                    FVector::DotProduct((Missile->GetActorLocation() - Start).GetSafeNormal(), Pawn->GetActorForwardVector()) > .98);
                Missile->AuthorityCancel();
            }
        }
        SourceCombat->AuthoritySelectProjectileTarget(nullptr, SourceCombat->GetCombatAvatarGeneration(), 0);
        if (FrozenTarget.IsValid()) FrozenTarget->Destroy();
        if (AlternateTarget.IsValid()) AlternateTarget->Destroy();
        FrozenTarget.Reset(); AlternateTarget.Reset();
        ASC->CancelAbilityHandle(MissileHandle); ASC->ClearAbility(MissileHandle);
        const auto ClearCooldown = [&]()
        { ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(CooldownTag)); };
        const auto SetHealth = [](UGamePlatformAbilitySystemComponent* System, float Health)
        { System->SetNumericAttributeBase(UGamePlatformCombatAttributeSet::GetHealthAttribute(), Health); };

        const float SourceMaximum = SourceCombat->GetCombatMaxHealth();
        SetHealth(ASC.Get(), SourceMaximum * .5f);
        Test->TestTrue(TEXT("按键5平台输入受理"), ASC->AbilityInputPressed(HealingInput, ASC->GetInputToken()).IsSuccess());
        ASC->ProcessAbilityInput(); ASC->AbilityInputReleased(HealingInput, ASC->GetInputToken());
        Test->TestEqual(TEXT("自疗为自己最大生命20%"), SourceCombat->GetCombatHealth(), SourceMaximum * .7f);
        Test->TestEqual(TEXT("零气势治疗后仍为零"), ASC->GetNumericAttribute(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute()), 0.f);
        Test->TestTrue(TEXT("治疗真实授予GAS冷却标签"), ASC->HasMatchingGameplayTag(CooldownTag));
        const auto Cooldowns = ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CooldownTag)));
        if (Test->TestEqual(TEXT("治疗只有一份真实冷却"), Cooldowns.Num(), 1))
            Test->TestEqual(TEXT("治疗实际冷却10秒"), ASC->GetActiveGameplayEffect(Cooldowns[0])->GetDuration(), 10.f);
        Test->TestFalse(TEXT("冷却中不能再次激活"), ASC->TryActivateAbility(AbilityHandle, false));
        ClearCooldown();

        // 治疗移到数字5后仍为真实法术，按键位置不能绕过平台沉默GE。
        auto Silence = ASC->MakeOutgoingSpec(UGamePlatformSilenceGameplayEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
        if (!Test->TestTrue(TEXT("可创建真实沉默GE"), Silence.IsValid())) return;
        Silence.Data->SetDuration(1.f, true);
        const auto SilenceHandle = ASC->ApplyGameplayEffectSpecToSelf(*Silence.Data.Get());
        Test->TestTrue(TEXT("真实沉默GE已应用"), SilenceHandle.IsValid());
        ASC->AbilityInputPressed(HealingInput, ASC->GetInputToken()); ASC->ProcessAbilityInput();
        ASC->AbilityInputReleased(HealingInput, ASC->GetInputToken());
        Test->TestEqual(TEXT("沉默时按5不增加生命"), SourceCombat->GetCombatHealth(), SourceMaximum * .7f);
        Test->TestFalse(TEXT("沉默拒绝不产生治疗冷却"), ASC->HasMatchingGameplayTag(CooldownTag));
        ASC->RemoveActiveGameplayEffect(SilenceHandle);

        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        FrozenTarget = World->SpawnActor<ADivineBeastsCharacter>(FVector(300, 0, 1000), FRotator::ZeroRotator, Spawn);
        auto* Friend = Cast<ADivineBeastsCharacter>(FrozenTarget.Get());
        auto* FriendController = World->SpawnActor<APlayerController>();
        AlternateTarget = FriendController; // 由现有夹具统一销毁，不留下控制器/PlayerState。
        if (!Test->TestTrue(TEXT("真实友方角色/控制器存在"), Friend && FriendController)) return;
        FriendController->Possess(Friend); Friend->GetCharacterMovement()->DisableMovement();
        auto* Identity = Friend->FindComponentByClass<UDivineBeastsCharacterComponent>();
        FGamePlatformCharacterInitializationContext Context;
        Context.CharacterId = TEXT("Automation.PrimaryHealing.Friend"); Context.HeroDefinitionId = TEXT("Hero.Zodiac.Rat");
        Context.SpawnGeneration = 1; Context.AvatarGeneration = 1;
        FString Error;
        if (!Test->TestTrue(TEXT("友方真实身份初始化"), Identity->AuthorityBindTrustedContext(Context, Error) &&
            Identity->TryUsePreloadedDefinition(WarmupLease, *WarmupOwner.Get(), Error))) return;
        Friend->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>()->SetServerPlayerActive(true);
        auto* FriendASC = Friend->GetGamePlatformAbilitySystemComponent();
        auto* FriendCombat = Friend->GetGamePlatformCombatComponent();
        const float FriendMaximum = FriendCombat->GetCombatMaxHealth();
        Pawn->AuthorityFriendlyTargetQuery().BindLambda([WeakFriend = TWeakObjectPtr<AActor>(Friend)](const ADivineBeastsCharacter*, const AActor* Target)
        { return WeakFriend.IsValid() && Target == WeakFriend.Get(); });
        SetHealth(FriendASC, FriendMaximum * .5f);
        SourceCombat->AuthoritySelectProjectileTarget(Friend, 1, 1);
        Test->TestTrue(TEXT("选中可信友方时受理治疗"), ASC->TryActivateAbility(AbilityHandle, false));
        Test->TestEqual(TEXT("友方获得自身最大生命20%"), FriendCombat->GetCombatHealth(), FriendMaximum * .7f);
        Test->TestEqual(TEXT("治疗友方时自己生命不变"), SourceCombat->GetCombatHealth(), SourceMaximum * .7f);
        ClearCooldown(); SetHealth(FriendASC, FriendMaximum * .95f);
        ASC->TryActivateAbility(AbilityHandle, false);
        Test->TestEqual(TEXT("治疗不超过最大生命"), FriendCombat->GetCombatHealth(), FriendMaximum);
        ClearCooldown();

        // 未被权威关系认可的选中对象作为敌方；只能自疗，不能把它作为治疗目标。
        Pawn->AuthorityFriendlyTargetQuery().Unbind(); SetHealth(FriendASC, FriendMaximum * .5f);
        ASC->TryActivateAbility(AbilityHandle, false);
        Test->TestEqual(TEXT("选中敌方绝不治疗敌方"), FriendCombat->GetCombatHealth(), FriendMaximum * .5f);
        ClearCooldown(); SetHealth(ASC.Get(), SourceMaximum * .5f);
        Pawn->AuthorityFriendlyTargetQuery().BindLambda([WeakFriend = TWeakObjectPtr<AActor>(Friend)](const ADivineBeastsCharacter*, const AActor* Target)
        { return WeakFriend.IsValid() && Target == WeakFriend.Get(); });
        const auto VerifyRejectedWithoutSelfHealing = [&](const TCHAR* Label)
        {
            ASC->TryActivateAbility(AbilityHandle, false);
            Test->TestEqual(FString(Label) + TEXT("不悄悄自疗"), SourceCombat->GetCombatHealth(), SourceMaximum * .5f);
            Test->TestFalse(FString(Label) + TEXT("不扣冷却"), ASC->HasMatchingGameplayTag(CooldownTag));
        };
        Friend->SetActorLocation(FVector(2100, 0, 1000));
        VerifyRejectedWithoutSelfHealing(TEXT("友方超出2000厘米"));
        Friend->SetActorLocation(FVector(300, 0, 1000));
        FriendCombat->ResetForNewAvatar(2);
        VerifyRejectedWithoutSelfHealing(TEXT("选中友方已换化身"));
        // Combat单独换代不代表真实角色Gate也已授权新化身；本次重选必须拒绝且保留旧选择，
        // 否则后续死亡会被错当成“没有选择”并隐式自疗、扣除10秒冷却。
        Test->TestFalse(TEXT("角色Gate仍属旧代次时不接纳新化身选择"), SourceCombat->AuthoritySelectProjectileTarget(Friend, 1, 2));
        TWeakObjectPtr<AActor> RejectedSelectionTarget;
        int32 RejectedSourceGeneration = 0, RejectedTargetGeneration = 0;
        Test->TestTrue(TEXT("拒绝选择不清空先前友方意图"),
            SourceCombat->GetSelectedProjectileTargetIntent(RejectedSelectionTarget, RejectedSourceGeneration, RejectedTargetGeneration) &&
            RejectedSelectionTarget.Get() == Friend && RejectedSourceGeneration == 1 && RejectedTargetGeneration == 1);
        // 旧化身场景故意只推进Combat而未重绑角色身份；本段直接调用既有属性结算回调制造真实死亡，
        // 不用不匹配的身份强行通过攻击资格，也不直接写bDead来伪造治疗死亡保护。
        FGameplayEffectSpec DamageEffect;
        FriendCombat->ResolveIncomingDamage(*FriendCombat->GetCombatAttributeSet(), DamageEffect, FriendMaximum * 2.f);
        if (!Test->TestTrue(TEXT("友方已通过真实Combat结算死亡"), FriendCombat->IsCombatDead())) return;
        VerifyRejectedWithoutSelfHealing(TEXT("选中友方已死亡"));
        Test->TestEqual(TEXT("治疗不会复活死亡友方"), FriendCombat->GetCombatHealth(), 0.f);
        Pawn->AuthorityFriendlyTargetQuery().Unbind();
    }

    /** 已保存冰箭GA的真实起手时序：0.65秒无弹、0.75秒恰一枚，取消/来源换代不产生迟到发射。 */
    bool BeginFrostboltWindup()
    {
        UWorld* World = Instance->GetWorld();
        UClass* AbilityClass = bUsePlaceholder ? Cast<UClass>(FSoftObjectPath(TEXT("/DBAGameplay/Abilities/Placeholders/GA_DBA_Rat_PlaceholderFrostbolt.GA_DBA_Rat_PlaceholderFrostbolt_C")).ResolveObject()) : AbilityClasses[1];
        if (!Test->TestNotNull(TEXT("真实冰箭授权类"), AbilityClass)) return false;
        Pawn->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>()->SetServerPlayerActive(true);
        AbilityHandle = ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
        const auto* Ability = AbilityClass->GetDefaultObject<UGameplayAbility>();
        if (!Test->TestNotNull(TEXT("冰箭真实冷却"), Ability->GetCooldownGameplayEffect())) return false;
        WindupCooldownTags = *Ability->GetCooldownTags();
        SpawnHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateRaw(this, &FRealMagicCommand::ObserveProjectile));
        const bool bAccepted = Test->TestTrue(TEXT("服务器受理冰箭起手"), ASC->TryActivateAbility(AbilityHandle, false));
        Test->TestEqual(TEXT("激活当刻未提前发射"), Projectiles.Num(), 0);
        // 独立写明用户要求，不从待测常量反算；复制快照是施法条和动画的唯一事件输入。
        const auto* CastFacts = Pawn->FindComponentByClass<UGamePlatformAbilityCastComponent>();
        if (Test->TestNotNull(TEXT("冰箭真实复制施法组件"), CastFacts))
            Test->TestEqual(TEXT("冰箭复制施法时长严格2秒"), CastFacts->GetCastFacts().DurationSeconds, 2.0f);
        return bAccepted;
    }

    /** 各段由真实Automation帧推进；不篡改GFrameCounter或在同帧重复Tick绕过引擎Timer语义。 */
    bool CheckFrostboltWindupFrame()
    {
        if (FlightFrames == 39) Test->TestEqual(TEXT("起手1.95秒仍未发射"), Projectiles.Num(), 0);
        if (FlightFrames == 41)
        {
            Test->TestEqual(TEXT("起手2.05秒恰好一枚权威冰箭"), Projectiles.Num(), 1);
            ASC->RemoveActiveEffectsWithGrantedTags(WindupCooldownTags);
            Test->TestTrue(TEXT("新一轮冰箭起手受理"), ASC->TryActivateAbility(AbilityHandle, false));
            ASC->CancelAbilityHandle(AbilityHandle);
        }
        if (FlightFrames == 83)
        {
            Test->TestEqual(TEXT("取消起手没有迟到发射"), Projectiles.Num(), 1);
            ASC->RemoveActiveEffectsWithGrantedTags(WindupCooldownTags);
            Test->TestTrue(TEXT("换代前冰箭起手受理"), ASC->TryActivateAbility(AbilityHandle, false));
            Pawn->GetGamePlatformCombatComponent()->ResetForNewAvatar(2);
        }
        if (FlightFrames < 125) return false;
        Test->TestEqual(TEXT("来源换代没有迟到发射"), Projectiles.Num(), 1);
        return true;
    }

    /**
     * 真实Hero出生后直接通过平台输入释放三种魔法，不调用FillMomentum。
     * 漏改英雄资产的初始气势、输入未进入GAS或成本重复扣除都会使本用例失败。
     * 用户明确法术不消耗气势，三次施法均保持初始50；期望不由待测代码反算。
     */
    void CheckFreshSpawnInput()
    {
        const float SpawnMomentum = ASC->GetNumericAttribute(
            UDivineBeastsMomentumAttributeSet::GetMomentumAttribute());
        if (!Test->TestEqual(TEXT("真实英雄数据初始化50气势，不靠测试补满"), SpawnMomentum, 50.f)) { return; }
        // 其他服务器场景故意使用远端控制器；本用例验证本地输入，必须通过引擎SetPlayer绑定
        // 真实ULocalPlayer。不能修改ActorInfo私有标记或放宽平台“仅本地拥有者输入”门禁。
        // ULocalPlayer声明Within=Engine，必须以真实GEngine为Outer；GI不是合法容器。
        LocalInputPlayer.Reset(NewObject<ULocalPlayer>(GEngine));
        Controller->SetPlayer(LocalInputPlayer.Get());
        if (!Test->TestTrue(TEXT("输入夹具拥有真实本地玩家控制器"), Pawn->IsLocallyControlled())) { return; }
        auto* Eligibility = Pawn->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>();
        if (!Test->TestNotNull(TEXT("服务器准入资格组件存在"), Eligibility)) { return; }
        Eligibility->SetServerPlayerActive(true);
        const TCHAR* InputNames[] = {
            TEXT("Platform.Ability.Input.DivineBeasts.Slot1"),
            TEXT("Platform.Ability.Input.DivineBeasts.Slot2"),
            TEXT("Platform.Ability.Input.DivineBeasts.Slot4") };
        const float ExpectedMomentum[] = {50.f, 50.f, 50.f};
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedMagic); ++Index)
        {
            const FGameplayTag InputTag = FGameplayTag::RequestGameplayTag(FName(InputNames[Index]));
            FGameplayAbilitySpec Spec(AbilityClasses[Index], 1);
            Spec.GetDynamicSpecSourceTags().AddTag(InputTag);
            const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
            if (!Test->TestTrue(TEXT("真实GAS授予魔法"), Handle.IsValid())) { return; }
            const FGamePlatformResult InputResult = ASC->AbilityInputPressed(InputTag, ASC->GetInputToken());
            if (!Test->TestTrue(TEXT("出生资源允许接纳平台输入：") + InputResult.Code.ToString(),
                InputResult.IsSuccess())) { return; }
            // 零费用后属性不变不能证明输入真的施法；独立观察该Spec的原生Commit，
            // 并在同步输入处理后立即解绑，失败或潜伏连发均不会引用栈上计数。
            int32 InputCommitCount = 0;
            const FDelegateHandle InputCommitHandle = ASC->AbilityCommittedCallbacks.AddLambda(
                [Handle, &InputCommitCount](UGameplayAbility* Ability)
                { if (Ability->GetCurrentAbilitySpecHandle() == Handle) ++InputCommitCount; });
            ASC->ProcessAbilityInput();
            ASC->AbilityCommittedCallbacks.Remove(InputCommitHandle);
            ASC->AbilityInputReleased(InputTag, ASC->GetInputToken());
            Test->TestEqual(TEXT("每个真实输入确实提交一次对应法术"), InputCommitCount, 1);
            const auto* AbilityCDO = CastChecked<UDivineBeastsDevelopmentMagicAbility>(AbilityClasses[Index]->GetDefaultObject());
            Test->TestEqual(TEXT("每个已提交法术产生一份真实冷却"),
                ASC->GetGameplayEffectCount(AbilityCDO->GetCooldownGameplayEffect()->GetClass(), ASC.Get()), 1);
            Test->TestEqual(FString::Printf(TEXT("%s通过真实输入不消耗气势"), ExpectedMagic[Index].Name),
                ASC->GetNumericAttribute(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute()),
                ExpectedMomentum[Index]);
        }
        FString Error;
        // 独立模拟其他玩法改变资源，重复Hero通知不能重置为初始值。
        ASC->SetNumericAttributeBase(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute(), 7.f);
        auto* Identity = Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>();
        Test->TestTrue(TEXT("同一可信定义再次交接是幂等的"),
            Identity->TryUsePreloadedDefinition(WarmupLease, *WarmupOwner.Get(), Error));
        Test->TestEqual(TEXT("重复定义通知不把其他玩法的7气势重置为50"),
            ASC->GetNumericAttribute(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute()), 7.f);
    }

    /** 初始化其他隔离场景的测试资源；真实出生回归不得调用本方法掩盖初始数据缺陷。 */
    bool FillMomentum()
    {
        const auto* Momentum = ASC->GetSet<UDivineBeastsMomentumAttributeSet>();
        if (!Test->TestNotNull(TEXT("真实Hero初始化的Momentum属性集"), Momentum) ||
            !Test->TestTrue(TEXT("真实Hero气势上限合法且保留其他玩法资源"), Momentum->GetMaxMomentum() > 0.f)) { return false; }
        ASC->SetNumericAttributeBase(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute(), Momentum->GetMaxMomentum());
        InitialMomentum = ASC->GetNumericAttribute(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute());
        return true;
    }

    /** 独立执行三对已保存GE；检测错误属性、错误幅值、漏授冷却标签、错误持续时间和移除残留。 */
    void CheckGameplayEffects()
    {
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedMagic); ++Index)
        {
            if (!FillMomentum()) { return; }
            const auto& Expected = ExpectedMagic[Index];
            const auto* Ability = CastChecked<UDivineBeastsDevelopmentMagicAbility>(AbilityClasses[Index]->GetDefaultObject());
            const FString Label = FString::Printf(TEXT("%s真实GAS："), Expected.Name);
            auto* LegacyCostClass = CastChecked<UClass>(MagicPath(TEXT("GE"), Expected, TEXT("Cost"), true).ResolveObject());
            const auto CostSpec = ASC->MakeOutgoingSpec(LegacyCostClass, 1.f, ASC->MakeEffectContext());
            const auto CooldownSpec = ASC->MakeOutgoingSpec(Ability->GetCooldownGameplayEffect()->GetClass(), 1.f, ASC->MakeEffectContext());
            if (!Test->TestTrue(Label + TEXT("两份真实GE Spec有效"), CostSpec.IsValid() && CooldownSpec.IsValid())) { return; }
            // 瞬时GE的返回句柄可以无效；必须以实际属性副作用证明执行，不能仅断言句柄。
            ASC->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());
            Test->TestEqual(Label + TEXT("保留旧GE身份但不修改气势"),
                ASC->GetNumericAttribute(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute()), InitialMomentum - Expected.Cost);
            const auto ActiveHandle = ASC->ApplyGameplayEffectSpecToSelf(*CooldownSpec.Data.Get());
            const auto* Active = ASC->GetActiveGameplayEffect(ActiveHandle);
            if (!Test->TestNotNull(Label + TEXT("真实冷却进入ActiveEffects"), Active)) { return; }
            Test->TestEqual(Label + TEXT("实际冷却持续秒数"), Active->GetDuration(), Expected.Cooldown);
            const auto Tag = FGameplayTag::RequestGameplayTag(FName(Expected.CooldownTag), false);
            Test->TestTrue(Label + TEXT("GE本身实际授予冷却标签"), ASC->HasMatchingGameplayTag(Tag));
            Test->TestEqual(Label + TEXT("本技能只有一份活跃冷却"),
                ASC->GetGameplayEffectCount(Ability->GetCooldownGameplayEffect()->GetClass(), ASC.Get()), 1);
            Test->TestTrue(Label + TEXT("实际冷却可以撤销"), ASC->RemoveActiveGameplayEffect(ActiveHandle));
            Test->TestFalse(Label + TEXT("移除冷却后标签无残留"), ASC->HasMatchingGameplayTag(Tag));
        }
    }

    /** 真实Spawn回调观察器：在AuthorityInitialize前订阅事实，取消/后继场景也走公开GAS入口。 */
    void ObserveProjectile(AActor* Actor)
    {
        auto* Projectile = Cast<AGamePlatformCombatProjectile>(Actor);
        if (!Projectile || Projectile->GetOwner() != Pawn.Get()) { return; }
        const int32 Index = Projectiles.AddDefaulted();
        Projectiles[Index].Actor = Projectile; Projectiles[Index].Origin = Projectile->GetActorLocation();
        Projectiles[Index].FactsHandle = Projectile->OnFlightFactsChanged().AddLambda(
            [this, Index](AGamePlatformCombatProjectile* SourceProjectile, const FGamePlatformCombatProjectileFacts& Facts)
            {
                Projectiles[Index].Facts = Facts;
                if (Facts.bStarted && Projectiles[Index].StartedWorldSeconds<0)
                    Projectiles[Index].StartedWorldSeconds=SourceProjectile->GetWorld()->GetTimeSeconds();
            });
        if (Scenario == EScenario::SpawnStaleSource && !bReentered)
        {
            // Spawn尚未返回时推进真实Combat代次，但不取消技能；生产路径必须自行收敛为取消终态。
            bReentered = true;
            Test->TestTrue(TEXT("Spawn回调真实推进来源代次"), Pawn->GetGamePlatformCombatComponent()->ResetForNewAvatar(2));
        }
        if (Scenario == EScenario::SpawnSuccessor && !bReentered)
        {
            // 原Spawn栈尚未返回时结束它，移除它自己的真实冷却，再同步激活同一个实例。
            // 不退款、不换资产、不写令牌；旧栈不得初始化刚生成的旧弹体或清掉后继连发。
            bReentered = true;
            ASC->CancelAbilityHandle(AbilityHandle);
            RemoveArcaneCooldown();
            bSuccessorAccepted = ASC->TryActivateAbility(AbilityHandle, false);
        }
    }

    /** 本组目标回归共享真实三发入口；只改变选择或目标生命周期，不调整技能状态/令牌/计时器。 */
    bool IsTargetBurstScenario() const
    {
        return Scenario == EScenario::TargetSelectionSwitch || Scenario == EScenario::TargetDestroyedGC ||
            Scenario == EScenario::TargetAvatarChanged;
    }

    /** 带真实ASC/Combat和球体的同世界目标；普通Actor不伪造项目Hero或玩家Ready，战斗资格由平台组件核验。 */
    bool InitializeBurstTargets()
    {
        UWorld* World = Instance->GetWorld();
        const auto SpawnTarget = [World](const FVector& Location) -> AActor*
        {
            AActor* Target = World->SpawnActor<AActor>();
            if (!Target) return nullptr;
            auto* Sphere = NewObject<USphereComponent>(Target);
            Sphere->InitSphereRadius(30.f); Target->SetRootComponent(Sphere); Target->AddInstanceComponent(Sphere);
            Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Sphere->SetCollisionResponseToAllChannels(ECR_Block);
            Sphere->RegisterComponent(); Target->SetActorLocation(Location);
            auto* TargetASC = NewObject<UGamePlatformAbilitySystemComponent>(Target);
            auto* Combat = NewObject<UGamePlatformCombatComponent>(Target);
            Target->AddInstanceComponent(TargetASC); Target->AddInstanceComponent(Combat);
            TargetASC->RegisterComponent(); Combat->RegisterComponent(); TargetASC->InitAbilityActorInfo(Target, Target);
            return Target;
        };
        // 两目标分居前方左右；0.6秒观测期间距首目标仍足够远，避免自然命中掩盖连发选择错误。
        FrozenTarget = SpawnTarget(FVector(1000, 500, 1000));
        AlternateTarget = SpawnTarget(FVector(1000, -500, 1000));
        if (!Test->TestTrue(TEXT("两名真实同世界战斗目标"), FrozenTarget.IsValid() && AlternateTarget.IsValid())) return false;
        auto* Combat = Pawn->GetGamePlatformCombatComponent();
        auto* TargetCombat = FrozenTarget->FindComponentByClass<UGamePlatformCombatComponent>();
        return Test->TestTrue(TEXT("激活前显式冻结第一目标选择"), TargetCombat && Combat->AuthoritySelectProjectileTarget(
            FrozenTarget.Get(), Combat->GetCombatAvatarGeneration(), TargetCombat->GetCombatAvatarGeneration()));
    }

    /** 实际GAS分别验证50米外不Commit/不出弹、50米边界可施法；开发和正式占位GA共用夹具，不能只查表。 */
    bool BeginSelectedTargetRange()
    {
        auto* Eligibility = Pawn->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>();
        if (!Eligibility) { Test->AddError(TEXT("缺少真实资格组件")); return false; }
        Eligibility->SetServerPlayerActive(true);
        if (!FillMomentum() || !InitializeBurstTargets()) return false;
        UClass* AbilityClass = bUsePlaceholder ? PlaceholderArcaneClass : AbilityClasses[2];
        AbilityHandle = ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, Pawn.Get()));
        CommitHandle = ASC->AbilityCommittedCallbacks.AddLambda([this](UGameplayAbility* Ability)
        { if (Ability->GetCurrentAbilitySpecHandle() == AbilityHandle) ++CommitCount; });
        SpawnHandle = Instance->GetWorld()->AddOnActorSpawnedHandler(
            FOnActorSpawned::FDelegate::CreateRaw(this, &FRealMagicCommand::ObserveProjectile));
        auto* Combat = Pawn->GetGamePlatformCombatComponent();
        const FVector Center = Combat->GetCombatTargetPoint();
        FrozenTarget->SetActorLocation(Center + FVector(0, 5000.1, 0));
        ASC->TryActivateAbility(AbilityHandle, false); // GAS可接纳调用后由服务器技能取消，不能只断言TryActivate返回值。
        Test->TestEqual(TEXT("超过50米的已选目标不消耗冷却Commit"), CommitCount, 0);
        Test->TestEqual(TEXT("超过50米不能丢失目标后改成直射"), Projectiles.Num(), 0);
        FrozenTarget->SetActorLocation(Center + FVector(0, 5000, 0));
        Test->TestTrue(TEXT("50米边界真实GAS仍可施法"), ASC->TryActivateAbility(AbilityHandle, false));
        Test->TestEqual(TEXT("合法目标只Commit一次"), CommitCount, 1);
        Test->TestEqual(TEXT("边界首枚真实释放"), Projectiles.Num(), 1);
        return true;
    }

    /** 跨12个真实帧后检查实际Timer连发及飞行位置；不手改GFrameCounter或能力私有状态。 */
    void CheckSelectedTargetRangeTerminal()
    {
        Test->TestEqual(TEXT("边界完整三发仍冻结原目标"), Projectiles.Num(), 3);
        for (const auto& Shot : Projectiles)
        {
            Test->TestTrue(TEXT("每枚经过服务器初始化而非空壳"), Shot.Facts.bStarted);
            Test->TestTrue(TEXT("沿角色前向出射后向侧方目标弯曲"), Shot.Actor.IsValid() &&
                Shot.Actor->GetActorLocation().X > Shot.Origin.X && Shot.Actor->GetActorLocation().Y > Shot.Origin.Y);
        }
    }

    /** 第一枚已通过真实AuthorityInitialize后才变更外部事实；Destroy后同步完整GC会揭示跨Timer悬空引用。 */
    bool MutateTargetAfterFirstShot()
    {
        if (!Test->TestEqual(TEXT("改变目标前只释放了真实首枚"), Projectiles.Num(), 1) ||
            !Test->TestTrue(TEXT("首枚已真实开始飞行"), Projectiles[0].Facts.bStarted)) return false;
        if (Scenario == EScenario::TargetSelectionSwitch)
        {
            auto* Combat = Pawn->GetGamePlatformCombatComponent();
            auto* NextCombat = AlternateTarget->FindComponentByClass<UGamePlatformCombatComponent>();
            if (!Test->TestTrue(TEXT("第一枚后明确改选另一目标"), Combat->AuthoritySelectProjectileTarget(
                AlternateTarget.Get(), Combat->GetCombatAvatarGeneration(), NextCombat->GetCombatAvatarGeneration()))) return false;
            int32 Generation = 0;
            return Test->TestTrue(TEXT("服务器当前选择确已变为另一目标"),
                Combat->GetSelectedProjectileTarget(2000.f, Generation) == AlternateTarget.Get() && Generation == 1);
        }
        if (Scenario == EScenario::TargetDestroyedGC)
        {
            const TWeakObjectPtr<AActor> DestroyedTarget = FrozenTarget;
            if (!Test->TestTrue(TEXT("第一枚后真实Destroy冻结目标"), FrozenTarget->Destroy())) return false;
            // 不保留目标强引用；GC必须回收它，而不仅是IsValid因PendingKill返回false。
            CollectGarbage(RF_NoFlags, true);
            return Test->TestFalse(TEXT("已销毁目标完成真实GC而非只打销毁标记"), DestroyedTarget.IsValid(true, true));
        }
        auto* TargetCombat = FrozenTarget->FindComponentByClass<UGamePlatformCombatComponent>();
        return Test->TestTrue(TEXT("同Actor真实推进目标化身代次"), TargetCombat && TargetCombat->ResetForNewAvatar(2)) &&
            Test->TestEqual(TEXT("目标Actor保留且当前代次为2"), TargetCombat->GetCombatAvatarGeneration(), 2);
    }

    /** 计数、真实飞行方向和终态交叉验证；不能仅看到三次Spawn便把改选后的错误追踪记为通过。 */
    void CheckTargetBurstTerminal()
    {
        const bool bSwitchedSelection = Scenario == EScenario::TargetSelectionSwitch;
        const int32 ExpectedShots = bSwitchedSelection ? 3 : 1;
        Test->TestEqual(TEXT("目标变化不额外Commit"), CommitCount, 1);
        Test->TestEqual(TEXT("本次施法只应用原有一份冷却"), CooldownApplications, 1);
        // 两类真实GA均将三种目标路径的终态气势与本次激活前的真实初值比较，不使用固定资源值。
        Test->TestEqual(TEXT("真实连发及目标失效终态均保持原始气势"),
            ASC->GetNumericAttribute(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute()), InitialMomentum);
        const auto* Spec = ASC->FindAbilitySpecFromHandle(AbilityHandle);
        const auto* Ability = Spec ? Spec->GetPrimaryInstance() : nullptr;
        if (!Test->TestNotNull(TEXT("目标生命周期回归保留真实能力实例"), Ability)) return;
        Test->TestFalse(TEXT("完成或目标失效后没有迟到连发激活"), Ability->IsActive());
        Test->TestEqual(TEXT("目标失效在下一次Spawn前终止，改选则仍完整三发"), Projectiles.Num(), ExpectedShots);
        for (const auto& Observed : Projectiles)
        {
            Test->TestTrue(TEXT("已观察的弹体均通过真实初始化"), Observed.Facts.bStarted);
            if (bSwitchedSelection)
            {
                Test->TestTrue(TEXT("每枚均从角色前方起飞，不能将原目标方向当作出生切线"),
                    Observed.Origin.X > Pawn->GetActorLocation().X + 40.f &&
                    FMath::Abs(Observed.Origin.Y - Pawn->GetActorLocation().Y) < 1.f);
                if (!Test->TestTrue(TEXT("观测窗口内原目标弹体仍存活"), Observed.Actor.IsValid())) continue;
                Test->TestTrue(TEXT("后续真实飞行仍向原目标，不追踪后来右侧选择"),
                    Observed.Actor->GetActorLocation().Y > Observed.Origin.Y + 5.f);
            }
            else
            {
                Test->TestTrue(TEXT("目标销毁或换代也结束已发射弹体"), Observed.Facts.bFinished);
                Test->TestFalse(TEXT("目标失效不冒充命中"), Observed.Facts.bImpacted);
            }
        }
    }

    /** 仅撤销夹具ASC内当前奥术GE，给同步后继激活创造合法前提，不改变其他技能或外部世界。 */
    void RemoveArcaneCooldown()
    {
        const auto* Ability = CastChecked<UDivineBeastsDevelopmentMagicAbility>(AbilityClasses[2]->GetDefaultObject());
        for (const auto Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
        {
            const auto* Effect = ASC->GetActiveGameplayEffect(Handle);
            if (Effect && Effect->Spec.Def->GetClass() == Ability->GetCooldownGameplayEffect()->GetClass())
            { Test->TestTrue(TEXT("取消后只撤销夹具旧奥术冷却"), ASC->RemoveActiveGameplayEffect(Handle)); }
        }
    }

    /** 原生GiveAbility/TryActivate执行真实服务器GA；观察Commit与实际GE应用，不直接调用Activate或私有释放函数。 */
    bool BeginRealActivation()
    {
        auto* Eligibility = Pawn->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>();
        if (!Test->TestNotNull(TEXT("真实中立Gameplay资格组件"), Eligibility) || !FillMomentum()) { return false; }
        // 隔离Engine.GameModeBase宿主复用既有非Experience中立资格分支；此调用不是生产后端准入证据。
        Eligibility->SetServerPlayerActive(true);
        const auto Gate = ASC->EvaluateActivationEligibility();
        if (!Test->TestTrue(TEXT("真实项目激活Gate通过"), Gate.IsSuccess()))
        { Test->AddError(FString::Printf(TEXT("项目Gate拒绝：%s"), *Gate.Code.ToString())); return false; }
        if (!bUsePlaceholder && !Test->TestTrue(TEXT("真实进程满足开发技能配置和命令行资格"),
            UDivineBeastsDevelopmentMagicDefinition::IsDevelopmentEnabledForCurrentProcess())) { return false; }
        UClass* ArcaneClass = bUsePlaceholder ? PlaceholderArcaneClass : AbilityClasses[2];
        const auto* AbilityCDO = ArcaneClass->GetDefaultObject<UGameplayAbility>();
        CostClass = bUsePlaceholder ? (AbilityCDO->GetCostGameplayEffect() ? AbilityCDO->GetCostGameplayEffect()->GetClass() : nullptr) :
            CastChecked<UClass>(MagicPath(TEXT("GE"), ExpectedMagic[2], TEXT("Cost"), true).ResolveObject());
        CooldownClass = AbilityCDO->GetCooldownGameplayEffect()->GetClass();
        OtherAvatar = Instance->GetWorld()->SpawnActor<AActor>();
        if (!Test->TestTrue(TEXT("换Avatar回归的真实后继Actor"), OtherAvatar.IsValid())) { return false; }
        if (IsTargetBurstScenario() && !InitializeBurstTargets()) return false;
        AbilityHandle = ASC->GiveAbility(FGameplayAbilitySpec(ArcaneClass, 1, INDEX_NONE, Pawn.Get()));
        if (!Test->TestTrue(TEXT("真实奥术GA Spec授予成功"), AbilityHandle.IsValid())) { return false; }
        CommitHandle = ASC->AbilityCommittedCallbacks.AddLambda([this](UGameplayAbility* Ability)
        {
            if (Ability->GetCurrentAbilitySpecHandle() != AbilityHandle) { return; }
            ++CommitCount;
            if (Scenario == EScenario::CommitAvatarSwap && !bReentered)
            {
                bReentered = true;
                Test->TestTrue(TEXT("真实Commit同步回调更换Avatar"), ASC->BindAbilityActorInfo(Pawn.Get(), OtherAvatar.Get()));
            }
        });
        EffectHandle = ASC->OnGameplayEffectAppliedDelegateToSelf.AddLambda(
            [this](UAbilitySystemComponent*, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
            {
                if (Spec.Def->GetClass() == CostClass) { ++CostApplications; }
                if (Spec.Def->GetClass() == CooldownClass) { ++CooldownApplications; }
            });
        SpawnHandle = Instance->GetWorld()->AddOnActorSpawnedHandler(
            FOnActorSpawned::FDelegate::CreateRaw(this, &FRealMagicCommand::ObserveProjectile));
        if (Scenario == EScenario::ZeroMomentum)
        {
            InitialMomentum = 0.f;
            ASC->SetNumericAttributeBase(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute(), 0.f);
        }
        if (!Test->TestTrue(TEXT("真实服务器GAS接纳奥术激活"), ASC->TryActivateAbility(AbilityHandle, false))) { return false; }
        auto* Spec = ASC->FindAbilitySpecFromHandle(AbilityHandle);
        auto* InstanceAbility = Spec ? Spec->GetPrimaryInstance() : nullptr;
        if (!Test->TestNotNull(TEXT("真实InstancedPerActor魔法实例"), InstanceAbility)) { return false; }
        if (Scenario == EScenario::CommitAvatarSwap || Scenario == EScenario::SpawnStaleSource)
        {
            Test->TestTrue(TEXT("Commit回调已实际发生"), bReentered);
            if (Scenario == EScenario::CommitAvatarSwap)
            { Test->TestTrue(TEXT("ASC当前Avatar确为后继"), ASC->GetAvatarActor() == OtherAvatar.Get()); }
            Test->TestFalse(TEXT("Commit返回发现Avatar变化立即结束"), InstanceAbility->IsActive());
            Test->TestEqual(TEXT("失效来源不新增弹体"), Projectiles.Num(), Scenario == EScenario::SpawnStaleSource ? 1 : 0);
        }
        else
        {
            Test->TestTrue(TEXT("首枚释放后技能仍持有真实连发激活"), InstanceAbility->IsActive());
            if (Scenario == EScenario::Cancel) { ASC->CancelAbilityHandle(AbilityHandle); }
            if (Scenario == EScenario::StaleSource)
            {
                auto* Combat = Pawn->GetGamePlatformCombatComponent();
                Test->TestTrue(TEXT("连发中真实来源Combat进入后继代次"), Combat->ResetForNewAvatar(2));
                Test->TestEqual(TEXT("来源Combat代次确实为2"), Combat->GetCombatAvatarGeneration(), 2);
            }
            if (IsTargetBurstScenario() && !MutateTargetAfterFirstShot()) return false;
        }
        return true;
    }

    /** 对真实委托计数、属性、ActiveEffects、弹体终态和GAS激活状态做交叉断言，捕捉多Commit/迟到连发。 */
    void CheckTerminalBehavior()
    {
        if (IsTargetBurstScenario()) { CheckTargetBurstTerminal(); return; }
        const bool bSuccessor = Scenario == EScenario::SpawnSuccessor;
        const int32 ExpectedCommits = bSuccessor ? 2 : 1;
        Test->TestEqual(TEXT("每次GAS激活只Commit一次"), CommitCount, ExpectedCommits);
        Test->TestEqual(TEXT("法术Commit不应用气势成本GE"), CostApplications, 0);
        Test->TestEqual(TEXT("每次Commit只应用一份真实冷却GE"), CooldownApplications, ExpectedCommits);
        Test->TestEqual(TEXT("连发、取消、换Avatar及后继施法均不改变气势"),
            ASC->GetNumericAttribute(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute()), InitialMomentum);
        Test->TestEqual(TEXT("当前激活只有一份真实冷却"), ASC->GetGameplayEffectCount(CooldownClass, ASC.Get()), 1);
        const auto* Spec = ASC->FindAbilitySpecFromHandle(AbilityHandle);
        const auto* Ability = Spec ? Spec->GetPrimaryInstance() : nullptr;
        if (!Test->TestNotNull(TEXT("结束检查仍有真实能力实例"), Ability)) { return; }
        Test->TestFalse(TEXT("完成/取消/失效后无残留激活"), Ability->IsActive());
        const int32 ExpectedStarted = (Scenario == EScenario::CommitAvatarSwap || Scenario == EScenario::SpawnStaleSource) ? 0 :
            (Scenario == EScenario::Cancel || Scenario == EScenario::StaleSource ? 1 : 3);
        TSet<FGuid> Occurrences;
        int32 StartedCount = 0;
        for (const auto& Observed : Projectiles)
        {
            if (!Observed.Facts.bStarted) { continue; }
            ++StartedCount;
            Test->TestTrue(TEXT("真实弹体具有有效独立发生Guid"), Observed.Facts.OccurrenceId.IsValid());
            Test->TestFalse(TEXT("每枚真实弹体Guid均不重复"), Occurrences.Contains(Observed.Facts.OccurrenceId));
            Occurrences.Add(Observed.Facts.OccurrenceId);
            Test->TestEqual(TEXT("真实弹体保留原可信来源代次"), Observed.Facts.SourceAvatarGeneration, 1);
            Test->TestEqual(TEXT("真实弹体关联奥术逻辑身份"), Observed.Facts.AbilityId, FName(TEXT("dba.ability.rat_dev_arcanemissiles@1")));
            Test->TestEqual(TEXT("真实弹体发布奥术表现语义"), Observed.Facts.PresentationStyleId, FName(TEXT("DBA.Development.ArcaneMissiles")));
            if (Scenario == EScenario::StaleSource)
            {
                Test->TestTrue(TEXT("已发射旧代次弹体也终止飞行"), Observed.Facts.bFinished);
                Test->TestFalse(TEXT("来源失效不得伪造命中事实"), Observed.Facts.bImpacted);
            }
        }
        Test->TestEqual(TEXT("真正AuthorityInitialize的弹数符合场景"), StartedCount, ExpectedStarted);
        Test->TestEqual(TEXT("Spawn次数不含意外迟到连发"), Projectiles.Num(), ExpectedStarted + ((bSuccessor || Scenario == EScenario::SpawnStaleSource) ? 1 : 0));
        if (Scenario == EScenario::SpawnStaleSource && !Projectiles.IsEmpty())
        {
            Test->TestFalse(TEXT("Spawn失效的旧弹体未初始化"), Projectiles[0].Facts.bStarted);
            Test->TestTrue(TEXT("Spawn失效的旧弹体已撤销"), !Projectiles[0].Actor.IsValid() || Projectiles[0].Actor->IsActorBeingDestroyed());
        }
        if (bSuccessor)
        {
            Test->TestTrue(TEXT("真实Spawn回调确实取消并激活后继"), bReentered && bSuccessorAccepted);
            if (!Projectiles.IsEmpty())
            {
                Test->TestFalse(TEXT("返回旧Spawn栈的旧弹体没有被初始化"), Projectiles[0].Facts.bStarted);
                Test->TestTrue(TEXT("旧Spawn栈撤销自己的旧弹体"),
                    !Projectiles[0].Actor.IsValid() || Projectiles[0].Actor->IsActorBeingDestroyed());
            }
        }
        if (Scenario == EScenario::Burst || Scenario == EScenario::ZeroMomentum || bSuccessor)
        {
            TArray<double> StartTimes;
            for (const auto& Observed : Projectiles) { if (Observed.StartedWorldSeconds>=0) StartTimes.Add(Observed.StartedWorldSeconds); }
            if (Test->TestEqual(TEXT("每枚弹体都有真实开始时刻"),StartTimes.Num(),3))
            {
                for (int32 Index=1;Index<StartTimes.Num();++Index)
                {
                    const double Interval=StartTimes[Index]-StartTimes[Index-1];
                    Test->TestTrue(TEXT("真实间隔不少于0.2秒且至多一帧量化"),Interval>=0.1999 && Interval<=0.2501);
                }
            }
            // 完整连发已结束，此时拒绝再激活应来自真实冷却，而不是同一实例仍Active。
            Test->TestFalse(TEXT("真实3秒冷却仍阻止第二次施法"), ASC->TryActivateAbility(AbilityHandle, false));
            Test->TestEqual(TEXT("冷却拒绝不会新增Commit"), CommitCount, ExpectedCommits);
        }
    }

    FAutomationTestBase* Test;       // Automation拥有，潜伏命令期间有效，不持久化或跨线程访问。
    EScenario Scenario;              // 本次用例唯一行为，不能在失败后退回另一种场景。
    bool bUsePlaceholder = false;     // 只选择已保存的真实另一执行器，不修改生成类或运行时CDO。
    double StartedSeconds;           // 墙钟秒，用于真实资源/初始化超时。
    TStrongObjectPtr<UGameInstance> Instance;
    TStrongObjectPtr<ULocalPlayer> LocalInputPlayer; // 仅输入回归拥有；与Controller同一夹具销毁，不触发身份认证。
    TWeakObjectPtr<ADivineBeastsCharacter> Pawn;
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<AActor> WarmupOwner, OtherAvatar;
    TWeakObjectPtr<AActor> FrozenTarget, AlternateTarget; // 测试不延长被Destroy目标的GC寿命。
    TWeakObjectPtr<UGamePlatformAbilitySystemComponent> ASC;
    FGamePlatformDataLease WarmupLease; // 测试只释放本GI、本Owner签发的资源需求。
    UClass* AbilityClasses[3] = {};     // 只借用成功租约中的真实生成类，销毁角色前租约不得释放。
    UClass* PlaceholderArcaneClass = nullptr; // 独立占位租约中的真实蓝图类，仅本用例借用。
    UClass* CostClass = nullptr;
    UClass* CooldownClass = nullptr;
    FGameplayAbilitySpecHandle AbilityHandle;
    FGameplayTagContainer WindupCooldownTags; // 冰箭场景借用的真实冷却标签值，跨潜伏帧不持有Ability/CDO裸引用。
    FDelegateHandle SpawnHandle, CommitHandle, EffectHandle;
    TArray<FObservedProjectile> Projectiles;
    float InitialMomentum = 0;         // 真实ASC起始属性值；所有法术的费用期望均为零。
    int32 CommitCount = 0, CostApplications = 0, CooldownApplications = 0, FlightFrames = 0;
    bool bAssetsChecked = false, bCastStarted = false, bReentered = false, bSuccessorAccepted = false;
};
}

// 真实Definition/表/GA/GE身份错配、参数偏移或非法等级被接纳时，本合同用例失败。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicAssetContractsTest,
    "DivineBeasts.Abilities.DevelopmentMagic.RealAssets.DefinitionBalanceGA",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicAssetContractsTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::AssetContracts)); return true;
}

// 旧成本GE改为无修改以兼容稳定资产身份；实际冷却必须授予标签、持续正确且可清理。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicGameplayEffectsTest,
    "DivineBeasts.Abilities.DevelopmentMagic.RealAssets.GASCostCooldown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicGameplayEffectsTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::GameplayEffects)); return true;
}

// 多Commit、多扣费、连发弹数/Guid错误、实际运动退化为直线或结束后绕过冷却时失败。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicArcaneBurstTest,
    "DivineBeasts.Abilities.DevelopmentMagic.ServerGAS.ArcaneBurstSingleCommitThreeGuids",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicArcaneBurstTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::Burst)); return true;
}

// 首枚之后走真实GAS取消；清理遗漏导致0.2秒后的余弹继续生成时失败，不要求取消已发射弹体。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicCancelBurstTest,
    "DivineBeasts.Abilities.DevelopmentMagic.ServerGAS.CancelStopsPendingBurst",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicCancelBurstTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::Cancel)); return true;
}

// 首枚之后真实Combat推进代次；旧施法继续释放或旧弹体继续飞行/发布命中事实时失败。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicStaleSourceTest,
    "DivineBeasts.Abilities.DevelopmentMagic.ServerGAS.StaleSourceStopsBurstAndFlight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicStaleSourceTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::StaleSource)); return true;
}

// 成本/冷却已提交后的原生Commit委托同步换Avatar；沿用旧资格Spawn或残留Active时失败。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicCommitAvatarSwapTest,
    "DivineBeasts.Abilities.DevelopmentMagic.ServerGAS.CommitCallbackAvatarSwapRejectsSpawn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicCommitAvatarSwapTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::CommitAvatarSwap)); return true;
}

// 原Spawn同步回调取消/重激活同实例；旧栈污染新施法计数、定时器或旧弹体时失败。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicSpawnSuccessorTest,
    "DivineBeasts.Abilities.DevelopmentMagic.ServerGAS.SpawnCallbackSuccessorKeepsOwnBurst",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicSpawnSuccessorTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::SpawnSuccessor)); return true;
}

// 真实服务器零气势仍须发出完整三弹；冷却保留，任何气势扣减或成本GE应用均失败。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicZeroMomentumTest,
    "DivineBeasts.Abilities.DevelopmentMagic.ServerGAS.ZeroMomentumCanCastAndKeepsCooldown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicZeroMomentumTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::ZeroMomentum)); return true;
}

// Spawn同步回调改变来源代次，但未显式Cancel；技能必须自行结束，不得残留Active或迟到Timer。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicSpawnStaleSourceTest,
    "DivineBeasts.Abilities.DevelopmentMagic.ServerGAS.SpawnCallbackStaleSourceEndsCast",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicSpawnStaleSourceTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::SpawnStaleSource)); return true;
}

// 真实出生初始50、平台输入成功且三个法术均不改变气势，不在测试中补充初始资源。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicFreshSpawnInputTest,
    "DivineBeasts.Abilities.DevelopmentMagic.ServerGAS.FreshSpawnCanCastAllThreeWithoutTestRefill",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicFreshSpawnInputTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::FreshSpawnInput)); return true;
}

// 以下六项使用同一真实资源/GAS夹具；每项独立世界，不共享目标、冷却、租约或施法实例。
// 第一发后改选必须仍向旧目标完整连发；目标销毁并GC或同Actor换化身则在下一次Spawn前终止。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicFrozenBurstSelectionTest,
    "DivineBeasts.Abilities.DevelopmentMagic.ServerGAS.TargetBurst.SelectionFrozen",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicFrozenBurstSelectionTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::TargetSelectionSwitch)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicDestroyedTargetBurstTest,
    "DivineBeasts.Abilities.DevelopmentMagic.ServerGAS.TargetBurst.DestroyAndGCStopsBurst",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicDestroyedTargetBurstTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::TargetDestroyedGC)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicTargetAvatarBurstTest,
    "DivineBeasts.Abilities.DevelopmentMagic.ServerGAS.TargetBurst.TargetAvatarChangeStopsBurst",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicTargetAvatarBurstTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::TargetAvatarChanged)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPlaceholderFrozenBurstSelectionTest,
    "DivineBeasts.Abilities.Placeholder.ServerGAS.TargetBurst.SelectionFrozen",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPlaceholderFrozenBurstSelectionTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::TargetSelectionSwitch, true)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPlaceholderDestroyedTargetBurstTest,
    "DivineBeasts.Abilities.Placeholder.ServerGAS.TargetBurst.DestroyAndGCStopsBurst",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPlaceholderDestroyedTargetBurstTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::TargetDestroyedGC, true)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPlaceholderTargetAvatarBurstTest,
    "DivineBeasts.Abilities.Placeholder.ServerGAS.TargetBurst.TargetAvatarChangeStopsBurst",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPlaceholderTargetAvatarBurstTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::TargetAvatarChanged, true)); return true;
}

// Primary真实资产必须已完成显式迁移；缺资产或模式仍为普攻时失败，不以跳过冒充治疗通过。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPlaceholderPrimaryHealingTest,
    "DivineBeasts.Abilities.Placeholder.ServerGAS.PrimaryHealing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPlaceholderPrimaryHealingTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::Healing, true)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentFrostboltWindupTest,
    "DivineBeasts.Abilities.DevelopmentMagic.ServerGAS.FrostboltWindup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentFrostboltWindupTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::FrostboltWindup)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPlaceholderFrostboltWindupTest,
    "DivineBeasts.Abilities.Placeholder.ServerGAS.FrostboltWindup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPlaceholderFrostboltWindupTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::FrostboltWindup, true)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPlaceholderTargetRangeTest,
    "DivineBeasts.Abilities.Placeholder.ServerGAS.SelectedTargetFiftyMetres",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPlaceholderTargetRangeTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::TargetRange, true)); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentTargetRangeTest,
    "DivineBeasts.Abilities.DevelopmentMagic.ServerGAS.SelectedTargetFiftyMetres",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentTargetRangeTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::TargetRange)); return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
