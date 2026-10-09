// 本文件属于MobaCommon可选MOBA层 MobaPresentation，负责回归用例；夹具仅测试作用域，不伪造生产资源成功。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// F13：真实Controller Possess/UnPossess驱动本地事实订阅；仅Transient组件夹具，不产生英雄资产或网络事实。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "MobaPresentationClientSubsystem.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "UObject/UnrealType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/ScopeExit.h"
#include "Tags/MobaPresentationTags.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobaPresentationPawnBindingRegressionTest,
    "Moba.Presentation.Client.LatePawnAndRespawnBindings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMobaPresentationPawnBindingRegressionTest::RunTest(const FString&)
{
    // LocalPlayer与Viewport的ClassWithin为Engine；仅用真实GEngine作Outer，缺引擎明确失败。
    if (!TestNotNull(TEXT("测试宿主Engine必须存在"), GEngine)) { return false; }
    const TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
    // 真实Controller/Pawn生命周期需要Actor初始化；只建World不会进入PostInitializeComponents。
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    const TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    const TStrongObjectPtr<UGameViewportClient> Viewport(NewObject<UGameViewportClient>(GEngine));
    auto* ViewportWorld = FindFProperty<FObjectPropertyBase>(Viewport->GetClass(), TEXT("World"));
    if (!TestNotNull(TEXT("引擎Viewport世界字段用于Transient接线夹具"), ViewportWorld)) return false;
    ViewportWorld->SetObjectPropertyValue_InContainer(Viewport.Get(), World.Get()); Player->ViewportClient = Viewport.Get();
    auto* Controller = World->SpawnActor<APlayerController>();
    // GetPlayerController(World)遍历真实Controller并核其LocalPlayer；只写PlayerController字段不足以建立双向拥有。
    Controller->SetPlayer(Player.Get()); Player->PlayerController = Controller;
    const TStrongObjectPtr<UMobaPresentationClientSubsystem> Service(NewObject<UMobaPresentationClientSubsystem>(Player.Get()));
    Service->BoundWorld = World.Get(); Service->BindController(Controller);
    auto* First = World->SpawnActor<APawn>(); auto* Second = World->SpawnActor<APawn>();
    auto* FirstCombat = NewObject<UGamePlatformCombatComponent>(First); First->AddInstanceComponent(FirstCombat); FirstCombat->RegisterComponent();
    auto* SecondCombat = NewObject<UGamePlatformCombatComponent>(Second); Second->AddInstanceComponent(SecondCombat); SecondCombat->RegisterComponent();
    TestNull(TEXT("尚未Possess不会伪造Combat订阅"), Service->BoundCombatComponent.Get());
    Controller->Possess(First);
    TestEqual(TEXT("迟到Pawn自动接线"), Service->BoundCombatComponent.Get(), FirstCombat);
    Controller->Possess(Second);
    TestEqual(TEXT("重生Pawn自动替换旧订阅"), Service->BoundCombatComponent.Get(), SecondCombat);
    Controller->UnPossess(); TestNull(TEXT("取消占有解绑旧Combat"), Service->BoundCombatComponent.Get());
    FMobaPresentationAdaptedFact Fact; Fact.Identity.FactId = FGuid::NewGuid(); Fact.Semantic = MobaPresentationTags::Combat_Hit;
    TestEqual(TEXT("缺平台提供者明确失败"), Service->SubmitAdaptedFact(Fact), EGamePlatformPresentationSubmitResult::ProviderMissing);
    TestEqual(TEXT("重复未受理事实不伪装Submitted"), Service->SubmitAdaptedFact(Fact), EGamePlatformPresentationSubmitResult::ProviderMissing);
    Service->HandleActorSpawned(First);
    const auto DeferredTimer = Service->PendingBindingRefreshTimer; const uint64 DeferredGeneration = Service->BindingGeneration;
    TestTrue(TEXT("出生接线持有可取消的下一轮Timer"), DeferredTimer.IsValid());
    Service->Deinitialize();
    TestFalse(TEXT("关停取消自身延后Timer"), World->GetTimerManager().TimerExists(DeferredTimer));
    Service->HandleDeferredBindingRefresh(DeferredGeneration, World.Get());
    Service->RefreshBindings(); World->GetTimerManager().Tick(0.1f);
    TestNull(TEXT("关停后旧回调和显式Refresh不能重新绑定Controller"), Service->BoundController.Get());
    TestNull(TEXT("关停后旧回调不能重新绑定Combat"), Service->BoundCombatComponent.Get());
    TestFalse(TEXT("关停不保留Timer身份"), Service->PendingBindingRefreshTimer.IsValid());
    Player->PlayerController = nullptr;
    return true;
}

// 真实两个Transient World与LocalPlayer子系统提交；测试Provider只证明受理/代次，不宣称VFX资产播放。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMobaPresentationTravelGenerationRegressionTest,
    "Moba.Presentation.Client.FirstFactAfterTravelUsesCurrentCoordinatorGeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMobaPresentationTravelGenerationRegressionTest::RunTest(const FString&)
{
    // LocalPlayer与Viewport的ClassWithin为Engine；仅用真实GEngine作Outer，缺引擎明确失败。
    if (!TestNotNull(TEXT("测试宿主Engine必须存在"), GEngine)) { return false; }
    const TStrongObjectPtr<UWorld> FirstWorld(UWorld::CreateWorld(EWorldType::Game, false));
    const TStrongObjectPtr<UWorld> SecondWorld(UWorld::CreateWorld(EWorldType::Game, false));
    // 任一结构前置失败也必须释放本次两世界；本用例不触发服务器分配或正式地图加载。
    ON_SCOPE_EXIT { FirstWorld->DestroyWorld(false); SecondWorld->DestroyWorld(false); };
    const TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
    const TStrongObjectPtr<UGameViewportClient> Viewport(NewObject<UGameViewportClient>(GEngine));
    const TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
    auto* WorldProperty = FindFProperty<FObjectPropertyBase>(Viewport->GetClass(), TEXT("World"));
    auto* InstanceProperty = FindFProperty<FObjectPropertyBase>(Viewport->GetClass(), TEXT("GameInstance"));
    if (!TestNotNull(TEXT("真实Viewport World合同"), WorldProperty) || !TestNotNull(TEXT("真实Viewport GameInstance合同"), InstanceProperty)) return false;
    FirstWorld->SetGameInstance(GameInstance.Get()); SecondWorld->SetGameInstance(GameInstance.Get());
    WorldProperty->SetObjectPropertyValue_InContainer(Viewport.Get(), FirstWorld.Get());
    InstanceProperty->SetObjectPropertyValue_InContainer(Viewport.Get(), GameInstance.Get());
    Player->PlayerAdded(Viewport.Get(), 0);
    ON_SCOPE_EXIT { Player->PlayerRemoved(); };
    auto* Coordinator = Player->GetSubsystem<UGamePlatformPresentationClientSubsystem>();
    auto* Adapter = Player->GetSubsystem<UMobaPresentationClientSubsystem>();
    if (!TestNotNull(TEXT("真实平台协调器已初始化"), Coordinator) || !TestNotNull(TEXT("真实MOBA适配器已初始化"), Adapter))
    { return false; }
    int32 ObservedGeneration = 0; bool bMatchingGenerations = false;
    const auto Provider = Coordinator->RegisterProvider(TEXT("TravelAdmissionTest"), MAX_int32,
        FGamePlatformPresentationProviderHandler::CreateLambda([&](const FGamePlatformPresentationRequest& Request)
        { ObservedGeneration = Request.WorldGeneration; bMatchingGenerations = Request.WorldGeneration == Request.Context.WorldGeneration; return true; }));
    FMobaPresentationAdaptedFact Fact; Fact.Identity.FactId = FGuid::NewGuid(); Fact.Semantic = MobaPresentationTags::Combat_Hit;
    TestEqual(TEXT("旅行前事实受理"), Adapter->SubmitAdaptedFact(Fact), EGamePlatformPresentationSubmitResult::Submitted);
    const int32 PreviousGeneration = ObservedGeneration;
    WorldProperty->SetObjectPropertyValue_InContainer(Viewport.Get(), SecondWorld.Get()); Fact.Identity.FactId = FGuid::NewGuid();
    TestEqual(TEXT("旅行后首次事实不使用缓存世代而被StaleWorld丢弃"), Adapter->SubmitAdaptedFact(Fact), EGamePlatformPresentationSubmitResult::Submitted);
    TestTrue(TEXT("协调器在同一次Submit中刷新并补齐新世界世代"), ObservedGeneration > PreviousGeneration && bMatchingGenerations);
    Coordinator->UnregisterProvider(Provider);
    return true;
}
#endif
