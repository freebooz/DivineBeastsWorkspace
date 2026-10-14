/**
 * 神兽联盟开发魔法的原生资源与服务器 GAS 回归，属于 DBAGameplay 项目层。
 * 调用方为 Editor 自动化；仅在游戏线程创建隔离 GI/World，使用正式 Data 租约、
 * 真实子鼠 Hero/魔法 Definition、数值表、GA 和 GE，不创建测试 UCLASS 或修改资产/CDO。
 * 夹具通过现有可信身份初始化与中立 GameplayEligibility 接入服务器技能资格；
 * 这里不验证后端准入、联网复制、目标伤害结算、客户端 VFX 或 Cook。
 * 连发测试要求启动时已有 bAllowDevelopmentAbilitySets=true 和 -DBADevelopmentSkills；
 * 缺资源、标签、角色 Ready 或开发资格均报错，不能以跳过或默认成功替代。
 * 测试所有者先解绑观察回调、取消技能并销毁借用角色，再释放自身租约和世界。
 */
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Abilities/DivineBeastsDevelopmentMagicAbility.h"
#include "Attributes/DivineBeastsMomentumAttributeSet.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Definitions/DivineBeastsDevelopmentMagicDefinition.h"
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
        EDivineBeastsDevelopmentMagicKind::Fireball, 40.f, 10.f, 2.f, 1200.f, 1, 0.f, 0.f, 0.f},
    {TEXT("Frostbolt"), TEXT("dba.ability.rat_dev_frostbolt@1"), TEXT("DivineBeasts.Development.Cooldown.Rat.Active02"),
        EDivineBeastsDevelopmentMagicKind::Frostbolt, 30.f, 8.f, 1.5f, 1600.f, 1, 0.f, 0.f, 0.f},
    {TEXT("ArcaneMissiles"), TEXT("dba.ability.rat_dev_arcanemissiles@1"), TEXT("DivineBeasts.Development.Cooldown.Rat.Ultimate"),
        EDivineBeastsDevelopmentMagicKind::ArcaneMissiles, 12.f, 12.f, 3.f, 1400.f, 3, 0.2f, 18.f, 3.f}
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
    SpawnSuccessor, SpawnStaleSource, InsufficientMomentum, FreshSpawnInput
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
 * 有界异步资源夹具：30 秒墙钟超时，运行段最多 12 个真实自动化帧、每帧推进隔离世界 0.05 秒。
 * Data 自己完成资源加载；不手写 Ready、完成回调、GFrameCounter 或技能私有连发状态。
 * 返回 true 仅表示潜伏命令结束，成功与否由 Automation 的真实断言/错误决定。
 */
class FRealMagicCommand final : public IAutomationLatentCommand
{
public:
    FRealMagicCommand(FAutomationTestBase* InTest, EScenario InScenario)
        : Test(InTest), Scenario(InScenario), StartedSeconds(FPlatformTime::Seconds()) {}

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
            if (!BeginRealActivation()) { return true; }
            bCastStarted = true;
            if (Scenario == EScenario::InsufficientMomentum) { return true; }
            return false;
        }
        World->Tick(LEVELTICK_All, 0.05f);
        ++FlightFrames;
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
            bValid &= Test->TestEqual(Label + TEXT("轴向射程厘米"), Definition->ProjectileMotion.RangeCm, 2000.f);
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
            bValid &= Test->TestEqual(Label + TEXT("数值表射程厘米"), Balance.CastRangeCm, 2000.f);
            bValid &= Test->TestFalse(Label + TEXT("拒绝零级"), Definition->TryGetLoadedBalance(0, Balance, Error));
            bValid &= Test->TestFalse(Label + TEXT("拒绝未制作的第二级"), Definition->TryGetLoadedBalance(2, Balance, Error));
            const UGameplayEffect* Cost = Ability->GetCostGameplayEffect();
            const UGameplayEffect* Cooldown = Ability->GetCooldownGameplayEffect();
            if (!Test->TestNotNull(Label + TEXT("真实成本GE"), Cost) || !Test->TestNotNull(Label + TEXT("真实冷却GE"), Cooldown))
            { return false; }
            bValid &= Test->TestTrue(Label + TEXT("成本GE来自当前真实资产"),
                Cost->GetClass() == MagicPath(TEXT("GE"), Expected, TEXT("Cost"), true).ResolveObject());
            bValid &= Test->TestTrue(Label + TEXT("冷却GE来自当前真实资产"),
                Cooldown->GetClass() == MagicPath(TEXT("GE"), Expected, TEXT("Cooldown"), true).ResolveObject());
            bValid &= Test->TestTrue(Label + TEXT("成本瞬时、冷却有时限"),
                Cost->DurationPolicy == EGameplayEffectDurationType::Instant && Cooldown->DurationPolicy == EGameplayEffectDurationType::HasDuration);
            const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(Expected.CooldownTag), false);
            bValid &= Test->TestTrue(Label + TEXT("冷却字典已登记且GA仅声明本槽标签"), Tag.IsValid() &&
                Ability->GetCooldownTags() && Ability->GetCooldownTags()->Num() == 1 && Ability->GetCooldownTags()->HasTagExact(Tag));
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
     * 真实Hero出生后直接通过平台输入释放三种魔法，不调用FillMomentum。
     * 漏改英雄资产的初始气势、输入未进入GAS或成本重复扣除都会使本用例失败。
     * 50→40→32→20来自用户批准的初始值和既有10/8/12成本，不由待测代码反算。
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
        const float ExpectedMomentum[] = {40.f, 32.f, 20.f};
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
            ASC->ProcessAbilityInput();
            ASC->AbilityInputReleased(InputTag, ASC->GetInputToken());
            Test->TestEqual(FString::Printf(TEXT("%s通过真实输入仅扣一次成本"), ExpectedMagic[Index].Name),
                ASC->GetNumericAttribute(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute()),
                ExpectedMomentum[Index]);
        }
        FString Error;
        auto* Identity = Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>();
        Test->TestTrue(TEXT("同一可信定义再次交接是幂等的"),
            Identity->TryUsePreloadedDefinition(WarmupLease, *WarmupOwner.Get(), Error));
        Test->TestEqual(TEXT("重复定义通知不把已消耗的20气势重置为50"),
            ASC->GetNumericAttribute(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute()), 20.f);
    }

    /** 初始化其他隔离场景的测试资源；真实出生回归不得调用本方法掩盖初始数据缺陷。 */
    bool FillMomentum()
    {
        const auto* Momentum = ASC->GetSet<UDivineBeastsMomentumAttributeSet>();
        if (!Test->TestNotNull(TEXT("真实Hero初始化的Momentum属性集"), Momentum) ||
            !Test->TestTrue(TEXT("真实Hero气势上限足够两次奥术提交"), Momentum->GetMaxMomentum() >= 24.f)) { return false; }
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
            const auto CostSpec = ASC->MakeOutgoingSpec(Ability->GetCostGameplayEffect()->GetClass(), 1.f, ASC->MakeEffectContext());
            const auto CooldownSpec = ASC->MakeOutgoingSpec(Ability->GetCooldownGameplayEffect()->GetClass(), 1.f, ASC->MakeEffectContext());
            if (!Test->TestTrue(Label + TEXT("两份真实GE Spec有效"), CostSpec.IsValid() && CooldownSpec.IsValid())) { return; }
            // 瞬时GE的返回句柄可以无效；必须以实际属性副作用证明执行，不能仅断言句柄。
            ASC->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());
            Test->TestEqual(Label + TEXT("实际扣除一次对应气势"),
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
        if (!Test->TestTrue(TEXT("真实进程满足开发技能配置和命令行资格"),
            UDivineBeastsDevelopmentMagicDefinition::IsDevelopmentEnabledForCurrentProcess())) { return false; }
        const auto* AbilityCDO = CastChecked<UDivineBeastsDevelopmentMagicAbility>(AbilityClasses[2]->GetDefaultObject());
        CostClass = AbilityCDO->GetCostGameplayEffect()->GetClass();
        CooldownClass = AbilityCDO->GetCooldownGameplayEffect()->GetClass();
        OtherAvatar = Instance->GetWorld()->SpawnActor<AActor>();
        if (!Test->TestTrue(TEXT("换Avatar回归的真实后继Actor"), OtherAvatar.IsValid())) { return false; }
        AbilityHandle = ASC->GiveAbility(FGameplayAbilitySpec(AbilityClasses[2], 1, INDEX_NONE, Pawn.Get()));
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
        if (Scenario == EScenario::InsufficientMomentum)
        {
            ASC->SetNumericAttributeBase(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute(), 11.f);
            Test->TestFalse(TEXT("11气势不足支付真实奥术12成本，GAS拒绝激活"), ASC->TryActivateAbility(AbilityHandle, false));
            Test->TestEqual(TEXT("拒绝激活没有Commit"), CommitCount, 0);
            Test->TestEqual(TEXT("拒绝激活没有成本GE"), CostApplications, 0);
            Test->TestEqual(TEXT("拒绝激活没有冷却GE"), CooldownApplications, 0);
            Test->TestEqual(TEXT("拒绝激活没有活跃冷却残留"), ASC->GetGameplayEffectCount(CooldownClass, ASC.Get()), 0);
            Test->TestEqual(TEXT("拒绝激活没有Spawn副作用"), Projectiles.Num(), 0);
            Test->TestEqual(TEXT("拒绝激活不扣气势"), ASC->GetNumericAttribute(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute()), 11.f);
            return true;
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
        }
        return true;
    }

    /** 对真实委托计数、属性、ActiveEffects、弹体终态和GAS激活状态做交叉断言，捕捉多Commit/迟到连发。 */
    void CheckTerminalBehavior()
    {
        const bool bSuccessor = Scenario == EScenario::SpawnSuccessor;
        const int32 ExpectedCommits = bSuccessor ? 2 : 1;
        Test->TestEqual(TEXT("每次GAS激活只Commit一次"), CommitCount, ExpectedCommits);
        Test->TestEqual(TEXT("每次Commit只应用一份真实成本GE"), CostApplications, ExpectedCommits);
        Test->TestEqual(TEXT("每次Commit只应用一份真实冷却GE"), CooldownApplications, ExpectedCommits);
        Test->TestEqual(TEXT("连发不重复扣费，取消/换Avatar不伪造退款"),
            ASC->GetNumericAttribute(UDivineBeastsMomentumAttributeSet::GetMomentumAttribute()), InitialMomentum - 12.f * ExpectedCommits);
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
        if (Scenario == EScenario::Burst || bSuccessor)
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
    double StartedSeconds;           // 墙钟秒，用于真实资源/初始化超时。
    TStrongObjectPtr<UGameInstance> Instance;
    TStrongObjectPtr<ULocalPlayer> LocalInputPlayer; // 仅输入回归拥有；与Controller同一夹具销毁，不触发身份认证。
    TWeakObjectPtr<ADivineBeastsCharacter> Pawn;
    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<AActor> WarmupOwner, OtherAvatar;
    TWeakObjectPtr<UGamePlatformAbilitySystemComponent> ASC;
    FGamePlatformDataLease WarmupLease; // 测试只释放本GI、本Owner签发的资源需求。
    UClass* AbilityClasses[3] = {};     // 只借用成功租约中的真实生成类，销毁角色前租约不得释放。
    UClass* CostClass = nullptr;
    UClass* CooldownClass = nullptr;
    FGameplayAbilitySpecHandle AbilityHandle;
    FDelegateHandle SpawnHandle, CommitHandle, EffectHandle;
    TArray<FObservedProjectile> Projectiles;
    float InitialMomentum = 0;         // 真实ASC起始属性值；费用期望来自计划字面量12。
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

// 实际GE未扣对应气势、未授予冷却标签、持续时间错误或移除后残留时失败；不要求激活开发技能。
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

// 有效服务器身份但气势仅11；成本门禁失效或拒绝路径仍产生Commit/GE/Spawn副作用时失败。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicInsufficientMomentumTest,
    "DivineBeasts.Abilities.DevelopmentMagic.ServerGAS.InsufficientMomentumHasNoSideEffects",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicInsufficientMomentumTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::InsufficientMomentum)); return true;
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

// 真实出生初始资源为零、平台输入无效或施法成本未执行时失败，不在测试中注入初始气势。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMagicFreshSpawnInputTest,
    "DivineBeasts.Abilities.DevelopmentMagic.ServerGAS.FreshSpawnCanCastAllThreeWithoutTestRefill",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMagicFreshSpawnInputTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(DivineBeastsDevelopmentMagicTests::FRealMagicCommand(this,
        DivineBeastsDevelopmentMagicTests::EScenario::FreshSpawnInput)); return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
