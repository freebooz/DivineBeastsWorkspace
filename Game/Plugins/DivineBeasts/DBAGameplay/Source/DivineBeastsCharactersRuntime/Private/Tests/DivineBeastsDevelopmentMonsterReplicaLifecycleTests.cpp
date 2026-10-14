// 项目双端开发怪物新增独立回归：客户端OnRep只同步胶囊，以及真实GAS通知中开发许可撤销后的自有操作解锁。
// 不改原有五项已通过用例；真实World/EngineContext、ASC/Combat，不伪造DamageResult、权威生命或GAS结算。
// 客户端夹具只以真实服务器生命快照驱动实际OnRep；角色交换/反射投递不是网络包传输或联机验收证据。
#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITORONLY_DATA

#include "Misc/AutomationTest.h"
#include "Characters/DivineBeastsDevelopmentMonster.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeExit.h"
#include "Types/GamePlatformCombatSpec.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace DivineBeastsDevelopmentMonsterReplicaTests
{
/** 测试自有物理查询世界，真实来源Combat；生产装配/地图/PlayerStart/WorldSubsystem不参与此夹具。 */
struct FReplicaFixture
{
    TStrongObjectPtr<UWorld> World;
    ADivineBeastsDevelopmentMonster* AuthorityMonster = nullptr;
    TArray<ADivineBeastsDevelopmentMonster*> Replicas;
    AActor* Source = nullptr;
    UGamePlatformCombatComponent* SourceCombat = nullptr;
    bool bWorldContextCreated = false;

    /** 是否初始化由用例决定，守卫重入负例必须观察生产初始化的第一次GAS写入。 */
    bool Create(FAutomationTestBase& Test, bool bInitialize)
    {
        if (!Test.TestTrue(TEXT("真实开发入口已启用"),
            ADivineBeastsDevelopmentMonster::IsDevelopmentEnabledForCurrentProcess()) ||
            !Test.TestNotNull(TEXT("Engine真实WorldContext入口"), GEngine)) return false;
        const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
            true, ERHIFeatureLevel::Num, &Values));
        if (!Test.TestNotNull(TEXT("真实物理查询世界"), World.Get())) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World.Get());
        bWorldContextCreated = true;
        World->InitializeActorsForPlay(FURL());
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // 权威目标移出客户端射线路径；不以忽略死亡胶囊来让查询伪通过。
        AuthorityMonster = World->SpawnActor<ADivineBeastsDevelopmentMonster>(FVector(0.0, 1000.0, 100.0), FRotator::ZeroRotator, Spawn);
        if (!Test.TestNotNull(TEXT("真实权威怪物"), AuthorityMonster)) return false;
        AuthorityMonster->DispatchBeginPlay();
        if (bInitialize && !Test.TestTrue(TEXT("权威怪物唯一Combat真实初始化"),
            AuthorityMonster->AuthorityInitializeDevelopmentMonster())) return false;
        Source = World->SpawnActor<AActor>();
        if (!Test.TestNotNull(TEXT("真实伤害来源Actor"), Source)) return false;
        auto* SourceASC = NewObject<UGamePlatformAbilitySystemComponent>(Source);
        SourceCombat = NewObject<UGamePlatformCombatComponent>(Source);
        Source->AddInstanceComponent(SourceASC);
        Source->AddInstanceComponent(SourceCombat);
        SourceASC->RegisterComponent();
        SourceCombat->RegisterComponent();
        if (!Test.TestTrue(TEXT("来源ASC真实绑定"), SourceASC->BindAbilityActorInfo(Source, Source))) return false;
        Source->DispatchBeginPlay();
        return Test.TestTrue(TEXT("来源Combat真实就绪"), SourceCombat->HasBegunPlay() && SourceCombat->GetCombatAttributeSet());
    }

    /** 用Engine公开SwapRoles在BeginPlay前建立非权威角色，不创建Controller/Hero或赋予客户端命令权限。 */
    ADivineBeastsDevelopmentMonster* AddReplica(FAutomationTestBase& Test, const FVector& Location)
    {
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Replica = World->SpawnActor<ADivineBeastsDevelopmentMonster>(Location, FRotator::ZeroRotator, Spawn);
        if (!Test.TestNotNull(TEXT("真实接收端怪物Actor"), Replica)) return nullptr;
        Replicas.Add(Replica);
        Replica->SetReplicates(true);
        Replica->SwapRoles();
        if (!Test.TestFalse(TEXT("接收端必须为非权威Actor"), Replica->HasAuthority())) return nullptr;
        Replica->DispatchBeginPlay();
        auto* ReplicaASC = Replica->GetGamePlatformAbilitySystemComponent();
        if (!Test.TestNotNull(TEXT("非玩家客户端真实ASC"), ReplicaASC) ||
            !Test.TestTrue(TEXT("无Controller客户端ASC绑定自身Owner/Avatar"),
                ReplicaASC->GetOwnerActor() == Replica && ReplicaASC->GetAvatarActor() == Replica)) return nullptr;
        Test.TestFalse(TEXT("接收端不能初始化权威生命"), Replica->AuthorityInitializeDevelopmentMonster());
        return Replica;
    }

    /** 从真实权威Actor读取完整值复制到复制属性，再调用真实OnRep；不写接收端Combat/GAS或伪造死亡结果。 */
    bool Deliver(FAutomationTestBase& Test, ADivineBeastsDevelopmentMonster* Replica) const
    {
        auto* Property = FindFProperty<FStructProperty>(ADivineBeastsDevelopmentMonster::StaticClass(), TEXT("LifeState"));
        auto* Notify = Replica->FindFunction(TEXT("OnRep_LifeState"));
        if (!Test.TestNotNull(TEXT("真实复制生命属性"), Property) ||
            !Test.TestNotNull(TEXT("真实OnRep生命回调"), Notify) ||
            !Test.TestTrue(TEXT("复制属性类型与公开生命快照一致"),
                Property->Struct == FDivineBeastsDevelopmentMonsterLifeState::StaticStruct())) return false;
        const auto Received = AuthorityMonster->GetLifeState();
        Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Replica), &Received);
        Replica->ProcessEvent(Notify, nullptr);
        Test.TestEqual(TEXT("接收端仅投递实际服务器代次"), Replica->GetLifeState().AvatarGeneration, Received.AvatarGeneration);
        return true;
    }

    /** 权威生命的所有变化必须由真实正代次ApplyDamage/Respawn产生。 */
    FGamePlatformCombatResult Damage(float Magnitude) const
    {
        FGamePlatformCombatSpec Spec;
        Spec.EventId = FGuid::NewGuid();
        Spec.Source = Source;
        Spec.Target = AuthorityMonster;
        Spec.Magnitude = Magnitude;
        Spec.DamageType = EGamePlatformDamageType::Magic;
        Spec.SourceAvatarGeneration = SourceCombat->GetCombatAvatarGeneration();
        Spec.TargetAvatarGeneration = AuthorityMonster->GetCombatAvatarGeneration();
        return SourceCombat->ApplyDamage(Spec);
    }

    /** 查询包含两个真实客户端胶囊，不忽略前方Actor；无PhysicsScene或碰撞体会由前置命中断言暴露。 */
    AActor* VisibilityHit() const
    {
        FHitResult Hit;
        const bool bHit = World->LineTraceSingleByChannel(Hit, FVector(800.0, 0.0, 100.0),
            FVector(1600.0, 0.0, 100.0), ECC_Visibility, FCollisionQueryParams(SCENE_QUERY_STAT(DevelopmentMonsterReplica), false));
        return bHit ? Hit.GetActor() : nullptr;
    }

    /** 显式销毁已BeginPlay的Actor，然后清自有World/Context，所有提前失败同样释放。 */
    ~FReplicaFixture()
    {
        if (!World.IsValid()) return;
        for (auto* Replica : Replicas) if (IsValid(Replica)) World->DestroyActor(Replica, true);
        if (IsValid(AuthorityMonster)) AuthorityMonster->Destroy();
        if (IsValid(Source)) Source->Destroy();
        World->EndPlay(EEndPlayReason::Quit);
        World->DestroyWorld(false);
        if (bWorldContextCreated && GEngine) GEngine->DestroyWorldContext(World.Get());
    }
};

/** 只保存/恢复一个已存在的开发键的原始内存字符串；不Flush、不改磁盘、不改其他Config/CLI策略。 */
struct FScopedDevelopmentPermission
{
    FString OriginalValue;
    bool bCaptured = false;
    bool bRestored = false;
    FScopedDevelopmentPermission()
    {
        bCaptured = GConfig && GConfig->GetString(TEXT("DivineBeasts.Development"),
            TEXT("bAllowDevelopmentMonsters"), OriginalValue, GGameIni);
    }
    void Reject() const
    {
        if (bCaptured && GConfig) GConfig->SetBool(TEXT("DivineBeasts.Development"),
            TEXT("bAllowDevelopmentMonsters"), false, GGameIni);
    }
    void Restore()
    {
        if (!bRestored && bCaptured && GConfig)
        {
            GConfig->SetString(TEXT("DivineBeasts.Development"), TEXT("bAllowDevelopmentMonsters"), *OriginalValue, GGameIni);
            bRestored = true;
        }
    }
    ~FScopedDevelopmentPermission() { Restore(); }
};
}

// OnRep只能读真实生命事实同步客户端碰撞：死亡必须不挡后方活体，重生必须恢复Visibility，不能写权威生命。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMonsterReplicaCollisionTest,
    "DivineBeasts.Character.DevelopmentMonster.ClientReplica.OnRepDeathRespawnVisibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMonsterReplicaCollisionTest::RunTest(const FString&)
{
    using namespace DivineBeastsDevelopmentMonsterReplicaTests;
    FReplicaFixture Fixture;
    if (!Fixture.Create(*this, true)) return false;
    auto* Front = Fixture.AddReplica(*this, FVector(1000.0, 0.0, 100.0));
    auto* Behind = Fixture.AddReplica(*this, FVector(1400.0, 0.0, 100.0));
    if (!Front || !Behind || !Fixture.Deliver(*this, Front) || !Fixture.Deliver(*this, Behind)) return false;
    auto* ReplicaCombat = Front->GetGamePlatformCombatComponent();
    if (!TestNotNull(TEXT("客户端仍拥有真实Combat组件"), ReplicaCombat)) return false;
    const float ClientCombatHealth = ReplicaCombat->GetCombatHealth();
    const int32 ClientCombatGeneration = ReplicaCombat->GetCombatAvatarGeneration();
    if (!TestTrue(TEXT("活体前方胶囊真实阻挡射线"), Fixture.VisibilityHit() == Front)) return false;
    const int32 DyingGeneration = Fixture.AuthorityMonster->GetLifeState().AvatarGeneration;
    if (!TestTrue(TEXT("致死伤害经过真实Combat"), Fixture.Damage(1000.0f).IsSuccess()) ||
        !TestTrue(TEXT("服务器Combat已提交死亡"), Fixture.AuthorityMonster->GetLifeState().bDead) ||
        !Fixture.Deliver(*this, Front)) return false;
    TestTrue(TEXT("客户端OnRep死亡关闭胶囊"), Front->GetCapsuleComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
    TestTrue(TEXT("前方已死NPC不挡后方存活目标"), Fixture.VisibilityHit() == Behind);
    TestFalse(TEXT("非权威客户端不能执行重生"), Front->AuthorityRespawnDevelopmentMonster(DyingGeneration));
    if (!TestTrue(TEXT("服务器匹配死亡代次真实重生"),
        Fixture.AuthorityMonster->AuthorityRespawnDevelopmentMonster(DyingGeneration)) || !Fixture.Deliver(*this, Front)) return false;
    TestTrue(TEXT("重生复制推进生命代次"), Front->GetLifeState().AvatarGeneration > DyingGeneration);
    TestTrue(TEXT("客户端OnRep重生恢复胶囊查询及物理"),
        Front->GetCapsuleComponent()->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
    TestTrue(TEXT("重生Visibility再次阻挡"), Front->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block);
    TestTrue(TEXT("真实射线再次命中重生前方目标"), Fixture.VisibilityHit() == Front);
    TestEqual(TEXT("OnRep不写客户端GAS生命"), ReplicaCombat->GetCombatHealth(), ClientCombatHealth);
    TestEqual(TEXT("OnRep不重置客户端Combat代次"), ReplicaCombat->GetCombatAvatarGeneration(), ClientCombatGeneration);
    TestEqual(TEXT("显示复制不改服务器当前生命"), Fixture.AuthorityMonster->GetLifeState().Health, 300.0f);
    return true;
}

// 许可只在真实MaxHealth GAS通知内撤销：第一次须失败，恢复许可后不得因旧操作锁存而永久拒绝重试。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMonsterGuardReentryTest,
    "DivineBeasts.Character.DevelopmentMonster.GuardReentry.ConfigOffDuringGASInitializationCanRetry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMonsterGuardReentryTest::RunTest(const FString&)
{
    using namespace DivineBeastsDevelopmentMonsterReplicaTests;
    FReplicaFixture Fixture;
    if (!Fixture.Create(*this, false)) return false;
    FScopedDevelopmentPermission Config;
    if (!TestTrue(TEXT("开发键原始内存值已捕获"), Config.bCaptured)) return false;
    auto* MonsterASC = Fixture.AuthorityMonster->GetGamePlatformAbilitySystemComponent();
    if (!TestNotNull(TEXT("目标真实ASC"), MonsterASC) ||
        !TestTrue(TEXT("第一次初始化确实会改变真实MaxHealth"),
            !FMath::IsNearlyEqual(MonsterASC->GetNumericAttribute(UGamePlatformCombatAttributeSet::GetMaxHealthAttribute()), 300.0f))) return false;
    bool bPermissionRevokedInGAS = false;
    auto& MaxHealthDelegate = MonsterASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetMaxHealthAttribute());
    const auto Handle = MaxHealthDelegate.AddLambda([&Config, &bPermissionRevokedInGAS](const FOnAttributeChangeData&)
    {
        if (bPermissionRevokedInGAS) return;
        bPermissionRevokedInGAS = true;
        Config.Reject();
    });
    const bool bFirstInitialized = Fixture.AuthorityMonster->AuthorityInitializeDevelopmentMonster();
    MaxHealthDelegate.Remove(Handle);
    TestTrue(TEXT("开发许可在真实GAS MaxHealth回调内撤销"), bPermissionRevokedInGAS);
    TestFalse(TEXT("当前许可撤销中止初始化"), bFirstInitialized);
    TestEqual(TEXT("被中止操作不得发布有效生命"), Fixture.AuthorityMonster->GetLifeState().AvatarGeneration, 0);
    Config.Restore();
    TestTrue(TEXT("唯一开发守卫恢复允许"), ADivineBeastsDevelopmentMonster::IsDevelopmentEnabledForCurrentProcess());
    TestTrue(TEXT("恢复许可后旧操作已解锁且真实初始化可重试"), Fixture.AuthorityMonster->AuthorityInitializeDevelopmentMonster());
    TestEqual(TEXT("重试初始化提交实际300生命"), Fixture.AuthorityMonster->GetLifeState().Health, 300.0f);
    return true;
}

// 重生Health通知撤权时，Combat可能已完成不可回滚的新生命提交；项目失败必须退役Actor，不能留下禁伤/禁重生僵尸。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMonsterRespawnPermissionRetirementTest,
    "DivineBeasts.Character.DevelopmentMonster.GuardReentry.ConfigOffDuringGASRespawnRetiresActor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMonsterRespawnPermissionRetirementTest::RunTest(const FString&)
{
    using namespace DivineBeastsDevelopmentMonsterReplicaTests;
    FReplicaFixture Fixture;
    if (!Fixture.Create(*this, true)) return false;
    FScopedDevelopmentPermission Config;
    if (!TestTrue(TEXT("撤权回归已保存开发键原始内存值"), Config.bCaptured)) return false;

    // Actor及组件/AttributeSet均在调用前强持有：生产同步Destroy后仅读受保护对象，不沿用夹具未保护rawActor。
    const TStrongObjectPtr<ADivineBeastsDevelopmentMonster> KeepMonster(Fixture.AuthorityMonster);
    const TStrongObjectPtr<UGamePlatformAbilitySystemComponent> KeepASC(KeepMonster->GetGamePlatformAbilitySystemComponent());
    const TStrongObjectPtr<UGamePlatformCombatComponent> KeepCombat(KeepMonster->GetGamePlatformCombatComponent());
    if (!TestNotNull(TEXT("目标实际ASC"), KeepASC.Get()) || !TestNotNull(TEXT("目标实际Combat"), KeepCombat.Get())) return false;
    const TStrongObjectPtr<UGamePlatformCombatAttributeSet> KeepAttributes(KeepCombat->GetCombatAttributeSet());
    if (!TestNotNull(TEXT("实际GAS属性集"), KeepAttributes.Get())) return false;
    const TWeakObjectPtr<ADivineBeastsDevelopmentMonster> WeakMonster(KeepMonster.Get());
    FDelegateHandle HealthHandle;
    ON_SCOPE_EXIT
    {
        KeepASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
        // 退役的Actor由生产销毁；先撤销夹具raw入口，保证强引用释放后析构不再访问它，也不由测试补Destroy。
        if (!WeakMonster.IsValid() || KeepMonster->IsActorBeingDestroyed()) Fixture.AuthorityMonster = nullptr;
    };
    if (!TestTrue(TEXT("死亡由真实SourceCombat.ApplyDamage产生"), Fixture.Damage(1000.0f).IsSuccess())) return false;
    const auto Death = KeepMonster->GetLifeState();
    if (!TestTrue(TEXT("前置为Combat确认的死亡"), Death.bDead && KeepCombat->IsCombatDead()) ||
        !TestEqual(TEXT("前置实际GAS生命归零"), KeepAttributes->GetHealth(), 0.0f) ||
        !TestTrue(TEXT("死亡胶囊已关闭"), KeepMonster->GetCapsuleComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision)) return false;

    bool bRevokedInHealth = false;
    float HealthInCallback = -1.0f;
    int32 GenerationInCallback = 0;
    HealthHandle = KeepASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetHealthAttribute()).AddLambda(
        [&Config, &KeepAttributes, &KeepCombat, &bRevokedInHealth, &HealthInCallback, &GenerationInCallback](const FOnAttributeChangeData& Change)
        {
            if (bRevokedInHealth || Change.NewValue <= 0.0f) return;
            bRevokedInHealth = true;
            HealthInCallback = KeepAttributes->GetHealth();
            GenerationInCallback = KeepCombat->GetCombatAvatarGeneration();
            // 只撤销真实内存配置；不销毁Actor、不改GAS、不调用Reset或伪造战斗/恢复结果。
            Config.Reject();
        });
    const bool bRespawn = KeepMonster->AuthorityRespawnDevelopmentMonster(Death.AvatarGeneration);
    KeepASC->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
    HealthHandle.Reset();
    const int32 GenerationAfterReset = KeepCombat->GetCombatAvatarGeneration();
    // EndPlay会清Combat属性集入口，故从调用前强持有的真实AttributeSet读取已发生的GAS写入。
    const float GASHealthAfterReset = KeepAttributes->GetHealth();
    const bool bCombatDeadAfterReset = KeepCombat->IsCombatDead();
    const bool bResetCommitted = GenerationAfterReset == Death.AvatarGeneration + 1 &&
        !bCombatDeadAfterReset && FMath::IsNearlyEqual(GASHealthAfterReset, 300.0f);
    const auto ProjectionAfterReset = KeepMonster->GetLifeState();
    const bool bRetired = !WeakMonster.IsValid() || KeepMonster->IsActorBeingDestroyed();
    // bResetCommitted是实际提交事实的诊断，不冒充私有ResetForNewAvatar局部返回值；bRespawn为真实公开调用返回。
    AddInfo(FString::Printf(TEXT("Respawn撤权实测：bRespawn=%d bResetCommitted=%d callbackGen=%d callbackHP=%.0f CombatGen=%d GASHP=%.0f CombatDead=%d LifeGen=%d LifeHP=%.0f LifeDead=%d retired=%d"),
        bRespawn, bResetCommitted, GenerationInCallback, HealthInCallback, GenerationAfterReset, GASHealthAfterReset,
        bCombatDeadAfterReset, ProjectionAfterReset.AvatarGeneration, ProjectionAfterReset.Health, ProjectionAfterReset.bDead, bRetired));
    TestTrue(TEXT("撤权发生于真实重生Health GAS通知"), bRevokedInHealth);
    TestEqual(TEXT("真实Health通知观察到300，不伪造恢复"), HealthInCallback, 300.0f);
    TestFalse(TEXT("撤开发许可的项目重生不能报告成功"), bRespawn);
    TestTrue(TEXT("真实Combat仍完成新代次活体提交，不允许伪造GAS回滚"), bResetCommitted);
    TestTrue(TEXT("提交后项目失效必须由生产退役Actor"), bRetired);
    Config.Restore();
    TestTrue(TEXT("开发许可原样恢复"), ADivineBeastsDevelopmentMonster::IsDevelopmentEnabledForCurrentProcess());
    bool bStillInWorld = false;
    for (TActorIterator<ADivineBeastsDevelopmentMonster> It(Fixture.World.Get()); It; ++It)
        if (*It == KeepMonster.Get() && IsValid(*It) && !It->IsActorBeingDestroyed()) bStillInWorld = true;
    TestFalse(TEXT("恢复许可后世界不残留旧死投影/新活Combat的僵尸Actor"), bStillInWorld);
    TestFalse(TEXT("退役后不能恢复受伤准入"), KeepMonster->CanReceiveCombat());
    // 测试不再尝试初始化/重生或补写LifeState；退役是此失败路径唯一合法终态，而非夹具fake recovery。
    return true;
}

#endif
