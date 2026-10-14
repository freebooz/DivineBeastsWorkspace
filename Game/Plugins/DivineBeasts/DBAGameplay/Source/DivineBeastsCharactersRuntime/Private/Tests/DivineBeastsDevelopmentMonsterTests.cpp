// 神兽联盟项目层开发怪物回归：在瞬态Game世界验证唯一ASC/Combat、300生命、真实伤害/死亡及重生代次。
// 测试只通过反射调用新增公开API；类型未实现时明确RED，不包含缺失生产头、不建立反射替身或伪造伤害。
// 调用者须使用开发配置中的[DivineBeasts.Development] bAllowDevelopmentMonsters=true和-DBADevelopmentMonsters。
// 本文件不改配置/命令行，不启动服务器，不读取地图/表现资产；世界、来源Actor及委托均由夹具独占并清理。
// 验证范围仅为结算Spec的目标代次保护，不能证明死亡前发射、重生后才命中的现有投射物已被阻断。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Types/GamePlatformCombatEvent.h"
#include "Types/GamePlatformCombatSpec.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace DivineBeastsDevelopmentMonsterTests
{
/** 测试拥有的生命值副本，独立读取生产反射返回值；不是生产快照或第二套战斗状态。 */
struct FLifeObservation
{
    float Health = 0.0f;
    float MaxHealth = 0.0f;
    bool bDead = false;
    int32 AvatarGeneration = 0;
};

/** 缺类型时用明确断言产生RED；类必须继承现有项目角色，不能用另造的ASC/Combat角色代替。 */
UClass* RequireMonsterClass(FAutomationTestBase& Test)
{
    UClass* Class = FindObject<UClass>(nullptr,
        TEXT("/Script/DivineBeastsCharactersRuntime.DivineBeastsDevelopmentMonster"));
    if (!Test.TestNotNull(TEXT("开发怪物必须由真实CharactersRuntime模块编译注册"), Class)) return nullptr;
    return Test.TestTrue(TEXT("怪物复用现有ADivineBeastsCharacter"),
        Class->IsChildOf(ADivineBeastsCharacter::StaticClass())) ? Class : nullptr;
}

/** 游戏线程瞬态世界夹具；真实来源不实现玩家Combatant资格，使用平台支持的原生测试Actor路径。
 * 怪物仍走自己的真实CanReceiveCombat；测试不绕开该门禁或直接调用ResolveIncomingDamage。
 */
struct FMonsterCombatFixture
{
    TStrongObjectPtr<UWorld> World;
    ADivineBeastsCharacter* Monster = nullptr;
    AActor* Source = nullptr;
    UGamePlatformCombatComponent* SourceCombat = nullptr;
    int32 DeathEvents = 0;
    FGamePlatformCombatEvent LastDeath;
    FDelegateHandle DeathHandle;

    /** 要求显式开发启动前提，缺失时报错；不临时改全局GConfig/命令行来冒充已启用。 */
    bool Create(FAutomationTestBase& Test, UClass* MonsterClass)
    {
        bool bConfigured = false;
        if (GConfig)
        {
            GConfig->GetBool(TEXT("DivineBeasts.Development"), TEXT("bAllowDevelopmentMonsters"),
                bConfigured, GGameIni);
        }
        if (!Test.TestTrue(TEXT("测试入口须显式配置允许开发怪物"), bConfigured) ||
            !Test.TestTrue(TEXT("测试入口须携带-DBADevelopmentMonsters"),
                FParse::Param(FCommandLine::Get(), TEXT("DBADevelopmentMonsters")))) return false;

        // UE5.8的CreateWorld已经初始化世界：仅通过InIVS传值，不再次InitializeNewWorld。
        const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false)
            .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
            true, ERHIFeatureLevel::Num, &Values));
        if (!Test.TestNotNull(TEXT("真实无地图Game世界"), World.Get())) return false;
        World->InitializeActorsForPlay(FURL());

        FActorSpawnParameters Spawn;
        // 本夹具不测试出生碰撞选址；AlwaysSpawn只用于隔离世界，生产WorldSubsystem另行验证安全候选。
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Monster = World->SpawnActor<ADivineBeastsCharacter>(MonsterClass,
            FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
        if (!Test.TestNotNull(TEXT("真实原生怪物Actor"), Monster)) return false;
        Monster->DispatchBeginPlay();
        if (!Test.TestTrue(TEXT("怪物已路由真实初始化与BeginPlay"),
            Monster->IsActorInitialized() && Monster->HasActorBegunPlay() && Monster->HasAuthority())) return false;
        DeathHandle = Monster->OnAuthoritativeDeath().AddLambda([this](const FGamePlatformCombatEvent& Event)
        {
            ++DeathEvents;
            LastDeath = Event;
        });

        Source = World->SpawnActor<AActor>();
        if (!Test.TestNotNull(TEXT("独立真实权威伤害来源"), Source)) return false;
        auto* SourceASC = NewObject<UGamePlatformAbilitySystemComponent>(Source);
        SourceCombat = NewObject<UGamePlatformCombatComponent>(Source);
        Source->AddInstanceComponent(SourceASC);
        Source->AddInstanceComponent(SourceCombat);
        SourceASC->RegisterComponent();
        SourceCombat->RegisterComponent();
        if (!Test.TestTrue(TEXT("来源绑定真实平台ASC"), SourceASC->BindAbilityActorInfo(Source, Source))) return false;
        Source->DispatchBeginPlay();
        return Test.TestTrue(TEXT("来源Combat已真实BeginPlay并拥有GAS属性"),
            SourceCombat->HasBegunPlay() && SourceCombat->GetCombatAttributeSet() != nullptr);
    }

    /** 仅调用无参初始化或带一个int32期望代次的重生UFUNCTION；形状不符时报错而非解引用空属性。 */
    bool InvokeAuthorityCommand(FAutomationTestBase& Test, FName Name, const int32* ExpectedGeneration = nullptr)
    {
        UFunction* Function = Monster ? Monster->FindFunction(Name) : nullptr;
        if (!Test.TestNotNull(TEXT("怪物公开权威命令必须存在"), Function)) return false;
        auto* Return = FindFProperty<FBoolProperty>(Function, TEXT("ReturnValue"));
        if (!Test.TestNotNull(TEXT("怪物权威命令必须返回bool"), Return)) return false;
        const int32 ExpectedParameterCount = ExpectedGeneration ? 2 : 1;
        if (!Test.TestEqual(TEXT("权威命令只接纳约定参数及bool返回值"),
            static_cast<int32>(Function->NumParms), ExpectedParameterCount)) return false;
        FStructOnScope Arguments(Function);
        if (ExpectedGeneration)
        {
            auto* Generation = FindFProperty<FIntProperty>(Function, TEXT("ExpectedAvatarGeneration"));
            if (!Test.TestNotNull(TEXT("重生参数必须为int32 ExpectedAvatarGeneration"), Generation)) return false;
            Generation->SetPropertyValue_InContainer(Arguments.GetStructMemory(), *ExpectedGeneration);
        }
        Monster->ProcessEvent(Function, Arguments.GetStructMemory());
        return Return->GetPropertyValue_InContainer(Arguments.GetStructMemory());
    }

    /** 初始化公开API返回真实成功；测试本身不写怪物属性、身份或生命代次。 */
    bool InitializeMonster(FAutomationTestBase& Test)
    {
        return Test.TestTrue(TEXT("显式开发守卫满足后初始化成功"),
            InvokeAuthorityCommand(Test, TEXT("AuthorityInitializeDevelopmentMonster")));
    }

    /** 重生只提交死亡时捕获的目标代次；是否允许以及状态变化完全由生产API决定。 */
    bool Respawn(FAutomationTestBase& Test, int32 ExpectedGeneration)
    {
        return InvokeAuthorityCommand(Test, TEXT("AuthorityRespawnDevelopmentMonster"), &ExpectedGeneration);
    }

    /** 只读GetLifeState输出的已约定四字段；类型名/字段类型异常明确失败，避免默认值掩盖缺失实现。 */
    bool ReadLifeState(FAutomationTestBase& Test, FLifeObservation& OutState) const
    {
        UFunction* Function = Monster ? Monster->FindFunction(TEXT("GetLifeState")) : nullptr;
        if (!Test.TestNotNull(TEXT("客户端只读GetLifeState必须存在"), Function)) return false;
        auto* Return = FindFProperty<FStructProperty>(Function, TEXT("ReturnValue"));
        if (!Test.TestNotNull(TEXT("GetLifeState必须返回生命快照结构"), Return)) return false;
        if (!Test.TestEqual(TEXT("公开生命快照使用已约定反射身份"), Return->Struct->GetFName(),
            FName(TEXT("DivineBeastsDevelopmentMonsterLifeState")))) return false;
        auto* Health = FindFProperty<FFloatProperty>(Return->Struct, TEXT("Health"));
        auto* MaxHealth = FindFProperty<FFloatProperty>(Return->Struct, TEXT("MaxHealth"));
        auto* Dead = FindFProperty<FBoolProperty>(Return->Struct, TEXT("bDead"));
        auto* Generation = FindFProperty<FIntProperty>(Return->Struct, TEXT("AvatarGeneration"));
        if (!Test.TestNotNull(TEXT("生命快照含float Health"), Health) ||
            !Test.TestNotNull(TEXT("生命快照含float MaxHealth"), MaxHealth) ||
            !Test.TestNotNull(TEXT("生命快照含bool bDead"), Dead) ||
            !Test.TestNotNull(TEXT("生命快照含int32 AvatarGeneration"), Generation)) return false;
        FStructOnScope Arguments(Function);
        Monster->ProcessEvent(Function, Arguments.GetStructMemory());
        const void* State = Return->ContainerPtrToValuePtr<void>(Arguments.GetStructMemory());
        OutState.Health = Health->GetPropertyValue_InContainer(State);
        OutState.MaxHealth = MaxHealth->GetPropertyValue_InContainer(State);
        OutState.bDead = Dead->GetPropertyValue_InContainer(State);
        OutState.AvatarGeneration = Generation->GetPropertyValue_InContainer(State);
        return true;
    }

    /** 每次显式捕获正数来源/目标代次，事件身份由测试输入拥有；不使用NormalizeSpec的零代次补全。 */
    FGamePlatformCombatSpec MakeDamage(float Magnitude) const
    {
        FGamePlatformCombatSpec Spec;
        Spec.EventId = FGuid::NewGuid();
        Spec.Source = Source;
        Spec.Target = Monster;
        Spec.Magnitude = Magnitude;
        Spec.DamageType = EGamePlatformDamageType::Magic;
        Spec.SourceAvatarGeneration = SourceCombat->GetCombatAvatarGeneration();
        Spec.TargetAvatarGeneration = Monster->GetGamePlatformCombatComponent()->GetCombatAvatarGeneration();
        return Spec;
    }

    /** 每一条伤害都通过来源Combat.ApplyDamage；测试不广播伪造死亡或直接消费GAS元属性。 */
    FGamePlatformCombatResult ApplyDamage(const FGamePlatformCombatSpec& Spec) const
    {
        return SourceCombat->ApplyDamage(Spec);
    }

    /** 委托先解绑，再让World路由Actor/Component EndPlay；只释放本夹具的世界，失败路径也执行。 */
    ~FMonsterCombatFixture()
    {
        if (IsValid(Monster)) Monster->OnAuthoritativeDeath().Remove(DeathHandle);
        if (World.IsValid())
        {
            World->EndPlay(EEndPlayReason::Quit);
            World->DestroyWorld(false);
        }
    }
};
}

// 捕获初始化错误：另造组件、伪造Hero/玩家Ready、未恢复300生命、未阻挡技能Visibility查询或返回失真的生命快照。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMonsterInitializationTest,
    "DivineBeasts.Character.DevelopmentMonster.InitializationAndLifeState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMonsterInitializationTest::RunTest(const FString&)
{
    using namespace DivineBeastsDevelopmentMonsterTests;
    UClass* Class = RequireMonsterClass(*this);
    if (!Class) return false;
    FMonsterCombatFixture Fixture;
    if (!Fixture.Create(*this, Class) || !Fixture.InitializeMonster(*this)) return false;
    auto* Monster = Fixture.Monster;
    auto* ASC = Monster->GetGamePlatformAbilitySystemComponent();
    auto* Combat = Monster->GetGamePlatformCombatComponent();
    TArray<UGamePlatformAbilitySystemComponent*> AbilitySystems;
    TArray<UGamePlatformCombatComponent*> Combats;
    Monster->GetComponents(AbilitySystems);
    Monster->GetComponents(Combats);
    TestEqual(TEXT("仅一个真实平台ASC"), AbilitySystems.Num(), 1);
    TestEqual(TEXT("仅一个真实平台Combat"), Combats.Num(), 1);
    if (!TestNotNull(TEXT("继承唯一ASC"), ASC) || !TestNotNull(TEXT("继承唯一Combat"), Combat)) return false;
    TestTrue(TEXT("无Controller仍将唯一ASC绑定本怪物"),
        !Monster->GetController() && ASC->GetOwnerActor() == Monster && ASC->GetAvatarActor() == Monster);
    TestTrue(TEXT("可作为真实Combat目标"), Monster->CanReceiveCombat());
    TestFalse(TEXT("静止训练敌人不成为伤害来源"), Monster->CanSourceCombat());
    TestEqual(TEXT("初始化生命为300"), Combat->GetCombatHealth(), 300.0f);
    TestEqual(TEXT("初始化最大生命为300"), Combat->GetCombatMaxHealth(), 300.0f);
    TestTrue(TEXT("怪物公开代次是Combat正代次"), Monster->GetCombatAvatarGeneration() > 0 &&
        Monster->GetCombatAvatarGeneration() == Combat->GetCombatAvatarGeneration());
    TestEqual(TEXT("胶囊阻挡现有魔法Sweep的Visibility通道"),
        Monster->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Visibility), ECR_Block);
    TestTrue(TEXT("原生静止敌人不执行CharacterMovement"),
        Monster->GetCharacterMovement()->MovementMode == MOVE_None);
    const auto* Identity = Monster->FindComponentByClass<UDivineBeastsCharacterComponent>();
    if (!TestNotNull(TEXT("保留继承身份组件但不伪造英雄"), Identity)) return false;
    TestTrue(TEXT("怪物没有HeroDefinitionId"), Identity->GetHeroDefinitionId().IsNone());
    TestFalse(TEXT("怪物不冒充玩家角色Ready"), Identity->IsCharacterReady());
    FLifeObservation Life;
    if (!Fixture.ReadLifeState(*this, Life)) return false;
    TestEqual(TEXT("只读快照生命300"), Life.Health, 300.0f);
    TestEqual(TEXT("只读快照最大生命300"), Life.MaxHealth, 300.0f);
    TestFalse(TEXT("初始快照为存活"), Life.bDead);
    TestEqual(TEXT("快照代次与唯一Combat一致"), Life.AvatarGeneration, Combat->GetCombatAvatarGeneration());
    return true;
}

// 捕获结算错误：目标资格未覆盖、独立扣血、重复事件重复扣血、死亡未发布或尸体继续受伤。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMonsterDamageDeathTest,
    "DivineBeasts.Character.DevelopmentMonster.AuthoritativeDamageAndDeath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMonsterDamageDeathTest::RunTest(const FString&)
{
    using namespace DivineBeastsDevelopmentMonsterTests;
    UClass* Class = RequireMonsterClass(*this);
    if (!Class) return false;
    FMonsterCombatFixture Fixture;
    if (!Fixture.Create(*this, Class) || !Fixture.InitializeMonster(*this)) return false;
    auto* Combat = Fixture.Monster->GetGamePlatformCombatComponent();
    const auto Hit = Fixture.MakeDamage(30.0f);
    const auto Result = Fixture.ApplyDamage(Hit);
    TestTrue(TEXT("来源真实Combat受理伤害"), Result.Error == EGamePlatformCombatError::None);
    TestEqual(TEXT("GAS/Combat真实结算后生命270"), Combat->GetCombatHealth(), 270.0f);
    TestEqual(TEXT("实际扣除30生命"), Result.AppliedToHealth, 30.0f);
    Fixture.ApplyDamage(Hit);
    TestEqual(TEXT("同EventId重复命中不再扣血"), Combat->GetCombatHealth(), 270.0f);
    TestEqual(TEXT("非致命命中不发布死亡"), Fixture.DeathEvents, 0);
    FLifeObservation Life;
    if (!Fixture.ReadLifeState(*this, Life)) return false;
    TestEqual(TEXT("受伤只读快照跟随唯一生命属性"), Life.Health, 270.0f);

    const auto Lethal = Fixture.MakeDamage(1000.0f);
    const auto DeathResult = Fixture.ApplyDamage(Lethal);
    TestTrue(TEXT("致命伤害仍通过真实Combat成功结算"), DeathResult.Error == EGamePlatformCombatError::None);
    TestTrue(TEXT("真实结算报告造成死亡"), DeathResult.bCausedDeath);
    TestTrue(TEXT("唯一Combat进入死亡状态"), Combat->IsCombatDead());
    TestEqual(TEXT("死亡生命归零"), Combat->GetCombatHealth(), 0.0f);
    TestEqual(TEXT("目标本代次权威死亡仅发布一次"), Fixture.DeathEvents, 1);
    TestTrue(TEXT("死亡事实保持本次来源事件身份和目标"),
        Fixture.LastDeath.EventId == Lethal.EventId && Fixture.LastDeath.TargetActor == Fixture.Monster);
    Fixture.ApplyDamage(Lethal);
    const auto DeadHit = Fixture.ApplyDamage(Fixture.MakeDamage(1.0f));
    TestTrue(TEXT("死后新的EventId伤害明确拒绝"), DeadHit.Error != EGamePlatformCombatError::None);
    TestFalse(TEXT("尸体退出受伤资格"), Fixture.Monster->CanReceiveCombat());
    TestEqual(TEXT("死后重复/新伤害不能再次发布死亡"), Fixture.DeathEvents, 1);
    TestEqual(TEXT("尸体生命保持零"), Combat->GetCombatHealth(), 0.0f);
    if (!Fixture.ReadLifeState(*this, Life)) return false;
    TestEqual(TEXT("死亡快照生命零"), Life.Health, 0.0f);
    TestTrue(TEXT("死亡快照包含真实死亡状态"), Life.bDead);
    TestEqual(TEXT("死亡快照保留致命Spec目标代次"), Life.AvatarGeneration, Lethal.TargetAvatarGeneration);
    return true;
}

// 捕获重生错误：零/旧期望代次可重置、存活时重复重生、代次未推进、旧正目标代次Spec污染新生命。
// 不测试10秒WorldSubsystem计时或旧投射物发射时间：它们不属于本夹具已经执行的合同。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentMonsterRespawnTest,
    "DivineBeasts.Character.DevelopmentMonster.RespawnGenerationContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentMonsterRespawnTest::RunTest(const FString&)
{
    using namespace DivineBeastsDevelopmentMonsterTests;
    UClass* Class = RequireMonsterClass(*this);
    if (!Class) return false;
    FMonsterCombatFixture Fixture;
    if (!Fixture.Create(*this, Class) || !Fixture.InitializeMonster(*this)) return false;
    auto* Combat = Fixture.Monster->GetGamePlatformCombatComponent();
    const int32 FirstGeneration = Combat->GetCombatAvatarGeneration();
    TestFalse(TEXT("存活怪物不得用重生API刷新生命/代次"), Fixture.Respawn(*this, FirstGeneration));
    TestEqual(TEXT("存活时拒绝重生保持原代次"), Combat->GetCombatAvatarGeneration(), FirstGeneration);
    auto OldTargetHit = Fixture.MakeDamage(10.0f);
    Fixture.ApplyDamage(Fixture.MakeDamage(1000.0f));
    if (!TestTrue(TEXT("重生前提是实际Combat死亡"), Combat->IsCombatDead())) return false;
    TestFalse(TEXT("零期望代次不能重生"), Fixture.Respawn(*this, 0));
    TestFalse(TEXT("旧期望代次不能重生"), Fixture.Respawn(*this, FirstGeneration - 1));
    TestEqual(TEXT("被拒重生不推进代次"), Combat->GetCombatAvatarGeneration(), FirstGeneration);
    TestTrue(TEXT("被拒重生仍为死亡"), Combat->IsCombatDead());
    if (!TestTrue(TEXT("匹配死亡代次的权威重生成功"), Fixture.Respawn(*this, FirstGeneration))) return false;
    const int32 NextGeneration = Combat->GetCombatAvatarGeneration();
    TestTrue(TEXT("重生推进实际Combat代次"), NextGeneration > FirstGeneration);
    TestEqual(TEXT("怪物公开代次同步唯一Combat"), Fixture.Monster->GetCombatAvatarGeneration(), NextGeneration);
    TestEqual(TEXT("重生恢复300生命"), Combat->GetCombatHealth(), 300.0f);
    TestEqual(TEXT("重生保留300生命上限"), Combat->GetCombatMaxHealth(), 300.0f);
    TestFalse(TEXT("重生撤销Combat死亡"), Combat->IsCombatDead());
    TestTrue(TEXT("重生恢复受伤资格"), Fixture.Monster->CanReceiveCombat());
    TestFalse(TEXT("迟到旧代次重生不能操作新生命"), Fixture.Respawn(*this, FirstGeneration));
    TestFalse(TEXT("新代次存活状态也不能重复重生"), Fixture.Respawn(*this, NextGeneration));
    TestEqual(TEXT("重复重生不再次推进代次"), Combat->GetCombatAvatarGeneration(), NextGeneration);

    // 使用新事件ID排除幂等缓存干扰，只保留死亡前捕获的正目标代次作为拒绝原因。
    OldTargetHit.EventId = FGuid::NewGuid();
    const auto Stale = Fixture.ApplyDamage(OldTargetHit);
    TestTrue(TEXT("旧正目标代次Spec由Combat明确拒绝"),
        Stale.Error == EGamePlatformCombatError::InvalidAvatarGeneration);
    TestEqual(TEXT("旧代次Spec不能扣除新生命"), Combat->GetCombatHealth(), 300.0f);
    FLifeObservation Life;
    if (!Fixture.ReadLifeState(*this, Life)) return false;
    TestEqual(TEXT("重生快照生命300"), Life.Health, 300.0f);
    TestEqual(TEXT("重生快照带新代次"), Life.AvatarGeneration, NextGeneration);
    TestFalse(TEXT("重生快照不再死亡"), Life.bDead);
    const auto NewHit = Fixture.ApplyDamage(Fixture.MakeDamage(25.0f));
    TestTrue(TEXT("新代次Spec继续通过唯一Combat受伤"), NewHit.Error == EGamePlatformCombatError::None);
    TestEqual(TEXT("新代次真实伤害后生命275"), Combat->GetCombatHealth(), 275.0f);
    Fixture.ApplyDamage(Fixture.MakeDamage(1000.0f));
    TestEqual(TEXT("新代次再次死亡发布第二次独立事实"), Fixture.DeathEvents, 2);
    TestEqual(TEXT("第二次死亡事实属于新代次"), Fixture.LastDeath.TargetAvatarGeneration, NextGeneration);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS：夹具不进入正式运行逻辑。
