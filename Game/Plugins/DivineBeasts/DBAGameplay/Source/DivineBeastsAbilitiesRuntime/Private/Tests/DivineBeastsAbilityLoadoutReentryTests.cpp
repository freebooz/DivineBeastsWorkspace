// SSLM GAS技能装配：使用真实UE能力组件及效果撤销委托验证后继授权不会被旧同步调用栈清空。
// 必须在包含本次源码的新UE5.8 DLL中运行；不在静态测试或旧模块中伪造结果。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Effects/GamePlatformCombatGameplayEffects.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/Actor.h"

namespace
{
/** 独立权威测试世界：不连接后端、不授予生肖主数据、不产生正式客户端副作用。 */
struct FNativeLoadoutGASFixture
{
    UWorld* World = nullptr;
    AActor* Actor = nullptr;
    UGamePlatformAbilitySystemComponent* ASC = nullptr;
    UDivineBeastsAbilityLoadoutComponent* Loadout = nullptr;

    bool Initialize(FAutomationTestBase& Test)
    {
        const UWorld::InitializationValues Values = UWorld::InitializationValues()
            .AllowAudioPlayback(false).CreatePhysicsScene(false)
            .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None,
            nullptr, true, ERHIFeatureLevel::Num, &Values);
        if (!Test.TestNotNull(TEXT("创建GAS隔离世界"), World)) return false;
        World->InitializeActorsForPlay(FURL());
        Actor = World->SpawnActor<AActor>();
        if (!Test.TestNotNull(TEXT("真实权威Actor"), Actor)) return false;
        ASC = NewObject<UGamePlatformAbilitySystemComponent>(Actor);
        Loadout = NewObject<UDivineBeastsAbilityLoadoutComponent>(Actor);
        Actor->AddInstanceComponent(ASC);
        Actor->AddInstanceComponent(Loadout);
        ASC->RegisterComponent();
        Loadout->RegisterComponent();
        ASC->InitAbilityActorInfo(Actor, Actor);
        Actor->DispatchBeginPlay();
        return Test.TestTrue(TEXT("真实ASC和装配组件完成初始化"),
            Actor->HasAuthority() && ASC->GetAvatarActor() == Actor &&
            Loadout->HasBegunPlay());
    }

    FActiveGameplayEffectHandle ApplyEffect()
    {
        // 非堆叠、有限期真实平台GameplayEffect；不使用纯模拟句柄。
        return ASC->ApplyGameplayEffectToSelf(
            GetDefault<UGamePlatformShieldGameplayEffect>(),
            1.0f, ASC->MakeEffectContext());
    }

    ~FNativeLoadoutGASFixture()
    {
        if (IsValid(Actor)) Actor->Destroy();
        if (World) World->DestroyWorld(false);
    }
};
}

/**
 * 旧资源移除委托中同步建立新效果：旧外层撤销函数只能删除旧句柄。
 * 这个测试用真实UE GAS移除事件检验MoveTemp及摘除顺序，而非文本/模拟广播。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsGASOwnedGrantReentryTest,
    "DivineBeasts.Abilities.GrantTransaction.NativeRemovedEffectPreservesSuccessor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsGASOwnedGrantReentryTest::RunTest(const FString&)
{
    FNativeLoadoutGASFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    auto* ASC = Fixture.ASC;
    auto* Loadout = Fixture.Loadout;
    Loadout->AbilitySystem = ASC;
    const FActiveGameplayEffectHandle Old = Fixture.ApplyEffect();
    if (!TestTrue(TEXT("真实旧GameplayEffect应用成功"), Old.IsValid())) return false;
    Loadout->OwnedEffectHandles.Add(Old);
    bool bReentered = false;
    FActiveGameplayEffectHandle Successor;
    const FDelegateHandle Handle = ASC->OnAnyGameplayEffectRemovedDelegate().AddLambda(
        [&](const FActiveGameplayEffect&)
        {
            if (bReentered) return;
            bReentered = true;
            ++Loadout->RequestSerial; // 模拟真实旧移除委托中后继请求接管
            Successor = Fixture.ApplyEffect();
            if (Successor.IsValid())
            {
                Loadout->OwnedEffectHandles.Add(Successor);
                Loadout->PendingSetId = TEXT("dba.abilityset.ox_placeholder@1");
            }
        });
    Loadout->RevokeOwnedGrants();
    ASC->OnAnyGameplayEffectRemovedDelegate().Remove(Handle);
    TestTrue(TEXT("旧效果移除实际触发GAS同步委托"), bReentered);
    TestTrue(TEXT("后继效果真实应用成功"), Successor.IsValid());
    TestTrue(TEXT("旧效果已经撤销"), ASC->GetActiveGameplayEffect(Old) == nullptr);
    if (Successor.IsValid())
    {
        TestNotNull(TEXT("后继效果仍被真实ASC持有"), ASC->GetActiveGameplayEffect(Successor));
        TestTrue(TEXT("后继句柄仍属于当前装配账本"),
            Loadout->OwnedEffectHandles.Num() == 1 &&
            Loadout->OwnedEffectHandles[0] == Successor);
    }
    TestEqual(TEXT("后继PendingSetId不可被旧撤销栈清空"), Loadout->PendingSetId,
        FName(TEXT("dba.abilityset.ox_placeholder@1")));
    // 清理测试拥有的后继效果时不再订阅同步移除委托。
    Loadout->RevokeOwnedGrants();
    return true;
}

/** 旧角色NotReady通知撤销GAS时，后继同步更新Ready不得被原调用栈再次设为false。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsGASReadinessReentryTest,
    "DivineBeasts.Abilities.GrantTransaction.NativeReadinessCallbackCannotClearSuccessor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsGASReadinessReentryTest::RunTest(const FString&)
{
    FNativeLoadoutGASFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    auto* ASC = Fixture.ASC;
    auto* Loadout = Fixture.Loadout;
    Loadout->AbilitySystem = ASC;
    const FActiveGameplayEffectHandle Old = Fixture.ApplyEffect();
    if (!TestTrue(TEXT("旧角色真实效果已应用"), Old.IsValid())) return false;
    Loadout->OwnedEffectHandles.Add(Old);
    Loadout->PublishLoadout(TEXT("Hero.Zodiac.Rat"), 10, true, {});
    const int64 BeforeRevision = Loadout->GetLoadoutStateRef().Revision;
    bool bReentered = false;
    const FDelegateHandle Handle = ASC->OnAnyGameplayEffectRemovedDelegate().AddLambda(
        [&](const FActiveGameplayEffect&)
        {
            if (bReentered) return;
            bReentered = true;
            ++Loadout->RequestSerial;
            // 仅为重入顺序夹具；不授予真实生肖技能/不伪造正式服务器玩法资格。
            Loadout->PublishLoadout(TEXT("Hero.Zodiac.Ox"), 11, true, {});
        });
    Loadout->HandleCharacterReadinessChanged(false);
    ASC->OnAnyGameplayEffectRemovedDelegate().Remove(Handle);
    TestTrue(TEXT("撤销确实触发一次同步委托"), bReentered);
    TestTrue(TEXT("旧NotReady不得覆盖后继Ready"), Loadout->GetLoadoutStateRef().bReady);
    TestEqual(TEXT("后继英雄身份仍存在"), Loadout->GetLoadoutStateRef().HeroDefinitionId,
        FName(TEXT("Hero.Zodiac.Ox")));
    TestEqual(TEXT("旧栈不再增加一次错误的空技能状态修订"), Loadout->GetLoadoutStateRef().Revision,
        BeforeRevision + 1);
    TestNull(TEXT("测试旧效果已正常撤销"), ASC->GetActiveGameplayEffect(Old));
    return true;
}

// 真实EndPlay入口会移除当前GAS句柄；其同步通知不得重新打开授予或把旧状态写回客户端。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsGASEndPlayReentryTest,
    "DivineBeasts.Abilities.GrantTransaction.NativeEndPlayCallbackRejectsNewGrant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsGASEndPlayReentryTest::RunTest(const FString&)
{
    FNativeLoadoutGASFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    auto* ASC = Fixture.ASC;
    auto* Loadout = Fixture.Loadout;
    Loadout->AbilitySystem = ASC;
    const FActiveGameplayEffectHandle Old = Fixture.ApplyEffect();
    if (!TestTrue(TEXT("退出前真实GE应用成功"), Old.IsValid())) return false;
    Loadout->OwnedEffectHandles.Add(Old);
    Loadout->PublishLoadout(TEXT("Hero.Zodiac.Rat"), 1, true, {});
    const int64 OriginalRevision = Loadout->GetLoadoutStateRef().Revision;
    bool bReentered = false;
    bool bClosingWasVisible = false;
    bool bRefreshRejected = false;
    const FDelegateHandle Removed = ASC->OnAnyGameplayEffectRemovedDelegate().AddLambda(
        [&](const FActiveGameplayEffect&)
        {
            if (bReentered) return;
            bReentered = true;
            bClosingWasVisible = Loadout->bEndingPlay;
            FString Error;
            bRefreshRejected = !Loadout->AuthorityRefreshFromCharacter(Error);
            // 严格验证结束后的发布入口也失败关闭，而不是只检查刷新API。
            Loadout->PublishLoadout(TEXT("Hero.Zodiac.Ox"), 2, true, {});
        });
    Loadout->EndPlay(EEndPlayReason::Destroyed);
    ASC->OnAnyGameplayEffectRemovedDelegate().Remove(Removed);
    TestTrue(TEXT("旧效果释放确实触发原生GAS委托"), bReentered);
    TestTrue(TEXT("EndPlay外部撤销之前关闭标志已生效"), bClosingWasVisible);
    TestTrue(TEXT("已退出组件拒绝重新申请技能集"), bRefreshRejected);
    TestEqual(TEXT("旧同步回调不能制造新的技能栏Revision"),
        Loadout->GetLoadoutStateRef().Revision, OriginalRevision);
    TestEqual(TEXT("旧同步回调不能更换技能栏Hero身份"),
        Loadout->GetLoadoutStateRef().HeroDefinitionId, FName(TEXT("Hero.Zodiac.Rat")));
    TestNull(TEXT("退出后旧真实GE已撤销"), ASC->GetActiveGameplayEffect(Old));
    return true;
}
#endif
