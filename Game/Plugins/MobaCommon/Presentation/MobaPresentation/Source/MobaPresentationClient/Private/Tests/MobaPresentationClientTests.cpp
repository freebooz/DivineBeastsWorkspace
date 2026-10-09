#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Adapters/MobaPresentationFactAdapters.h"
#include "Tags/MobaPresentationTags.h"
#include "Tags/GamePlatformCombatTags.h"
#include "Types/GamePlatformCombatEvent.h"
#include "MobaPresentationClientSubsystem.h"
#include "Events/MobaPresentationContextContributor.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Framework/GamePlatformArenaGameState.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
/** 仅测试：实际两World、Engine Within对象和PlayerAdded子系统，Provider只记录受理，不播放资源。 */
struct FMobaContributorFixture
{
    TStrongObjectPtr<UWorld> First;
    TStrongObjectPtr<UWorld> Second;
    TStrongObjectPtr<UGameInstance> Instance;
    TStrongObjectPtr<UGameViewportClient> Viewport;
    TStrongObjectPtr<ULocalPlayer> Player;
    FObjectPropertyBase* WorldProperty = nullptr;
    bool bPlayerAdded = false;
    bool Initialize(FAutomationTestBase& Test)
    {
        if (!Test.TestNotNull(TEXT("Engine Within宿主"), GEngine)) return false;
        const UWorld::InitializationValues Values = UWorld::InitializationValues()
            .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
        First.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values));
        Second.Reset(UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values));
        if (!Test.TestNotNull(TEXT("第一真实世界"), First.Get()) || !Test.TestNotNull(TEXT("第二真实世界"), Second.Get())) return false;
        First->InitializeActorsForPlay(FURL()); Second->InitializeActorsForPlay(FURL());
        Instance.Reset(NewObject<UGameInstance>());
        Viewport.Reset(NewObject<UGameViewportClient>(GEngine));
        Player.Reset(NewObject<ULocalPlayer>(GEngine));
        WorldProperty = FindFProperty<FObjectPropertyBase>(Viewport->GetClass(), TEXT("World"));
        auto* InstanceProperty = FindFProperty<FObjectPropertyBase>(Viewport->GetClass(), TEXT("GameInstance"));
        if (!Test.TestNotNull(TEXT("真实Viewport World字段"), WorldProperty) ||
            !Test.TestNotNull(TEXT("真实Viewport GI字段"), InstanceProperty)) return false;
        First->SetGameInstance(Instance.Get()); Second->SetGameInstance(Instance.Get());
        WorldProperty->SetObjectPropertyValue_InContainer(Viewport.Get(), First.Get());
        InstanceProperty->SetObjectPropertyValue_InContainer(Viewport.Get(), Instance.Get());
        Player->PlayerAdded(Viewport.Get(), 0); bPlayerAdded = true;
        return Test.TestNotNull(TEXT("MOBA真实子系统"), Adapter()) &&
            Test.TestNotNull(TEXT("平台真实协调器"), Coordinator());
    }
    UMobaPresentationClientSubsystem* Adapter() const { return Player->GetSubsystem<UMobaPresentationClientSubsystem>(); }
    UGamePlatformPresentationClientSubsystem* Coordinator() const { return Player->GetSubsystem<UGamePlatformPresentationClientSubsystem>(); }
    void Travel()
    {
        WorldProperty->SetObjectPropertyValue_InContainer(Viewport.Get(), Second.Get());
        Adapter()->RefreshBindings();
    }
    ~FMobaContributorFixture()
    {
        if (bPlayerAdded) Player->PlayerRemoved();
        if (First.IsValid()) First->DestroyWorld(false);
        if (Second.IsValid()) Second->DestroyWorld(false);
    }
};
struct FContributorObservation { bool bDestroyed = false; bool bDestroyedInsideCall = false; };
/** 原生virtual替身仅在Tests：先复制闭包/观察者，旧实现自毁时仍能安全记录，不借用已析构成员。 */
struct FReentrantMobaContributor final : IMobaPresentationContextContributor
{
    TSharedRef<FContributorObservation> Observation;
    TFunction<void(FMobaPresentationContext&)> Callback;
    FReentrantMobaContributor(TSharedRef<FContributorObservation> InObservation, TFunction<void(FMobaPresentationContext&)> InCallback)
        : Observation(InObservation), Callback(MoveTemp(InCallback)) {}
    ~FReentrantMobaContributor() override { Observation->bDestroyed = true; }
    void Contribute(FMobaPresentationContext& Context) const override
    {
        const auto State = Observation;
        const auto Invoke = Callback;
        Invoke(Context);
        State->bDestroyedInsideCall = State->bDestroyed;
    }
};
FMobaPresentationAdaptedFact MakeContributorFact(FGuid Id)
{
    FMobaPresentationAdaptedFact Fact; Fact.Identity.FactId = Id;
    Fact.Identity.bConfirmed = true; Fact.Semantic = MobaPresentationTags::Combat_Hit;
    return Fact;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMobaPresentationAbilityAdapterTest,
    "Moba.Presentation.Client.AbilityAdapter",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMobaPresentationAbilityAdapterTest::RunTest(const FString&)
{
    FMobaPresentationAbilityFact Fact;
    Fact.Identity.FactId = FGuid::NewGuid();
    Fact.Identity.bConfirmed = true;
    Fact.Type = EMobaPresentationAbilityFactType::CastStart;
    Fact.AbilityId = TEXT("Ability.Test");

    const FMobaPresentationAdaptedFact Adapted =
        FMobaPresentationFactAdapters::FromAbilityFact(Fact);
    TestEqual(TEXT("CastStart语义"), Adapted.Semantic, MobaPresentationTags::Ability_Cast_Start.GetTag());
    TestEqual(TEXT("AbilityId保持"), Adapted.Context.AbilityId, FString(TEXT("Ability.Test")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMobaPresentationPersistentStatusTest,
    "Moba.Presentation.Client.PersistentStatus",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMobaPresentationPersistentStatusTest::RunTest(const FString&)
{
    FMobaPresentationStatusFact Fact;
    Fact.Identity.FactId = FGuid::NewGuid();
    Fact.Identity.bConfirmed = true;
    Fact.Type = EMobaPresentationStatusFactType::Apply;
    Fact.StatusId = TEXT("Status.Test");

    const FMobaPresentationAdaptedFact Adapted =
        FMobaPresentationFactAdapters::FromStatusFact(Fact);
    TestFalse(TEXT("Status Apply应为持续事实"), Adapted.bTransient);
    TestEqual(TEXT("Status Apply使用Persistent生命周期"), Adapted.Lifetime, EGamePlatformPresentationLifetime::Persistent);
    return true;
}


/**
 * 历史暴击标签仅为资产兼容保留，MOBA不能据此产生额外表现事实。
 * 已确认伤害只有一个普通Hit（命中）事实，不能双重播放。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMobaPresentationLegacyCriticalIgnoredTest,
    "Moba.Presentation.Client.LegacyCriticalIgnored",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMobaPresentationLegacyCriticalIgnoredTest::RunTest(const FString&)
{
    FGamePlatformCombatEvent Event;
    Event.EventId = FGuid::NewGuid();
    Event.EventType = EGamePlatformCombatEventType::Damage;
    Event.AppliedMagnitude = 200.0f;
    Event.TargetAvatarGeneration = 3;
    Event.ResultTags.AddTag(GamePlatformCombatTags::Result_Critical);

    TArray<FMobaPresentationAdaptedFact> Facts;
    FMobaPresentationFactAdapters::FromCombatEvent(Event, Facts);
    TestEqual(TEXT("历史暴击标签不得产生第二个表现事实"), Facts.Num(), 1);
    if (Facts.Num() == 1)
    {
        TestEqual(TEXT("只保留正常命中语义"), Facts[0].Semantic, MobaPresentationTags::Combat_Hit.GetTag());
        TestEqual(TEXT("正常命中使用实际伤害值"), Facts[0].Context.Magnitude, 200.0f);
    }
    return true;
}

/** 注册表持有唯一贡献者，virtual内自注销不销毁仍在执行的对象；真实Provider只接收一次。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobaPresentationContributorSelfUnregisterTest,
    "Moba.Presentation.Client.ContributorSelfUnregister",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMobaPresentationContributorSelfUnregisterTest::RunTest(const FString&)
{
    FMobaContributorFixture Fixture; if (!Fixture.Initialize(*this)) return false;
    auto State = MakeShared<FContributorObservation>();
    TSharedPtr<FReentrantMobaContributor> Contributor = MakeShared<FReentrantMobaContributor>(State,
        [&](FMobaPresentationContext&) { Fixture.Adapter()->UnregisterContextContributor(TEXT("Self")); });
    Fixture.Adapter()->RegisterContextContributor(TEXT("Self"), Contributor.ToSharedRef());
    Contributor.Reset(); // 只有真实注册表持有，不能用测试局部SharedPtr掩盖最后一个引用释放。
    int32 Calls = 0;
    const auto Provider = Fixture.Coordinator()->RegisterProvider(TEXT("ContributorLifetime"), MAX_int32,
        FGamePlatformPresentationProviderHandler::CreateLambda([&](const FGamePlatformPresentationRequest&) { ++Calls; return true; }));
    ON_SCOPE_EXIT { Fixture.Coordinator()->UnregisterProvider(Provider); };
    TestEqual(TEXT("自注销贡献仍能完成本作用域提交"),
        Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(FGuid::NewGuid())), EGamePlatformPresentationSubmitResult::Submitted);
    TestFalse(TEXT("virtual仍在执行时贡献者不会析构"), State->bDestroyedInsideCall);
    TestTrue(TEXT("调用返回后自注销对象已释放"), State->bDestroyed);
    TestEqual(TEXT("真实Provider受理一次"), Calls, 1);
    return true;
}

/** virtual回调实际Deinitialize/Viewport换World：原事实不进入后继协调器，下一World同ID仍可受理。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobaPresentationContributorScopeReentryTest,
    "Moba.Presentation.Client.ContributorScopeReentry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMobaPresentationContributorScopeReentryTest::RunTest(const FString&)
{
    for (const bool bTravel : {false, true})
    {
        FMobaContributorFixture Fixture; if (!Fixture.Initialize(*this)) return false;
        int32 Calls = 0; bool bFirst = true;
        const auto Provider = Fixture.Coordinator()->RegisterProvider(TEXT("ContributorScope"), MAX_int32,
            FGamePlatformPresentationProviderHandler::CreateLambda([&](const FGamePlatformPresentationRequest&) { ++Calls; return true; }));
        ON_SCOPE_EXIT { Fixture.Coordinator()->UnregisterProvider(Provider); };
        auto State = MakeShared<FContributorObservation>();
        Fixture.Adapter()->RegisterContextContributor(TEXT("Scope"), MakeShared<FReentrantMobaContributor>(State,
            [&](FMobaPresentationContext&) { if (!bFirst) return; bFirst = false;
                if (bTravel) Fixture.Travel(); else Fixture.Adapter()->Deinitialize(); }));
        const FGuid Id = FGuid::NewGuid();
        TestEqual(TEXT("virtual接管后原事实明确拒绝"), Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id)),
            EGamePlatformPresentationSubmitResult::InvalidRequest);
        TestEqual(TEXT("旧事实未进入Provider"), Calls, 0);
        if (bTravel)
        {
            TestEqual(TEXT("后继World的同ID事实可真实受理"), Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id)),
                EGamePlatformPresentationSubmitResult::Submitted);
            TestEqual(TEXT("只播放后继事实一次"), Calls, 1);
        }
    }
    return true;
}

/** 真实Provider同步旅行后提交同ID后继再拒旧请求；旧失败不能撤掉后继的已受理去重账本。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobaPresentationFactRollbackOwnershipTest,
    "Moba.Presentation.Client.FactRollbackKeepsSuccessor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMobaPresentationFactRollbackOwnershipTest::RunTest(const FString&)
{
    FMobaContributorFixture Fixture; if (!Fixture.Initialize(*this)) return false;
    int32 Calls = 0; bool bFirst = true; bool bSuccessor = false;
    const FGuid Id = FGuid::NewGuid();
    const auto Provider = Fixture.Coordinator()->RegisterProvider(TEXT("FactRollback"), MAX_int32,
        FGamePlatformPresentationProviderHandler::CreateLambda([&](const FGamePlatformPresentationRequest&)
        {
            ++Calls; if (!bFirst) return true; bFirst = false;
            Fixture.Travel();
            bSuccessor = Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id)) == EGamePlatformPresentationSubmitResult::Submitted;
            return false;
        }));
    ON_SCOPE_EXIT { Fixture.Coordinator()->UnregisterProvider(Provider); };
    TestTrue(TEXT("旧作用域请求未成为成功受理"),
        Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id)) != EGamePlatformPresentationSubmitResult::Submitted);
    TestTrue(TEXT("真实Provider中后继事实受理"), bSuccessor);
    TestEqual(TEXT("旧一次和后继一次Provider调用"), Calls, 2);
    TestEqual(TEXT("再次同ID只返回既有受理结果"), Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id)),
        EGamePlatformPresentationSubmitResult::Submitted);
    TestEqual(TEXT("旧失败没有擦去后继去重记录"), Calls, 2);
    return true;
}

/** 同World预测Provider尚未返回时确认重入；预测拒绝后确认必须走真实Provider，不留下伪成功去重。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobaPresentationPredictionRejectedConfirmationReentryTest,
    "Moba.Presentation.Client.PredictionRejectedConfirmationReentry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMobaPresentationPredictionRejectedConfirmationReentryTest::RunTest(const FString&)
{
    FMobaContributorFixture Fixture; if (!Fixture.Initialize(*this)) return false;
    const FGuid Id = FGuid::NewGuid();
    int32 PredictedCalls = 0; int32 ConfirmedCalls = 0;
    EGamePlatformPresentationSubmitResult NestedResult = EGamePlatformPresentationSubmitResult::InvalidRequest;
    const auto Provider = Fixture.Coordinator()->RegisterProvider(TEXT("PredictionRejectedConfirmation"), MAX_int32,
        FGamePlatformPresentationProviderHandler::CreateLambda([&](const FGamePlatformPresentationRequest& Request)
        {
            if (Request.PredictionState == EGamePlatformPresentationPredictionState::Predicted)
            {
                ++PredictedCalls;
                NestedResult = Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id));
                TestEqual(TEXT("预测尚未决议时确认不抢跑Provider"), ConfirmedCalls, 0);
                return false; // 真实拒绝原预测，由适配器在原栈返回后提交留存确认。
            }
            ++ConfirmedCalls;
            return true;
        }));
    ON_SCOPE_EXIT { Fixture.Coordinator()->UnregisterProvider(Provider); };
    auto Predicted = MakeContributorFact(Id); Predicted.Identity.bPredicted = true; Predicted.Identity.bConfirmed = false;
    TestEqual(TEXT("原预测保留真实拒绝结果"), Fixture.Adapter()->SubmitAdaptedFact(Predicted),
        EGamePlatformPresentationSubmitResult::ProviderMissing);
    TestEqual(TEXT("嵌套确认明确返回在途Pending"), NestedResult, EGamePlatformPresentationSubmitResult::Pending);
    TestEqual(TEXT("预测Provider只调用一次"), PredictedCalls, 1);
    TestEqual(TEXT("确认在预测拒绝后真正调用Provider"), ConfirmedCalls, 1);
    TestEqual(TEXT("真正受理的后续重复返回Submitted"), Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id)),
        EGamePlatformPresentationSubmitResult::Submitted);
    TestEqual(TEXT("已受理确认没有重复播放"), ConfirmedCalls, 1);
    return true;
}

/** 同World预测真实接受时，回调中留存的确认仅升级去重身份；不把预测加确认播放两次。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobaPresentationPredictionAcceptedConfirmationReentryTest,
    "Moba.Presentation.Client.PredictionAcceptedConfirmationReentry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMobaPresentationPredictionAcceptedConfirmationReentryTest::RunTest(const FString&)
{
    FMobaContributorFixture Fixture; if (!Fixture.Initialize(*this)) return false;
    const FGuid Id = FGuid::NewGuid(); int32 Calls = 0;
    EGamePlatformPresentationSubmitResult NestedResult = EGamePlatformPresentationSubmitResult::InvalidRequest;
    const auto Provider = Fixture.Coordinator()->RegisterProvider(TEXT("PredictionAcceptedConfirmation"), MAX_int32,
        FGamePlatformPresentationProviderHandler::CreateLambda([&](const FGamePlatformPresentationRequest& Request)
        {
            ++Calls;
            if (Request.PredictionState == EGamePlatformPresentationPredictionState::Predicted)
                NestedResult = Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id));
            return true;
        }));
    ON_SCOPE_EXIT { Fixture.Coordinator()->UnregisterProvider(Provider); };
    auto Predicted = MakeContributorFact(Id); Predicted.Identity.bPredicted = true; Predicted.Identity.bConfirmed = false;
    TestEqual(TEXT("原预测真实受理"), Fixture.Adapter()->SubmitAdaptedFact(Predicted), EGamePlatformPresentationSubmitResult::Submitted);
    TestEqual(TEXT("预测决议前的确认明确Pending"), NestedResult, EGamePlatformPresentationSubmitResult::Pending);
    TestEqual(TEXT("留存确认不再次调用Provider"), Calls, 1);
    TestEqual(TEXT("升级后的确认重复仍指向已有受理"), Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id)),
        EGamePlatformPresentationSubmitResult::Submitted);
    TestEqual(TEXT("后续确认没有双播放"), Calls, 1);
    return true;
}

/** 预测与留存确认均被真实Provider拒绝后没有受理账本，后续同身份确认可真正重试。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobaPresentationPredictionConfirmationRetryAfterRefusalTest,
    "Moba.Presentation.Client.PredictionConfirmationRetryAfterRefusal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMobaPresentationPredictionConfirmationRetryAfterRefusalTest::RunTest(const FString&)
{
    FMobaContributorFixture Fixture; if (!Fixture.Initialize(*this)) return false;
    const FGuid Id = FGuid::NewGuid(); int32 Calls = 0; bool bAcceptConfirmation = false;
    EGamePlatformPresentationSubmitResult NestedResult = EGamePlatformPresentationSubmitResult::InvalidRequest;
    const auto Provider = Fixture.Coordinator()->RegisterProvider(TEXT("PredictionConfirmationRetry"), MAX_int32,
        FGamePlatformPresentationProviderHandler::CreateLambda([&](const FGamePlatformPresentationRequest& Request)
        {
            ++Calls;
            if (Request.PredictionState == EGamePlatformPresentationPredictionState::Predicted)
            {
                NestedResult = Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id));
                return false;
            }
            return bAcceptConfirmation;
        }));
    ON_SCOPE_EXIT { Fixture.Coordinator()->UnregisterProvider(Provider); };
    auto Predicted = MakeContributorFact(Id); Predicted.Identity.bPredicted = true; Predicted.Identity.bConfirmed = false;
    TestEqual(TEXT("预测明确拒绝"), Fixture.Adapter()->SubmitAdaptedFact(Predicted), EGamePlatformPresentationSubmitResult::ProviderMissing);
    TestEqual(TEXT("嵌套确认明确在途"), NestedResult, EGamePlatformPresentationSubmitResult::Pending);
    TestEqual(TEXT("留存确认也真实调用并拒绝"), Calls, 2);
    bAcceptConfirmation = true;
    TestEqual(TEXT("后续确认真正重试并受理"), Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id)),
        EGamePlatformPresentationSubmitResult::Submitted);
    TestEqual(TEXT("失败预约没有永久屏蔽重试"), Calls, 3);
    Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id));
    TestEqual(TEXT("重试真实成功后才去重"), Calls, 3);
    return true;
}

/** 在途确认随后遇到真实关闭/旅行时取消原资格，不向新作用域偷偷排放旧确认。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobaPresentationPredictionPendingScopeCancellationTest,
    "Moba.Presentation.Client.PredictionPendingScopeCancellation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMobaPresentationPredictionPendingScopeCancellationTest::RunTest(const FString&)
{
    for (const bool bTravel : {false, true})
    {
        FMobaContributorFixture Fixture; if (!Fixture.Initialize(*this)) return false;
        const FGuid Id = FGuid::NewGuid(); int32 Calls = 0; bool bFirst = true;
        EGamePlatformPresentationSubmitResult NestedResult = EGamePlatformPresentationSubmitResult::InvalidRequest;
        const auto Provider = Fixture.Coordinator()->RegisterProvider(TEXT("PredictionPendingCancellation"), MAX_int32,
            FGamePlatformPresentationProviderHandler::CreateLambda([&](const FGamePlatformPresentationRequest&)
            {
                ++Calls;
                if (!bFirst) return true;
                bFirst = false;
                NestedResult = Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id));
                if (bTravel) Fixture.Travel(); else Fixture.Adapter()->Deinitialize();
                return false;
            }));
        ON_SCOPE_EXIT { Fixture.Coordinator()->UnregisterProvider(Provider); };
        auto Predicted = MakeContributorFact(Id); Predicted.Identity.bPredicted = true; Predicted.Identity.bConfirmed = false;
        TestEqual(TEXT("失效预测栈返回StaleWorld"), Fixture.Adapter()->SubmitAdaptedFact(Predicted),
            EGamePlatformPresentationSubmitResult::StaleWorld);
        TestEqual(TEXT("关闭/旅行前确认是Pending"), NestedResult, EGamePlatformPresentationSubmitResult::Pending);
        TestEqual(TEXT("旧确认未在失效后排放"), Calls, 1);
        const auto Successor = Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id));
        TestEqual(TEXT("关闭拒绝或新World真实受理"), Successor, bTravel
            ? EGamePlatformPresentationSubmitResult::Submitted : EGamePlatformPresentationSubmitResult::InvalidRequest);
        TestEqual(TEXT("只有有效后继才另调Provider"), Calls, bTravel ? 2 : 1);
    }
    return true;
}

/** 真实SetGameState通知在同World重绑；旧在途确认取消，新绑定同ID须真正受理且不被旧栈撤销。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobaPresentationPredictionArenaRebindKeepsSuccessorTest,
    "Moba.Presentation.Client.PredictionArenaRebindKeepsSuccessor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMobaPresentationPredictionArenaRebindKeepsSuccessorTest::RunTest(const FString&)
{
    FMobaContributorFixture Fixture; if (!Fixture.Initialize(*this)) return false;
    const TStrongObjectPtr<AGamePlatformArenaGameState> OriginalState(Fixture.First->SpawnActor<AGamePlatformArenaGameState>());
    const TStrongObjectPtr<AGamePlatformArenaGameState> SuccessorState(Fixture.First->SpawnActor<AGamePlatformArenaGameState>());
    if (!TestNotNull(TEXT("原竞技GameState"), OriginalState.Get()) ||
        !TestNotNull(TEXT("后继竞技GameState"), SuccessorState.Get())) return false;
    Fixture.First->SetGameState(OriginalState.Get());
    Fixture.Adapter()->RefreshBindings();
    const FGuid Id = FGuid::NewGuid(); int32 SameIdCalls = 0; bool bFirst = true;
    EGamePlatformPresentationSubmitResult OldConfirmation = EGamePlatformPresentationSubmitResult::InvalidRequest;
    EGamePlatformPresentationSubmitResult Successor = EGamePlatformPresentationSubmitResult::InvalidRequest;
    const auto Provider = Fixture.Coordinator()->RegisterProvider(TEXT("PredictionArenaRebind"), MAX_int32,
        FGamePlatformPresentationProviderHandler::CreateLambda([&](const FGamePlatformPresentationRequest& Request)
        {
            if (Request.RequestId != Id) return true; // 新GS恢复的其他竞技事实不是本用例同ID计数。
            ++SameIdCalls;
            if (!bFirst) return true;
            bFirst = false;
            OldConfirmation = Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id));
            Fixture.First->SetGameState(SuccessorState.Get()); // 真实GameStateSetEvent→RefreshBindings，非手工推进代次。
            Successor = Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id));
            return false;
        }));
    ON_SCOPE_EXIT { Fixture.Coordinator()->UnregisterProvider(Provider); };
    auto Predicted = MakeContributorFact(Id); Predicted.Identity.bPredicted = true; Predicted.Identity.bConfirmed = false;
    TestEqual(TEXT("原竞技绑定的预测栈失效"), Fixture.Adapter()->SubmitAdaptedFact(Predicted),
        EGamePlatformPresentationSubmitResult::StaleWorld);
    TestEqual(TEXT("旧确认最初只留在途"), OldConfirmation, EGamePlatformPresentationSubmitResult::Pending);
    TestEqual(TEXT("World实际已换GameState"), Fixture.First->GetGameState(), static_cast<AGameStateBase*>(SuccessorState.Get()));
    TestEqual(TEXT("新竞技绑定同ID事实真正受理"), Successor, EGamePlatformPresentationSubmitResult::Submitted);
    TestEqual(TEXT("原预测和后继确认各真实调用一次"), SameIdCalls, 2);
    TestEqual(TEXT("旧栈未擦后继同ID受理账本"), Fixture.Adapter()->SubmitAdaptedFact(MakeContributorFact(Id)),
        EGamePlatformPresentationSubmitResult::Submitted);
    TestEqual(TEXT("旧确认取消且后继重复不双播"), SameIdCalls, 2);
    return true;
}

/** 首条竞技持续事实的真实Provider关闭服务并改GS数组；原绑定不读失效元素，也不尾写第二项修订。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobaPresentationArenaArrayReentryTest,
    "Moba.Presentation.Client.ArenaArrayReentry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMobaPresentationArenaArrayReentryTest::RunTest(const FString&)
{
    // 三条真实入口分别覆盖首次Bind恢复、Team更新与Objective更新；每次使用独立Player/World作用域。
    for (int32 Entry = 0; Entry < 3; ++Entry)
    {
        FMobaContributorFixture Fixture; if (!Fixture.Initialize(*this)) return false;
        const TStrongObjectPtr<AGamePlatformArenaGameState> State(Fixture.First->SpawnActor<AGamePlatformArenaGameState>());
        if (!TestNotNull(TEXT("真实竞技GameState"), State.Get())) return false;
        State->MatchIdPublic = TEXT("ArrayReentry");
        // Spawn/PostInitialize可已经自动设GameState并绑定空数组；重新建立本用例的精确入口前提。
        Fixture.Adapter()->UnbindArena();
        if (Entry != 0) Fixture.Adapter()->BindArena(State.Get());
        FGamePlatformArenaTeamState FirstTeam; FirstTeam.TeamId = TEXT("First"); FirstTeam.Revision = 1;
        FGamePlatformArenaTeamState SecondTeam; SecondTeam.TeamId = TEXT("Second"); SecondTeam.Revision = 1;
        FGamePlatformArenaObjectiveState FirstObjective; FirstObjective.ObjectiveId = TEXT("FirstObjective"); FirstObjective.Revision = 1;
        FGamePlatformArenaObjectiveState SecondObjective; SecondObjective.ObjectiveId = TEXT("SecondObjective"); SecondObjective.Revision = 1;
        State->TeamStates = {FirstTeam, SecondTeam}; State->ObjectiveStates = {FirstObjective, SecondObjective};
        int32 Calls = 0;
        const auto Provider = Fixture.Coordinator()->RegisterProvider(TEXT("ArenaArray"), MAX_int32,
            FGamePlatformPresentationProviderHandler::CreateLambda([&](const FGamePlatformPresentationRequest&)
            { ++Calls; Fixture.Adapter()->Deinitialize(); State->TeamStates.Reset(); State->ObjectiveStates.Reset(); return true; }));
        ON_SCOPE_EXIT { Fixture.Coordinator()->UnregisterProvider(Provider); };
        if (Entry == 0) Fixture.Adapter()->BindArena(State.Get());
        else if (Entry == 1) Fixture.Adapter()->HandleArenaTeamStatesChanged();
        else Fixture.Adapter()->HandleArenaObjectiveStatesChanged();
        TestEqual(TEXT("关闭后不继续第二条批次事实"), Calls, 1);
        TestFalse(TEXT("旧绑定不尾写第二队修订"), Fixture.Adapter()->TeamRevisions.Contains(TEXT("Second")));
        TestFalse(TEXT("旧绑定不尾写第二目标修订"), Fixture.Adapter()->ObjectiveRevisions.Contains(TEXT("SecondObjective")));
    }
    return true;
}
#endif
