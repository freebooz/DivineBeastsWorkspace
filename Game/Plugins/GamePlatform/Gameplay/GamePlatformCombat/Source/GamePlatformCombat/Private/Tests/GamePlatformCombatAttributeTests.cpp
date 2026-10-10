#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Attributes/GamePlatformAttributeSet.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "UObject/UnrealType.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Tags/GamePlatformCombatTags.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
/** 仅测试夹具：真实无地图World、权威Actor及平台ASC，不构造反射替身、不联网、不改帧号。 */
struct FCombatResetReentryFixture
{
    TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<AActor> Actor;
    TStrongObjectPtr<UGamePlatformAbilitySystemComponent> ASC;
    TStrongObjectPtr<UGamePlatformCombatComponent> Combat;
    TStrongObjectPtr<UGamePlatformCombatAttributeSet> Attributes;

    bool Initialize(FAutomationTestBase& Test)
    {
        // CreateWorld内部已经InitializeNewWorld，设置一次作为InIVS传入，禁止重复初始化WorldSettings。
        const UWorld::InitializationValues Values = UWorld::InitializationValues()
            .AllowAudioPlayback(false).CreatePhysicsScene(false).RequiresHitProxies(false)
            .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
            true, ERHIFeatureLevel::Num, &Values));
        if (!Test.TestNotNull(TEXT("创建真实测试世界"), World.Get())) return false;
        World->InitializeActorsForPlay(FURL());
        Actor.Reset(World->SpawnActor<AActor>());
        if (!Test.TestNotNull(TEXT("生成真实权威拥有者"), Actor.Get())) return false;
        ASC.Reset(NewObject<UGamePlatformAbilitySystemComponent>(Actor.Get()));
        Combat.Reset(NewObject<UGamePlatformCombatComponent>(Actor.Get()));
        Actor->AddInstanceComponent(ASC.Get());
        Actor->AddInstanceComponent(Combat.Get());
        ASC->RegisterComponent();
        Combat->RegisterComponent();
        ASC->InitAbilityActorInfo(Actor.Get(), Actor.Get());
        // 由Actor按引擎次序登记Tick函数并路由组件BeginPlay，不能直接跳过其注册前提。
        Actor->DispatchBeginPlay();
        Attributes.Reset(Combat->GetCombatAttributeSet());
        return Test.TestTrue(TEXT("组件已真实BeginPlay并绑定ASC/属性集"),
            Actor->HasAuthority() && Combat->HasBegunPlay() && Attributes.IsValid() &&
            Combat->GetAbilitySystemComponent() == ASC.Get());
    }

    FGamePlatformCombatResult GrantShield()
    {
        FGamePlatformCombatSpec Spec;
        Spec.EventId = FGuid::NewGuid();
        Spec.Source = Actor.Get();
        Spec.Target = Actor.Get();
        Spec.SourceAvatarGeneration = Combat->GetCombatAvatarGeneration();
        Spec.TargetAvatarGeneration = Spec.SourceAvatarGeneration;
        Spec.Magnitude = 9.0f;
        return Combat->ApplyShield(Spec, 10.0f);
    }

    ~FCombatResetReentryFixture()
    {
        // Native委托ScopeExit先于本对象析构；Actor->Destroy路由剩余EndPlay，最后只销毁一次World。
        if (Actor.IsValid()) Actor->Destroy();
        if (World.IsValid()) World->DestroyWorld(false);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatAttributeDefaultsTest,
    "GamePlatform.Combat.Attributes.DefaultsAndClamp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatAttributeDefaultsTest::RunTest(const FString& Parameters)
{
    UGamePlatformCombatAttributeSet* Attributes =
        NewObject<UGamePlatformCombatAttributeSet>();

    TestEqual(TEXT("默认MaxHealth"), Attributes->GetMaxHealth(), 100.0f);
    TestEqual(TEXT("默认Health"), Attributes->GetHealth(), 100.0f);

    float NegativeMaxHealth = -10.0f;
    Attributes->PreAttributeChange(
        UGamePlatformCombatAttributeSet::GetMaxHealthAttribute(),
        NegativeMaxHealth);
    TestEqual(TEXT("MaxHealth不得为负"), NegativeMaxHealth, 0.0f);

    const uint32 NaNBits = 0x7FC00000u;
    float NaNDamage = 0.0f;
    FMemory::Memcpy(&NaNDamage, &NaNBits, sizeof(float));
    Attributes->PreAttributeChange(
        UGamePlatformCombatAttributeSet::GetIncomingDamageAttribute(),
        NaNDamage);
    TestTrue(TEXT("NaN元属性被规范化"), FMath::IsFinite(NaNDamage));
    TestEqual(TEXT("NaN伤害归零"), NaNDamage, 0.0f);

    float NegativeHealing = -50.0f;
    Attributes->PreAttributeChange(
        UGamePlatformCombatAttributeSet::GetIncomingHealingAttribute(),
        NegativeHealing);
    TestEqual(TEXT("负治疗归零"), NegativeHealing, 0.0f);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatAttributeFamiliesTest,
    "GamePlatform.Combat.Attributes.Families",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatAttributeFamiliesTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("唯一平台战斗数值属性集继承中立GAS基类"),
        UGamePlatformCombatAttributeSet::StaticClass()->IsChildOf(UGamePlatformAttributeSet::StaticClass()));

    UGamePlatformCombatAttributeSet* Combat = NewObject<UGamePlatformCombatAttributeSet>();
    TestEqual(TEXT("初始伤害增强为0"), Combat->GetDamageBonus(), 0.0f);
    TestEqual(TEXT("初始伤害减免为0"), Combat->GetDamageReduction(), 0.0f);

    // 删除盾永久属性：临时盾只由GameplayEffect与组件效果实例负责。
    TestNull(TEXT("盾属性不再存在"),
        FindFProperty<FProperty>(UGamePlatformCombatAttributeSet::StaticClass(), FName(TEXT("Shield"))));
    TestNull(TEXT("最大盾属性不再存在"),
        FindFProperty<FProperty>(UGamePlatformCombatAttributeSet::StaticClass(), FName(TEXT("MaxShield"))));

    // Buff/Debuff可用有符号加减修饰器，不再提供攻击/防御/穿透/抗性等同义字段。
    float Weakness = -20.0f;
    Combat->PreAttributeChange(UGamePlatformCombatAttributeSet::GetDamageBonusAttribute(), Weakness);
    TestEqual(TEXT("削弱攻击的Debuff允许负增伤"), Weakness, -20.0f);
    float Vulnerability = -15.0f;
    Combat->PreAttributeChange(UGamePlatformCombatAttributeSet::GetDamageReductionAttribute(), Vulnerability);
    TestEqual(TEXT("降低减免的易伤Debuff允许负减免"), Vulnerability, -15.0f);

    for (const FName Retired : {
        FName(TEXT("AttackPower")), FName(TEXT("AbilityPower")),
        FName(TEXT("Penetration")), FName(TEXT("Resistance")),
        FName(TEXT("CriticalChance")), FName(TEXT("CriticalDamage"))
    })
    {
        TestNull(TEXT("旧攻击防御属性未迁入新的公共数值集"),
            FindFProperty<FProperty>(UGamePlatformCombatAttributeSet::StaticClass(), Retired));
    }

    return true;
}

/** 原生Dead标签移除通知内成功推进高代次，并写入新需求；外层不得改回低代次或清需求。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatResetNativeTagSuccessorTest,
    "GamePlatform.Combat.ResetReentry.NativeTagSuccessor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatResetNativeTagSuccessorTest::RunTest(const FString&)
{
    FCombatResetReentryFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    Fixture.ASC->AddLooseGameplayTag(GamePlatformCombatTags::State_Dead, 1,
        EGameplayTagReplicationState::TagAndCountToAll);
    bool bNotified = false;
    bool bSuccessorAccepted = false;
    const auto Handle = Fixture.ASC->RegisterGameplayTagEvent(GamePlatformCombatTags::State_Dead,
        EGameplayTagEventType::NewOrRemoved).AddLambda([&](const FGameplayTag, int32 Count)
        {
            if (Count != 0 || bNotified) return;
            bNotified = true;
            bSuccessorAccepted = Fixture.Combat->ResetForNewAvatar(20, 0.75f);
            Fixture.Attributes->SetIncomingHealing(7.0f);
        });
    ON_SCOPE_EXIT
    { Fixture.ASC->UnregisterGameplayTagEvent(Handle, GamePlatformCombatTags::State_Dead, EGameplayTagEventType::NewOrRemoved); };
    TestFalse(TEXT("被后继接管的外层重置不报告成功"), Fixture.Combat->ResetForNewAvatar(2, 0.25f));
    TestTrue(TEXT("真实RemoveLooseGameplayTag同步触发原生委托"), bNotified);
    TestTrue(TEXT("内层合法高代次重置成功"), bSuccessorAccepted);
    TestEqual(TEXT("保留后继Avatar代次"), Fixture.Combat->GetCombatAvatarGeneration(), 20);
    TestEqual(TEXT("保留后继健康值"), Fixture.Attributes->GetHealth(), 75.0f);
    TestEqual(TEXT("旧栈不能清后继元属性需求"), Fixture.Attributes->GetIncomingHealing(), 7.0f);
    return true;
}

/** Dead标签通知内真实销毁组件路由EndPlay；旧返回栈不得继续设置健康、代次或重新接受请求。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatResetNativeTagClosingTest,
    "GamePlatform.Combat.ResetReentry.NativeTagClosing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatResetNativeTagClosingTest::RunTest(const FString&)
{
    FCombatResetReentryFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    Fixture.ASC->AddLooseGameplayTag(GamePlatformCombatTags::State_Dead, 1,
        EGameplayTagReplicationState::TagAndCountToAll);
    bool bNotified = false;
    const auto Handle = Fixture.ASC->RegisterGameplayTagEvent(GamePlatformCombatTags::State_Dead,
        EGameplayTagEventType::NewOrRemoved).AddLambda([&](const FGameplayTag, int32 Count)
        {
            if (Count != 0 || bNotified) return;
            bNotified = true;
            Fixture.Combat->DestroyComponent();
        });
    ON_SCOPE_EXIT
    { Fixture.ASC->UnregisterGameplayTagEvent(Handle, GamePlatformCombatTags::State_Dead, EGameplayTagEventType::NewOrRemoved); };
    TestFalse(TEXT("关闭后的旧重置不报告成功"), Fixture.Combat->ResetForNewAvatar(2, 0.25f));
    TestTrue(TEXT("真实标签通知内执行组件销毁"), bNotified);
    TestFalse(TEXT("真实组件EndPlay已完成"), Fixture.Combat->HasBegunPlay());
    TestEqual(TEXT("关闭前代次不被尾写"), Fixture.Combat->GetCombatAvatarGeneration(), 1);
    TestEqual(TEXT("通知后不再写健康属性"), Fixture.Attributes->GetHealth(), 100.0f);
    TestFalse(TEXT("关闭组件拒绝后续重置"), Fixture.Combat->ResetForNewAvatar(30));
    return true;
}

/** SetNumericAttributeBase的真实健康委托内重置；已经完成自身合法代次推进不能被误判为失败。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatResetAttributeSuccessorTest,
    "GamePlatform.Combat.ResetReentry.AttributeSuccessor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatResetAttributeSuccessorTest::RunTest(const FString&)
{
    FCombatResetReentryFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    bool bNotified = false;
    bool bSuccessorAccepted = false;
    auto& Delegate = Fixture.ASC->GetGameplayAttributeValueChangeDelegate(
        UGamePlatformCombatAttributeSet::GetHealthAttribute());
    const auto Handle = Delegate.AddLambda([&](const FOnAttributeChangeData& Change)
        {
            if (bNotified || !FMath::IsNearlyEqual(Change.NewValue, 25.0f)) return;
            bNotified = true;
            bSuccessorAccepted = Fixture.Combat->ResetForNewAvatar(20, 0.75f);
            Fixture.Attributes->SetIncomingHealing(7.0f);
        });
    ON_SCOPE_EXIT { Delegate.Remove(Handle); };
    TestFalse(TEXT("属性通知被接管后旧重置终止"), Fixture.Combat->ResetForNewAvatar(2, 0.25f));
    TestTrue(TEXT("真实GAS健康变化委托触发"), bNotified);
    TestTrue(TEXT("内层重置自身推进代次仍成功"), bSuccessorAccepted);
    TestEqual(TEXT("后继代次仍为20"), Fixture.Combat->GetCombatAvatarGeneration(), 20);
    TestEqual(TEXT("健康保持后继75"), Fixture.Attributes->GetHealth(), 75.0f);
    TestEqual(TEXT("后继元属性需求未被尾清"), Fixture.Attributes->GetIncomingHealing(), 7.0f);
    return true;
}

/** 健康委托内关闭发生在已写25之后；停止后继写入，但不伪造回滚已完成的GAS通知。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatResetAttributeClosingTest,
    "GamePlatform.Combat.ResetReentry.AttributeClosing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatResetAttributeClosingTest::RunTest(const FString&)
{
    FCombatResetReentryFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    bool bNotified = false;
    auto& Delegate = Fixture.ASC->GetGameplayAttributeValueChangeDelegate(
        UGamePlatformCombatAttributeSet::GetHealthAttribute());
    const auto Handle = Delegate.AddLambda([&](const FOnAttributeChangeData& Change)
        {
            if (bNotified || !FMath::IsNearlyEqual(Change.NewValue, 25.0f)) return;
            bNotified = true;
            Fixture.Combat->DestroyComponent();
        });
    ON_SCOPE_EXIT { Delegate.Remove(Handle); };
    TestFalse(TEXT("健康通知内关闭使重置终止"), Fixture.Combat->ResetForNewAvatar(2, 0.25f));
    TestTrue(TEXT("真实健康通知内组件关闭"), bNotified);
    TestEqual(TEXT("已完成的GAS属性写入不被假回滚"), Fixture.Attributes->GetHealth(), 25.0f);
    TestEqual(TEXT("未提交的代次保持1"), Fixture.Combat->GetCombatAvatarGeneration(), 1);
    TestFalse(TEXT("关闭后拒绝新重置"), Fixture.Combat->ResetForNewAvatar(30));
    return true;
}

/** 多个真实护盾GE移除过程中高代次接管并授予新盾；旧批次不得误清新盾或覆盖新化身。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatResetShieldSuccessorTest,
    "GamePlatform.Combat.ResetReentry.ShieldSuccessor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatResetShieldSuccessorTest::RunTest(const FString&)
{
    FCombatResetReentryFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    if (!TestTrue(TEXT("第一份真实护盾GE授予成功"), Fixture.GrantShield().IsSuccess()) ||
        !TestTrue(TEXT("第二份真实护盾GE授予成功"), Fixture.GrantShield().IsSuccess())) return false;
    bool bNotified = false;
    bool bSuccessorAccepted = false;
    bool bNewShieldAccepted = false;
    const auto Handle = Fixture.ASC->RegisterGameplayTagEvent(GamePlatformCombatTags::State_Shielded,
        EGameplayTagEventType::AnyCountChange).AddLambda([&](const FGameplayTag, int32 Count)
        {
            if (Count != 1 || bNotified) return;
            bNotified = true;
            bSuccessorAccepted = Fixture.Combat->ResetForNewAvatar(20, 0.75f);
            bNewShieldAccepted = Fixture.GrantShield().IsSuccess();
        });
    ON_SCOPE_EXIT
    { Fixture.ASC->UnregisterGameplayTagEvent(Handle, GamePlatformCombatTags::State_Shielded, EGameplayTagEventType::AnyCountChange); };
    TestFalse(TEXT("护盾移除通知接管后旧重置终止"), Fixture.Combat->ResetForNewAvatar(2, 0.25f));
    TestTrue(TEXT("真实GE标签计数移除触发回调"), bNotified);
    TestTrue(TEXT("高代次重置完成"), bSuccessorAccepted);
    TestTrue(TEXT("新化身真实护盾成功"), bNewShieldAccepted);
    TestEqual(TEXT("后继化身不被覆盖"), Fixture.Combat->GetCombatAvatarGeneration(), 20);
    TestEqual(TEXT("后继健康不被覆盖"), Fixture.Attributes->GetHealth(), 75.0f);
    TestEqual(TEXT("仅新化身的一份护盾仍有效"), Fixture.ASC->GetTagCount(GamePlatformCombatTags::State_Shielded), 1);
    return true;
}

/** 无重入时按请求/自动推进代次正常成功；防只比较旧Avatar结果而拒绝本次合法提交的假门闩。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatResetNormalAdvanceTest,
    "GamePlatform.Combat.ResetReentry.NormalAdvance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatResetNormalAdvanceTest::RunTest(const FString&)
{
    FCombatResetReentryFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    TestTrue(TEXT("正常重置到代次2成功"), Fixture.Combat->ResetForNewAvatar(2, 0.5f));
    TestEqual(TEXT("正常健康为50"), Fixture.Attributes->GetHealth(), 50.0f);
    TestTrue(TEXT("重复请求旧代次按已有合同自动推进成功"), Fixture.Combat->ResetForNewAvatar(2, 0.9f));
    TestEqual(TEXT("自动推进为代次3"), Fixture.Combat->GetCombatAvatarGeneration(), 3);
    TestEqual(TEXT("第二次健康为90"), Fixture.Attributes->GetHealth(), 90.0f);
    return true;
}

/** 第一处RemoveActiveEffectsWithGrantedTags通过真实Stun GE解除通知重入，而非仅Loose Tag路径。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatResetControlEffectSuccessorTest,
    "GamePlatform.Combat.ResetReentry.ControlEffectSuccessor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatResetControlEffectSuccessorTest::RunTest(const FString&)
{
    FCombatResetReentryFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    FGamePlatformCombatSpec Spec;
    Spec.EventId = FGuid::NewGuid();
    Spec.Source = Fixture.Actor.Get();
    Spec.Target = Fixture.Actor.Get();
    Spec.SourceAvatarGeneration = Fixture.Combat->GetCombatAvatarGeneration();
    Spec.TargetAvatarGeneration = Spec.SourceAvatarGeneration;
    Spec.Magnitude = 1.0f;
    if (!TestTrue(TEXT("真实Stun GE申请成功"),
        Fixture.Combat->ApplyControl(Spec, EGamePlatformControlType::Stun, 1.0f).IsSuccess())) return false;
    bool bNotified = false;
    bool bSuccessorAccepted = false;
    const auto Handle = Fixture.ASC->RegisterGameplayTagEvent(GamePlatformCombatTags::Control_Stun,
        EGameplayTagEventType::NewOrRemoved).AddLambda([&](const FGameplayTag, int32 Count)
        {
            if (Count != 0 || bNotified) return;
            bNotified = true;
            bSuccessorAccepted = Fixture.Combat->ResetForNewAvatar(20, 0.75f);
        });
    ON_SCOPE_EXIT
    { Fixture.ASC->UnregisterGameplayTagEvent(Handle, GamePlatformCombatTags::Control_Stun, EGameplayTagEventType::NewOrRemoved); };
    TestFalse(TEXT("控制GE解除通知后旧栈终止"), Fixture.Combat->ResetForNewAvatar(2, 0.25f));
    TestTrue(TEXT("真实GE解除同步通知"), bNotified);
    TestTrue(TEXT("高代次重置成功"), bSuccessorAccepted);
    TestEqual(TEXT("控制解除后的后继代次未回退"), Fixture.Combat->GetCombatAvatarGeneration(), 20);
    TestEqual(TEXT("控制解除后的后继健康未覆盖"), Fixture.Attributes->GetHealth(), 75.0f);
    return true;
}

/** 生成的IncomingDamage setter也进GAS委托；此处接管后旧栈不能继续清IncomingHealing。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformCombatResetMetaAttributeSuccessorTest,
    "GamePlatform.Combat.ResetReentry.MetaAttributeSuccessor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformCombatResetMetaAttributeSuccessorTest::RunTest(const FString&)
{
    FCombatResetReentryFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    Fixture.Attributes->SetIncomingDamage(5.0f);
    bool bNotified = false;
    bool bSuccessorAccepted = false;
    auto& Delegate = Fixture.ASC->GetGameplayAttributeValueChangeDelegate(
        UGamePlatformCombatAttributeSet::GetIncomingDamageAttribute());
    const auto Handle = Delegate.AddLambda([&](const FOnAttributeChangeData& Change)
        {
            if (bNotified || !FMath::IsNearlyZero(Change.NewValue)) return;
            bNotified = true;
            bSuccessorAccepted = Fixture.Combat->ResetForNewAvatar(20, 0.75f);
            Fixture.Attributes->SetIncomingHealing(7.0f);
        });
    ON_SCOPE_EXIT { Delegate.Remove(Handle); };
    TestFalse(TEXT("元属性通知接管后旧重置终止"), Fixture.Combat->ResetForNewAvatar(2, 0.25f));
    TestTrue(TEXT("生成setter真实进入GAS变化通知"), bNotified);
    TestTrue(TEXT("后继重置成功"), bSuccessorAccepted);
    TestEqual(TEXT("元属性通知后的后继代次"), Fixture.Combat->GetCombatAvatarGeneration(), 20);
    TestEqual(TEXT("后继治疗需求不被旧栈清空"), Fixture.Attributes->GetIncomingHealing(), 7.0f);
    return true;
}

#endif
