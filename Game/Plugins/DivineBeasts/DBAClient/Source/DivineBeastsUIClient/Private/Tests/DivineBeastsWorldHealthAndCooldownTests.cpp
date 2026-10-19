// 项目UI真实资源事件回归：只验证隔离实例的UMG投影、清理及冷却材质参数，不连接后端。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/GamePlatformSlotWidget.h"
#include "WorldUI/GamePlatformWorldNameplateWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/StrongObjectPtr.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "WorldUI/DivineBeastsWorldHealthBarPresenter.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Characters/DivineBeastsVillageWolf.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Services/GamePlatformWorldUIService.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Gameplay/DivineBeastsWorldPlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsWorldHealthResourceTest,
    "DivineBeasts.UI.World.HealthBar.ResourceEventProjection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsWorldHealthResourceTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
    UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    auto* Class = LoadClass<UGamePlatformWorldNameplateWidget>(nullptr,
        TEXT("/DBAUIPack_Core/UI/World/WBP_DBA_WorldHealthBar.WBP_DBA_WorldHealthBar_C"));
    if (!TestNotNull(TEXT("真实头顶血条类"), Class)) return false;
    TStrongObjectPtr<UGamePlatformWorldNameplateWidget> Widget(CreateWidget<UGamePlatformWorldNameplateWidget>(Instance.Get(), Class));
    if (!TestNotNull(TEXT("真实名称板实例"), Widget.Get())) return false;
    auto* Fill = Cast<UProgressBar>(Widget->GetWidgetFromName(TEXT("ResourceFill")));
    if (!TestNotNull(TEXT("真实命名生命填充"), Fill)) return false;
    FGamePlatformWorldUIRequest Request; Request.RequestId = FGuid::NewGuid(); Request.SurfaceKey = TEXT("HealthTest");
    Request.Resource.MaximumValue = 300.; Request.Resource.CurrentValue = 300.;
    Widget->ApplyWorldUIRequest(Request); TestEqual(TEXT("初始化事件显示真实满血"), Fill->GetPercent(), 1.f);
    auto* SelectionOutline = Cast<UImage>(Widget->GetWidgetFromName(TEXT("SelectionOutline")));
    auto* SelectionMarker = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("SelectionMarker")));
    if (!TestNotNull(TEXT("选中状态有实际边框"), SelectionOutline) || !TestNotNull(TEXT("选中状态有非纯色标记"), SelectionMarker)) return false;
    const FLinearColor OriginalFill = Fill->GetFillColorAndOpacity();
    Request.bSelected = true; Request.SelectionTint = FLinearColor(1.f, .12f, .08f);
    Widget->ApplyWorldUIRequest(Request);
    TestEqual(TEXT("选中事件着色真实生命条"), Fill->GetFillColorAndOpacity(), Request.SelectionTint);
    TestEqual(TEXT("选中事件显示实际边框"), SelectionOutline->GetVisibility(), ESlateVisibility::HitTestInvisible);
    TestFalse(TEXT("选中事件保留非颜色识别文字"), SelectionMarker->GetText().IsEmpty());
    Request.bSelected = false; Widget->ApplyWorldUIRequest(Request);
    TestEqual(TEXT("切换目标后恢复原生命条颜色"), Fill->GetFillColorAndOpacity(), OriginalFill);
    TestEqual(TEXT("旧目标立即清除边框"), SelectionOutline->GetVisibility(), ESlateVisibility::Collapsed);
    TestEqual(TEXT("旧目标立即清除标记"), SelectionMarker->GetVisibility(), ESlateVisibility::Collapsed);
    Request.Resource.CurrentValue = 90.; Widget->ApplyWorldUIRequest(Request);
    TestEqual(TEXT("伤害事件立即更新30%血条，不等待Tick"), Fill->GetPercent(), .3f);
    Request.Resource.MaximumValue = 0.; Widget->ApplyWorldUIRequest(Request);
    TestEqual(TEXT("无合法上限不能伪装满血"), Fill->GetPercent(), 0.f);
    // 无Ticker时首次事件必须完成排版，不能等镜头移动才修正零尺寸偏移。
    // 离屏夹具必须像真实Overlay一样持有Slate表面；UMG只缓存弱引用，
    // 丢弃TakeWidget返回值会销毁Slate实例，预排版自然无法得到有效尺寸。
    const TSharedRef<SWidget> NameplateSlate = Widget->TakeWidget();
    Widget->SetProjectedState(FVector2D(500, 300), true, 100.f);
    TestTrue(TEXT("静止镜头首投影获得真实160x44尺寸"), Widget->GetDesiredSize().Equals(FVector2D(160, 44), .01));
    const float Scale = UWidgetLayoutLibrary::GetViewportScale(Widget.Get());
    TestTrue(TEXT("首投影按DPI居中并置于目标上方"), Widget->GetRenderTransform().Translation.Equals(
        FVector2D(500, 300) / FMath::Max(Scale, .001f) - FVector2D(80, 44), .01));
    Widget->SetProjectedState(FVector2D::ZeroVector, false, 100.f);
    Widget->SetProjectedState(FVector2D(700, 400), true, 100.f);
    TestTrue(TEXT("隐藏后重显立即重新排版定位"), Widget->GetRenderTransform().Translation.Equals(
        FVector2D(700, 400) / FMath::Max(Scale, .001f) - FVector2D(80, 44), .01));
    Widget->ResetWorldUIState(); TestFalse(TEXT("回收释放旧显示注册身份"), Widget->GetWorldUIRequest().RequestId.IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCooldownSweepEventTest,
    "DivineBeasts.UI.Combat.CooldownSweep.EventParametersAndClear", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCooldownSweepEventTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString())); UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    auto* Class = LoadClass<UGamePlatformSlotWidget>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilitySlot.WBP_DBA_UI_AbilitySlot_C"));
    if (!TestNotNull(TEXT("真实技能槽类"), Class)) return false;
    TStrongObjectPtr<UGamePlatformSlotWidget> Slot(CreateWidget<UGamePlatformSlotWidget>(Instance.Get(), Class));
    if (!TestNotNull(TEXT("真实技能槽实例"), Slot.Get())) return false;
    FGamePlatformUISlotState State; State.SlotId = TEXT("SkillTest"); State.ContentId = TEXT("SpellTest"); State.bEnabled = false;
    Slot->ApplySlotState(State); Slot->ApplyOverlayTiming(123.f, 3.f);
    auto* Sweep = Cast<UImage>(Slot->GetWidgetFromName(TEXT("CooldownSweep")));
    if (!TestNotNull(TEXT("真实旋转遮罩Image"), Sweep)) return false;
    auto* Material = Sweep->GetDynamicMaterial(); if (!TestNotNull(TEXT("每实例动态材质"), Material)) return false;
    TestEqual(TEXT("事件设置不可重启的本地截止时刻"), Material->K2_GetScalarParameterValue(TEXT("CooldownEndsAt")), 123.f);
    TestEqual(TEXT("事件保留权威复制的总冷却"), Material->K2_GetScalarParameterValue(TEXT("CooldownDuration")), 3.f);
    TestEqual(TEXT("遮罩不拦截技能输入"), Sweep->GetVisibility(), ESlateVisibility::HitTestInvisible);
    Slot->ApplyOverlayTiming(0.f, 0.f);
    TestEqual(TEXT("终态事件立即清除遮罩"), Sweep->GetVisibility(), ESlateVisibility::Collapsed);
    TestEqual(TEXT("清理不会放宽冷却技能输入资格"), Slot->GetSlotState().bEnabled, false);
    return true;
}
// 仅验证观察生命周期：夹具注入本地作用域且不装配屏幕服务，不声称联机准入或渲染成功。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsWorldHealthBarLifecycleTest,
    "DivineBeasts.UI.World.HealthBar.PossessionEndPlayAndClear", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsWorldHealthBarLifecycleTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString())); UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    // Controller在PostInitializeComponents才进入World控制器列表；真实本地玩家查找依赖该列表。
    World->InitializeActorsForPlay(FURL());
    auto* Controller = World->SpawnActor<APlayerController>();
    TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine)); Controller->SetPlayer(Player.Get());
    if (!TestEqual(TEXT("夹具本地玩家可从真实World控制器列表解析"), Player->GetPlayerController(World), Controller)) return false;
    auto* OldPawn = World->SpawnActor<ADivineBeastsCharacter>();
    auto* NewPawn = World->SpawnActor<ADivineBeastsCharacter>(); Controller->Possess(OldPawn);
    if (!TestTrue(TEXT("夹具真实控制旧角色"), Controller->GetPawn() == OldPawn)) return false;
    TStrongObjectPtr<UDivineBeastsWorldHealthBarPresenter> Presenter(NewObject<UDivineBeastsWorldHealthBarPresenter>());
    ON_SCOPE_EXIT { Presenter->Clear(); };
    Presenter->ObservedWorld = World; Presenter->LocalPlayer = Player.Get(); Presenter->ExcludedLocalPawn = OldPawn;
    auto* Class = LoadClass<UGamePlatformWorldWidgetBase>(nullptr,
        TEXT("/DBAUIPack_Core/UI/World/WBP_DBA_WorldHealthBar.WBP_DBA_WorldHealthBar_C"));
    Presenter->WidgetClass = Class;
    Presenter->ObserveActor(OldPawn); TestEqual(TEXT("自身不会加入世界血条"), Presenter->GetObservedCount(), 0);
    Presenter->ObserveActor(NewPawn); TestEqual(TEXT("先Spawn的其他角色被事件观察"), Presenter->GetObservedCount(), 1);
    Controller->Possess(NewPawn);
    if (!TestTrue(TEXT("夹具真实切换到新角色"), Controller->GetPawn() == NewPawn)) return false;
    Presenter->Initialize(Player.Get(), World, Class);
    TestFalse(TEXT("同世界Possess后移除新自身"), Presenter->Bindings.Contains(NewPawn));
    TestTrue(TEXT("不再次扫描世界仍重新观察旧角色"), Presenter->Bindings.Contains(OldPawn));
    // 明确模拟引擎流送EndPlay通知，不调用Destroy，以捕捉只绑定OnDestroyed的旧错误。
    OldPawn->OnEndPlay.Broadcast(OldPawn, EEndPlayReason::RemovedFromWorld);
    TestEqual(TEXT("仅EndPlay也解除全部实体观察"), Presenter->GetObservedCount(), 0);
    Presenter->ObserveActor(OldPawn); Presenter->Clear();
    OldPawn->SetActorLocation(FVector(50, 0, 0)); OldPawn->OnDestroyed.Broadcast(OldPawn);
    TestEqual(TEXT("清理后迟到位置与销毁通知不恢复旧显示"), Presenter->GetObservedCount(), 0);
    return true;
}
// 破坏目标事件订阅、死亡撤销或旧来源防护都会使本用例失败；真实控制器与Presenter，不创建第二份选中真源。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsWorldTargetSelectionTest,
    "DivineBeasts.UI.World.TargetSelection.EventsDeathAndClear", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsWorldTargetSelectionTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString())); UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    World->InitializeActorsForPlay(FURL());
    auto* Controller = World->SpawnActor<ADivineBeastsWorldPlayerController>();
    TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine)); Controller->SetPlayer(Player.Get());
    auto* Pawn = World->SpawnActor<ADivineBeastsCharacter>(); Controller->Possess(Pawn);
    auto* First = World->SpawnActor<ADivineBeastsCharacter>();
    auto* Second = World->SpawnActor<ADivineBeastsCharacter>();
    TStrongObjectPtr<UDivineBeastsWorldHealthBarPresenter> Presenter(NewObject<UDivineBeastsWorldHealthBarPresenter>());
    ON_SCOPE_EXIT { Presenter->Clear(); };
    Presenter->ObservedWorld = World; Presenter->LocalPlayer = Player.Get();
    Presenter->ObserveActor(First); Presenter->ObserveActor(Second);
    Presenter->ApplyHealth(First, 100.f, 100.f); Presenter->ApplyHealth(Second, 100.f, 100.f);
    Controller->SetSelectedWorldTarget(First); // 先选择，再绑定必须读取首次快照。
    Presenter->BindSelectionController(Controller);
    TestTrue(TEXT("静止镜头首次绑定立即突出已有目标"), Presenter->Bindings.FindChecked(First).Request.bSelected);
    TestEqual(TEXT("没有可信关系的目标使用中立金色"), Presenter->Bindings.FindChecked(First).Request.SelectionTint, FLinearColor(1.f, .73f, .18f));
    auto* Wolf = World->SpawnActor<ADivineBeastsVillageWolf>();
    TestEqual(TEXT("正式狼怪目标使用敌对红色"), Presenter->ResolveSelectionTint(Wolf), FLinearColor(1.f, .12f, .08f));
    Presenter->BindSelectionController(Controller); // 重绑不应重复观察或重置选择。
    Controller->SetSelectedWorldTarget(Second);
    TestFalse(TEXT("切换事件立即取消旧目标"), Presenter->Bindings.FindChecked(First).Request.bSelected);
    TestTrue(TEXT("切换事件立即突出新目标"), Presenter->Bindings.FindChecked(Second).Request.bSelected);
    Presenter->ApplyHealth(Second, 0.f, 100.f);
    TestFalse(TEXT("死亡通知立即清选中显示"), Presenter->Bindings.FindChecked(Second).Request.bSelected);
    TestTrue(TEXT("UI不清除Gameplay失效选择意图"), Controller->GetSelectedWorldTarget() == Second);
    Presenter->ApplyHealth(Second, 100.f, 100.f);
    TestFalse(TEXT("复活属性通知不会复活旧选择"), Presenter->Bindings.FindChecked(Second).Request.bSelected);
    Controller->SetSelectedWorldTarget(Second);
    TestTrue(TEXT("同Actor显式重选恢复新展示"), Presenter->Bindings.FindChecked(Second).Request.bSelected);
    Second->OnEndPlay.Broadcast(Second, EEndPlayReason::RemovedFromWorld);
    TestFalse(TEXT("目标流送退出撤销选中引用"), Presenter->SelectedTarget.IsValid());
    Presenter->Clear();
    TestFalse(TEXT("退出解除目标真源订阅"), Controller->OnWorldTargetChanged.IsBoundToObject(Presenter.Get()));
    Controller->SetSelectedWorldTarget(First);
    TestEqual(TEXT("退出后迟到选择不能恢复实体或旧显示"), Presenter->GetObservedCount(), 0);
    return true;
}
// 真实服务与真实两份Root验证就绪通知和替换挂接；只读UI请求不是Gameplay生命事实。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsWorldHealthBarRootReadyTest,
    "DivineBeasts.UI.World.HealthBar.RootReadyAndReplacement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsWorldHealthBarRootReadyTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString())); UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    auto* Controller = World->SpawnActor<APlayerController>();
    TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine)); Controller->SetPlayer(Player.Get());
    auto* RootClass = LoadClass<UGamePlatformUILayerStack>(nullptr, TEXT("/DBAUIPack_Core/UI/Root/WBP_DBA_UI_RootLayout.WBP_DBA_UI_RootLayout_C"));
    auto* BarClass = LoadClass<UGamePlatformWorldWidgetBase>(nullptr, TEXT("/DBAUIPack_Core/UI/World/WBP_DBA_WorldHealthBar.WBP_DBA_WorldHealthBar_C"));
    if (!TestNotNull(TEXT("真实Root类"), RootClass) || !TestNotNull(TEXT("真实血条类"), BarClass)) return false;
    TStrongObjectPtr<UGamePlatformUILayerStack> First(CreateWidget<UGamePlatformUILayerStack>(Controller, RootClass));
    TStrongObjectPtr<UGamePlatformUILayerStack> Second(CreateWidget<UGamePlatformUILayerStack>(Controller, RootClass));
    const TSharedRef<SWidget> FirstSlate = First->TakeWidget(), SecondSlate = Second->TakeWidget();
    auto* FirstLayer = Cast<UOverlay>(First->GetWidgetFromName(TEXT("WorldProjectionLayer")));
    auto* SecondLayer = Cast<UOverlay>(Second->GetWidgetFromName(TEXT("WorldProjectionLayer")));
    if (!TestNotNull(TEXT("第一投影层"), FirstLayer) || !TestNotNull(TEXT("第二投影层"), SecondLayer)) return false;
    TStrongObjectPtr<UGamePlatformWorldUIService> UI(NewObject<UGamePlatformWorldUIService>()); UI->Initialize(Player.Get());
    ON_SCOPE_EXIT { UI->Deinitialize(); };
    FGamePlatformWorldUIRequest Request; Request.RequestId = FGuid::NewGuid(); Request.SurfaceKey = TEXT("RootReadyTest");
    TestFalse(TEXT("Root尚未安装时明确拒绝注册"), UI->RegisterWorldUI(Request, BarClass).IsValid());
    int32 Notifications = 0;
    const auto Handle = UI->OnRootLayoutChanged().AddLambda([&]()
    { ++Notifications; if (!UI->UpdateWorldUI(Request.RequestId, Request)) UI->RegisterWorldUI(Request, BarClass); });
    ON_SCOPE_EXIT { UI->OnRootLayoutChanged().Remove(Handle); };
    UI->SetRootLayout(First.Get()); TestEqual(TEXT("就绪事件重试后真实挂接一项"), FirstLayer->GetChildrenCount(), 1);
    UI->SetRootLayout(Second.Get());
    TestEqual(TEXT("Root替换移除旧投影层"), FirstLayer->GetChildrenCount(), 0);
    TestEqual(TEXT("原请求仅存在于新投影层，无重复实例"), SecondLayer->GetChildrenCount(), 1);
    TestEqual(TEXT("两次真实Root变化各通知一次"), Notifications, 2);
    TestTrue(TEXT("原请求身份仍可明确注销"), UI->UnregisterWorldUI(Request.RequestId));
    return true;
}
#endif
