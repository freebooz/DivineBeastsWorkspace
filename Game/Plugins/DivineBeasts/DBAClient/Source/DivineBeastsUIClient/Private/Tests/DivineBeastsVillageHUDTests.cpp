// 新手村HUD显示回归；只验证客户端坐标、真实英雄目录和生命周期隔离，不模拟后端准入或伪造等级。
#include "Components/DivineBeastsVillageMinimapWidget.h"
#include "Components/DivineBeastsPlayerPortraitWidget.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "GameFramework/Character.h"
#include "Components/TextBlock.h"
#include "Components/GamePlatformResourceBarWidget.h"
#include "Layers/DivineBeastsRootLayout.h"
#include "Panels/Combat/DivineBeastsCombatPanelBase.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/GamePlatformCastProgressWidget.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneFloatSection.h"
#include "UObject/UObjectHash.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
// 捕获真实HUD未接施法事件、取消后旧代复活以及动画误改技能时序的回归。
// 独立游戏世界只有本地夹具Pawn/真实复制组件；不连接后端、不激活GAS技能、不修改保存资产。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCastBarEventBindingTest,
    "DivineBeasts.UI.Combat.CastBarEventBinding", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCastBarEventBindingTest::RunTest(const FString&)
{
    UClass* RootClass = LoadClass<UDivineBeastsRootLayout>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Root/WBP_DBA_UI_RootLayout.WBP_DBA_UI_RootLayout_C"));
    if (!TestNotNull(TEXT("真实根布局类"), RootClass) || !TestNotNull(TEXT("Engine"), GEngine)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("独立施法显示世界"), World)) return false;
    TStrongObjectPtr<UDivineBeastsRootLayout> Root(NewObject<UDivineBeastsRootLayout>(GetTransientPackage(), RootClass));
    auto* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("真实本地控制器"), Controller)) { World->DestroyWorld(false); return false; }
    World->AddController(Controller);
    ON_SCOPE_EXIT { Root->NativeDestruct(); World->RemoveController(Controller); World->DestroyWorld(false); };
    auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    Controller->SetPlayer(LocalPlayer);
    APawn* Pawn = World->SpawnActor<APawn>();
    if (!TestNotNull(TEXT("施法者Pawn"), Pawn)) return false;
    auto* ASC = NewObject<UGamePlatformAbilitySystemComponent>(Pawn);
    auto* Combat = NewObject<UGamePlatformCombatComponent>(Pawn);
    auto* CastComponent = NewObject<UGamePlatformAbilityCastComponent>(Pawn);
    Pawn->AddInstanceComponent(ASC); Pawn->AddInstanceComponent(Combat); Pawn->AddInstanceComponent(CastComponent);
    ASC->RegisterComponent(); Combat->RegisterComponent(); CastComponent->RegisterComponent();
    Controller->Possess(Pawn);
    const FGuid FirstId = FGuid::NewGuid();
    TestTrue(TEXT("先于HUD绑定的权威冰箭窗口"), CastComponent->PublishConfirmedCast(
        FirstId, TEXT("ability.test.frostbolt@1"), TEXT("DBA.Development.Frostbolt"), 1, 0.7f));
    Root->Initialize(); Root->SetPlayerContext(FLocalPlayerContext(Controller)); Root->NativeConstruct();
    auto* Bar = Root->CombatHUD ? Cast<UGamePlatformCastProgressWidget>(Root->CombatHUD->GetWidgetFromName(TEXT("CastBar"))) : nullptr;
    if (!TestNotNull(TEXT("真实CombatHUD已嵌套施法条"), Bar)) return false;
    TestTrue(TEXT("首次绑定读取已存在的活跃快照"), Bar->GetCastSnapshot().bActive);
    TestEqual(TEXT("冰箭名称说明确认动作，不声称蓄力"), Bar->GetCastSnapshot().DisplayName.ToString(), FString(TEXT("寒冰箭 · 施法动作")));
    TestTrue(TEXT("保持既有0.7秒窗口"), FMath::IsNearlyEqual(Bar->GetCastSnapshot().TotalSeconds, 0.7, 0.0001));
    TestEqual(TEXT("真实条展开"), Bar->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
    const FGamePlatformAbilityCastFacts OldFacts = CastComponent->GetCastFacts();
    TestTrue(TEXT("取消由真实组件发布"), CastComponent->CancelConfirmedCast(FirstId));
    TestFalse(TEXT("取消事件立即清空活跃状态"), Bar->GetCastSnapshot().bActive);
    TestEqual(TEXT("取消后隐藏"), Bar->GetVisibility(), ESlateVisibility::Collapsed);
    const FGuid SecondId = FGuid::NewGuid();
    TestTrue(TEXT("后继火球窗口"), CastComponent->PublishConfirmedCast(
        SecondId, TEXT("ability.test.fireball@1"), TEXT("DBA.Development.Fireball"), 1, 0.7f));
    const auto SecondSnapshot = Bar->GetCastSnapshot();
    TestTrue(TEXT("后继窗口显示"), SecondSnapshot.bActive);
    CastComponent->OnCastFactsChanged().Broadcast(CastComponent, OldFacts);
    TestEqual(TEXT("迟到旧通知不能覆盖后继实例"), Bar->GetCastSnapshot().CastInstanceId, SecondSnapshot.CastInstanceId);
    Root->RefreshForPlayerController(Controller);
    TestEqual(TEXT("重复绑定不重复消费当前快照"), Bar->GetCastSnapshot().Revision, SecondSnapshot.Revision);
    // 组件自然到期已由GamePlatform.AbilitySystem.Cast.AuthorityIdentityAndExpiry潜伏用例覆盖；
    // 本用例仅验证同一事实终态通道，不在同一引擎帧强推TimerManager伪造到期。
    TestTrue(TEXT("后继终态由真实组件发布"), CastComponent->CancelConfirmedCast(SecondId));
    TestFalse(TEXT("终态通知收敛"), Bar->GetCastSnapshot().bActive);
    TestTrue(TEXT("可再次发布"), CastComponent->PublishConfirmedCast(
        FGuid::NewGuid(), TEXT("ability.test.fireball@1"), TEXT("DBA.Development.Fireball"), 1, 0.7f));
    const FGuid PreviousScope = Bar->GetCastSnapshot().SourceScopeId;
    APawn* NextPawn = World->SpawnActor<APawn>();
    if (!TestNotNull(TEXT("后继角色Pawn"), NextPawn)) return false;
    auto* NextASC = NewObject<UGamePlatformAbilitySystemComponent>(NextPawn);
    auto* NextCombat = NewObject<UGamePlatformCombatComponent>(NextPawn);
    auto* NextCast = NewObject<UGamePlatformAbilityCastComponent>(NextPawn);
    NextPawn->AddInstanceComponent(NextASC); NextPawn->AddInstanceComponent(NextCombat); NextPawn->AddInstanceComponent(NextCast);
    NextASC->RegisterComponent(); NextCombat->RegisterComponent(); NextCast->RegisterComponent();
    Controller->Possess(NextPawn);
    TestFalse(TEXT("换角色先清旧动作"), Bar->GetCastSnapshot().bActive);
    TestFalse(TEXT("换角色解绑旧组件"), CastComponent->OnCastFactsChanged().IsBoundToObject(Root.Get()));
    TestTrue(TEXT("后继角色发布真实窗口"), NextCast->PublishConfirmedCast(
        FGuid::NewGuid(), TEXT("ability.test.frostbolt@1"), TEXT("DBA.Development.Frostbolt"), 1, 0.7f));
    TestTrue(TEXT("新角色获得独立UI作用域"), Bar->GetCastSnapshot().SourceScopeId != PreviousScope);
    const auto NextSnapshot = Bar->GetCastSnapshot();
    CastComponent->OnCastFactsChanged().Broadcast(CastComponent, OldFacts);
    TestEqual(TEXT("旧角色通知不能覆盖新角色"), Bar->GetCastSnapshot().CastInstanceId, NextSnapshot.CastInstanceId);
    Root->RefreshForPlayerController(nullptr);
    TestFalse(TEXT("本地来源失活清空显示"), Bar->GetCastSnapshot().bActive);
    TestFalse(TEXT("失活成对解绑施法委托"), CastComponent->OnCastFactsChanged().IsBoundToObject(Root.Get()));
    TestFalse(TEXT("失活解绑后继组件"), NextCast->OnCastFactsChanged().IsBoundToObject(Root.Get()));
    CastComponent->OnCastFactsChanged().Broadcast(CastComponent, OldFacts);
    TestEqual(TEXT("旧来源不能恢复失活条"), Bar->GetVisibility(), ESlateVisibility::Collapsed);
    return true;
}

// 捕获“只有首次快照百分比而无连续动画”以及取消后残留动画的回归。
// 只读检验事件建立的UMG原生Percent曲线；动画完成不得清除服务器bActive或更改技能资格。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCastBarNativeAnimationTest,
    "DivineBeasts.UI.Combat.CastBarNativeAnimation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCastBarNativeAnimationTest::RunTest(const FString&)
{
    UClass* BarClass = LoadClass<UGamePlatformCastProgressWidget>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Components/WBP_DBA_UI_CastBar.WBP_DBA_UI_CastBar_C"));
    if (!TestNotNull(TEXT("真实已保存施法条类"), BarClass)) return false;
    TStrongObjectPtr<UGamePlatformCastProgressWidget> Bar(NewObject<UGamePlatformCastProgressWidget>(GetTransientPackage(), BarClass));
    Bar->Initialize();
    FGamePlatformUICastProgressState State;
    State.SourceScopeId = FGuid::NewGuid(); State.CastInstanceId = TEXT("Cast.Animation.1");
    State.DisplayName = FText::FromString(TEXT("寒冰箭 · 施法动作"));
    State.TotalSeconds = 0.7; State.RemainingSeconds = 0.35; State.bActive = true; State.Revision = 1;
    TestTrue(TEXT("绑定有效作用域"), Bar->BindCastSource(State.SourceScopeId));
    TestTrue(TEXT("应用晚到一半的窗口"), Bar->ApplyCastSnapshot(State));
    UWidgetAnimation* Animation = nullptr;
    TArray<UObject*> Children; GetObjectsWithOuter(Bar.Get(), Children, EGetObjectsFlags::None);
    for (UObject* Child : Children) if (auto* Candidate = Cast<UWidgetAnimation>(Child)) Animation = Candidate;
    if (!TestNotNull(TEXT("变化事件建立瞬态引擎动画"), Animation)) return false;
    TestTrue(TEXT("动画不产生保存资源"), Animation->HasAnyFlags(RF_Transient));
    UMovieScene* Scene = Animation->GetMovieScene();
    if (!TestNotNull(TEXT("真实MovieScene"), Scene) || !TestEqual(TEXT("动画只绑定自有条"), Animation->GetBindings().Num(), 1)) return false;
    const auto* Track = Scene->FindTrack<UMovieSceneFloatTrack>(Animation->GetBindings()[0].AnimationGuid, TEXT("Percent"));
    if (!TestNotNull(TEXT("原生Percent浮点轨道"), Track) || !TestEqual(TEXT("单次有限曲线"), Track->GetAllSections().Num(), 1)) return false;
    const auto* Section = Cast<UMovieSceneFloatSection>(Track->GetAllSections()[0]);
    float Value = 0;
    TestTrue(TEXT("曲线可评估"), Section && Section->GetChannel().Evaluate(FFrameTime(0), Value));
    TestTrue(TEXT("晚到不重置为0"), FMath::IsNearlyEqual(Value, 0.5f));
    const FFrameNumber End = Scene->GetPlaybackRange().GetUpperBoundValue();
    Section->GetChannel().Evaluate(FFrameTime(End), Value);
    TestTrue(TEXT("视觉曲线到达1"), FMath::IsNearlyEqual(Value, 1.0f));
    TestTrue(TEXT("曲线结束不自行冒充服务器完成"), Bar->GetCastSnapshot().bActive);
    State.bActive = false; State.RemainingSeconds = 0; ++State.Revision;
    TestTrue(TEXT("真实终态事件被接受"), Bar->ApplyCastSnapshot(State));
    TestEqual(TEXT("终态隐藏"), Bar->GetVisibility(), ESlateVisibility::Collapsed);
    const auto Terminal = Bar->GetCastSnapshot();
    --State.Revision; State.bActive = true;
    TestFalse(TEXT("过期修订拒绝"), Bar->ApplyCastSnapshot(State));
    TestEqual(TEXT("旧快照不能复活"), Bar->GetCastSnapshot().Revision, Terminal.Revision);
    Bar->ClearCastSource();
    TestFalse(TEXT("解绑后无活跃快照"), Bar->GetCastSnapshot().bActive);
    return true;
}

// 捕获SetVisibility同步通知重入后，外层重复建动画或Bind/Clear覆盖后继文字/进度的回归。
// 直接订阅真实UUserWidget可见性事件，不伪造播放器；每个分支只重入一次，避免测试自身无限递归。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCastBarVisibilityReentryTest,
    "DivineBeasts.UI.Combat.CastBarVisibilityReentry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsCastBarVisibilityReentryTest::RunTest(const FString&)
{
    UClass* BarClass = LoadClass<UGamePlatformCastProgressWidget>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Components/WBP_DBA_UI_CastBar.WBP_DBA_UI_CastBar_C"));
    if (!TestNotNull(TEXT("真实已保存施法条类"), BarClass)) return false;
    // Apply分支：首次展开同步发布Revision=2，外层Revision=1不得再创建第二条动画。
    {
        TStrongObjectPtr<UGamePlatformCastProgressWidget> Bar(NewObject<UGamePlatformCastProgressWidget>(GetTransientPackage(), BarClass));
        Bar->Initialize();
        FGamePlatformUICastProgressState First;
        First.SourceScopeId = FGuid::NewGuid(); First.CastInstanceId = TEXT("Cast.Reentry.1");
        First.DisplayName = FText::FromString(TEXT("旧动作")); First.TotalSeconds = 0.7;
        First.RemainingSeconds = 0.7; First.bActive = true; First.Revision = 1;
        Bar->BindCastSource(First.SourceScopeId);
        auto Next = First; Next.CastInstanceId = TEXT("Cast.Reentry.2");
        Next.DisplayName = FText::FromString(TEXT("后继动作")); Next.RemainingSeconds = 0.35; Next.Revision = 2;
        bool bReentered = false;
        const FDelegateHandle Handle = Bar->OnNativeVisibilityChanged.AddLambda([&](ESlateVisibility Visibility)
        {
            if (!bReentered && Visibility == ESlateVisibility::SelfHitTestInvisible)
            { bReentered = true; TestTrue(TEXT("可见性监听者同步发布后继"), Bar->ApplyCastSnapshot(Next)); }
        });
        ON_SCOPE_EXIT { Bar->OnNativeVisibilityChanged.Remove(Handle); Bar->ClearCastSource(); };
        TestTrue(TEXT("外层首次事件已受理"), Bar->ApplyCastSnapshot(First));
        TestTrue(TEXT("真实同步可见性回调已发生"), bReentered);
        TestEqual(TEXT("保留后继修订"), Bar->GetCastSnapshot().Revision, int64(2));
        TArray<UObject*> Children; GetObjectsWithOuter(Bar.Get(), Children, EGetObjectsFlags::None);
        TArray<UWidgetAnimation*> Animations;
        for (UObject* Child : Children) if (auto* Animation = Cast<UWidgetAnimation>(Child)) Animations.Add(Animation);
        TestEqual(TEXT("外层不得为后继重复创建动画"), Animations.Num(), 1);
        Next.bActive = false; Next.RemainingSeconds = 0; ++Next.Revision;
        TestTrue(TEXT("取消后继"), Bar->ApplyCastSnapshot(Next));
        for (auto* Animation : Animations)
            TestFalse(TEXT("取消后没有失去所有权的动画继续播放"), Bar->IsAnimationPlaying(Animation));
    }
    // Bind/Clear分支：隐藏通知同步绑定新来源并发布真实快照，旧栈不得清空新来源的文字与比例。
    for (bool bClearSource : {false, true})
    {
        TStrongObjectPtr<UGamePlatformCastProgressWidget> Bar(NewObject<UGamePlatformCastProgressWidget>(GetTransientPackage(), BarClass));
        Bar->Initialize();
        FGamePlatformUICastProgressState First;
        First.SourceScopeId = FGuid::NewGuid(); First.CastInstanceId = TEXT("Cast.Cleanup.1");
        First.TotalSeconds = 0.7; First.RemainingSeconds = 0.7; First.bActive = true; First.Revision = 1;
        Bar->BindCastSource(First.SourceScopeId); Bar->ApplyCastSnapshot(First);
        auto Next = First; Next.SourceScopeId = FGuid::NewGuid(); Next.CastInstanceId = TEXT("Cast.Cleanup.2");
        Next.DisplayName = FText::FromString(TEXT("新来源动作")); Next.RemainingSeconds = 0.35;
        bool bReentered = false;
        const FDelegateHandle Handle = Bar->OnNativeVisibilityChanged.AddLambda([&](ESlateVisibility Visibility)
        {
            if (!bReentered && Visibility == ESlateVisibility::Collapsed)
            {
                bReentered = true; Bar->BindCastSource(Next.SourceScopeId);
                TestTrue(TEXT("隐藏监听者同步发布新来源"), Bar->ApplyCastSnapshot(Next));
            }
        });
        ON_SCOPE_EXIT { Bar->OnNativeVisibilityChanged.Remove(Handle); Bar->ClearCastSource(); };
        if (bClearSource) Bar->ClearCastSource(); else Bar->BindCastSource(FGuid::NewGuid());
        TestTrue(TEXT("隐藏可见性回调已发生"), bReentered);
        TestEqual(TEXT("新来源作用域保留"), Bar->GetCastSnapshot().SourceScopeId, Next.SourceScopeId);
        auto* Name = Cast<UTextBlock>(Bar->GetWidgetFromName(TEXT("CastNameText")));
        auto* Progress = Cast<UProgressBar>(Bar->GetWidgetFromName(TEXT("CastProgressBar")));
        if (!TestNotNull(TEXT("真实名称控件"), Name) || !TestNotNull(TEXT("真实进度控件"), Progress)) return false;
        TestEqual(TEXT("旧清理栈不清新名称"), Name->GetText().ToString(), FString(TEXT("新来源动作")));
        TestTrue(TEXT("旧清理栈不重置新进度"), FMath::IsNearlyEqual(Progress->GetPercent(), 0.5f));
        TestEqual(TEXT("新来源保持可见"), Bar->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
    }
    return true;
}

// 实际保存的小地图必须把已加载底图绑定给MapImage，而不仅发布快照/裁剪UV；夹具世界不连接后端。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsMinimapImageBindingTest,
    "DivineBeasts.UI.Village.MinimapImageBinding", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsMinimapImageBindingTest::RunTest(const FString&)
{
    auto* Texture = LoadObject<UTexture2D>(nullptr, TEXT("/DBAUIPack_Core/UI/Maps/T_DBA_Village_Minimap.T_DBA_Village_Minimap"));
    UClass* Class = LoadClass<UDivineBeastsVillageMinimapWidget>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_Minimap.WBP_DBA_UI_Minimap_C"));
    if (!TestNotNull(TEXT("真实地图纹理"), Texture) || !TestNotNull(TEXT("真实小地图类"), Class)) return false;
    auto* Package = CreatePackage(TEXT("/Temp/MinimapImageBinding/L_Village_Start"));
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MinimapImageBindingWorld"), Package);
    if (!TestNotNull(TEXT("独立夹具世界"), World) || !TestNotNull(TEXT("Engine"), GEngine)) return false;
    TStrongObjectPtr<UDivineBeastsVillageMinimapWidget> Widget(NewObject<UDivineBeastsVillageMinimapWidget>(GetTransientPackage(), Class));
    ON_SCOPE_EXIT { Widget->BindToPawn(nullptr); World->DestroyWorld(false); };
    auto* Controller = World->SpawnActor<APlayerController>();
    auto* Character = World->SpawnActor<ACharacter>();
    if (!TestNotNull(TEXT("夹具控制器"), Controller) || !TestNotNull(TEXT("夹具角色"), Character)) return false;
    World->AddController(Controller);
    ON_SCOPE_EXIT { World->RemoveController(Controller); };
    auto* Player = NewObject<ULocalPlayer>(GEngine);
    Controller->SetPlayer(Player);
    Controller->Possess(Character);
    Widget->SetPlayerContext(FLocalPlayerContext(Controller));
    Widget->Initialize();
    auto* Image = Cast<UImage>(Widget->GetWidgetFromName(TEXT("MapImage")));
    if (!TestNotNull(TEXT("命名地图图像"), Image)) return false;
    Image->SetBrushFromTexture(nullptr);
    const auto DrawType = Image->GetBrush().DrawAs;
    Widget->BindToPawn(Character);
    TestEqual(TEXT("快照采用真实加载底图"), Widget->GetMinimapStateView().MapTexture.Get(), Texture);
    TestEqual(TEXT("首帧实际图像绑定底图，不能显示空画刷"), Image->GetBrush().GetResourceObject(), static_cast<UObject*>(Texture));
    TestEqual(TEXT("绑定保持作者圆形裁剪样式"), Image->GetBrush().DrawAs, DrawType);
    Widget->BindToPawn(nullptr);
    TestNull(TEXT("解绑释放实际地图画刷"), Image->GetBrush().GetResourceObject());
    return true;
}

// 同一世界UV经过1/2/4倍显示后仍使用真实位置；验证边缘裁剪、圆形标记边界和拒绝非法值后的输出保持。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsMinimapViewportTest,
    "DivineBeasts.UI.Village.MinimapViewport", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsMinimapViewportTest::RunTest(const FString&)
{
    FVector2D Minimum, Maximum, Marker;
    TestTrue(TEXT("中心全图有效"), UDivineBeastsVillageMinimapWidget::ProjectMapViewport(FVector2D(0.5), 1, Minimum, Maximum, Marker));
    TestEqual(TEXT("全图最小UV"), Minimum, FVector2D::ZeroVector);
    TestEqual(TEXT("全图最大UV"), Maximum, FVector2D(1));
    TestEqual(TEXT("中心标记"), Marker, FVector2D(0.5));
    for (float Zoom : {2.f, 4.f})
    {
        UDivineBeastsVillageMinimapWidget::ProjectMapViewport(FVector2D(0.5), Zoom, Minimum, Maximum, Marker);
        TestTrue(TEXT("裁剪宽度与倍率一致"), FMath::IsNearlyEqual(Maximum.X - Minimum.X, 1.0 / Zoom));
        TestEqual(TEXT("居中玩家不漂移"), Marker, FVector2D(0.5));
        UDivineBeastsVillageMinimapWidget::ProjectMapViewport(FVector2D(0, 1), Zoom, Minimum, Maximum, Marker);
        TestTrue(TEXT("底图边缘不越界"), Minimum.X >= 0 && Minimum.Y >= 0 && Maximum.X <= 1 && Maximum.Y <= 1);
        const FVector2D Offset = Marker - FVector2D(0.5);
        TestTrue(TEXT("圆边标记完整可见"), Offset.Size() <= 0.400001);
        TestTrue(TEXT("圆边保留西南方位"), Offset.X < 0 && Offset.Y > 0 && FMath::IsNearlyEqual(-Offset.X, Offset.Y));
    }
    const FVector2D PreviousMarker = Marker;
    TestFalse(TEXT("NaN拒绝"), UDivineBeastsVillageMinimapWidget::ProjectMapViewport(FVector2D(std::numeric_limits<double>::quiet_NaN(), 0), 2, Minimum, Maximum, Marker));
    TestFalse(TEXT("零倍率拒绝"), UDivineBeastsVillageMinimapWidget::ProjectMapViewport(FVector2D(0.5), 0, Minimum, Maximum, Marker));
    TestFalse(TEXT("不支持倍率拒绝"), UDivineBeastsVillageMinimapWidget::ProjectMapViewport(FVector2D(0.5), 3, Minimum, Maximum, Marker));
    TestEqual(TEXT("失败保留结果"), Marker, PreviousMarker);
    return true;
}

// 真实资源条资产消费受控只读夹具，验证文字、隐藏和非法输入；不向GAS写值，不冒充联网角色。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsResourceValueDisplayTest,
    "DivineBeasts.UI.Village.ResourceValueDisplay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsResourceValueDisplayTest::RunTest(const FString&)
{
    for (const TCHAR* Asset : {TEXT("/DBAUIPack_Core/UI/Components/WBP_DBA_UI_HealthBar.WBP_DBA_UI_HealthBar_C"),
                              TEXT("/DBAUIPack_Core/UI/Components/WBP_DBA_UI_MomentumBar.WBP_DBA_UI_MomentumBar_C"),
                              TEXT("/DBAUIPack_Core/UI/Components/WBP_DBA_UI_PlayerFrameHealthBar.WBP_DBA_UI_PlayerFrameHealthBar_C"),
                              TEXT("/DBAUIPack_Core/UI/Components/WBP_DBA_UI_PlayerFrameMomentumBar.WBP_DBA_UI_PlayerFrameMomentumBar_C")})
    {
        UClass* Class = LoadClass<UGamePlatformResourceBarWidget>(nullptr, Asset);
        if (!TestNotNull(TEXT("真实资源条类"), Class)) return false;
        TStrongObjectPtr<UGamePlatformResourceBarWidget> Bar(NewObject<UGamePlatformResourceBarWidget>(GetTransientPackage(), Class));
        Bar->Initialize();
        UTextBlock* ValueText = Cast<UTextBlock>(Bar->GetWidgetFromName(TEXT("ResourceValueText")));
        if (!TestNotNull(TEXT("数值文本保留"), ValueText)) return false;
        const auto FontSize = ValueText->GetFont().Size;
        FGamePlatformUIResourceBarState Snapshot;
        Snapshot.ResourceId = TEXT("Resource.TestFixture");
        Snapshot.CurrentValue = 2580;
        Snapshot.MaximumValue = 2580;
        Bar->ApplyResourceState(Snapshot);
        TestEqual(TEXT("数字来自快照"), ValueText->GetText().ToString(), FString(TEXT("2580 / 2580")));
        TestEqual(TEXT("显示事件不缩放字体"), ValueText->GetFont().Size, FontSize);
        Snapshot.CurrentValue = 1.49999;
        Snapshot.MaximumValue = 100;
        Bar->ApplyResourceState(Snapshot);
        const FString PreviousNumber = ValueText->GetText().ToString();
        Snapshot.CurrentValue = 1.50001;
        Bar->ApplyResourceState(Snapshot);
        TestNotEqual(TEXT("细小变化仍更新数值文字"), ValueText->GetText().ToString(), PreviousNumber);
        Snapshot.MaximumValue = 0.00009;
        Bar->ApplyResourceState(Snapshot);
        TestTrue(TEXT("阈值下未知"), ValueText->GetText().IsEmpty());
        Snapshot.MaximumValue = 0.00011;
        Bar->ApplyResourceState(Snapshot);
        TestFalse(TEXT("跨阈值后恢复显示"), ValueText->GetText().IsEmpty());
        Snapshot.bShowValueText = false;
        Bar->ApplyResourceState(Snapshot);
        TestTrue(TEXT("隐藏立即清空"), ValueText->GetText().IsEmpty());
        Snapshot.bShowValueText = true;
        Snapshot.MaximumValue = 0;
        Bar->ApplyResourceState(Snapshot);
        TestTrue(TEXT("未知最大值不伪造0/0"), ValueText->GetText().IsEmpty());
        Snapshot.MaximumValue = 2580;
        Snapshot.CurrentValue = std::numeric_limits<double>::quiet_NaN();
        Bar->ApplyResourceState(Snapshot);
        TestTrue(TEXT("非法快照清空数字"), ValueText->GetText().IsEmpty());
        TestEqual(TEXT("非法比例保持有限0"), Snapshot.GetNormalizedValue(), 0.0);
    }
    return true;
}

// 真实已保存根布局执行与Travel相同的ClearHUD：保留/恢复作者组合HUD，临时世界HUD不能复活。
// 此用例不改资产、不连接业务后端；原实现刷新只改变Visibility，不能把已移除的组合重新挂回树。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsHUDTravelOwnershipTest,
    "DivineBeasts.UI.Village.TravelRestoresAuthoredHUD", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsHUDTravelOwnershipTest::RunTest(const FString&)
{
    UClass* Class = LoadClass<UDivineBeastsRootLayout>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Root/WBP_DBA_UI_RootLayout.WBP_DBA_UI_RootLayout_C"));
    if (!TestNotNull(TEXT("真实根布局类"), Class)) return false;
    TStrongObjectPtr<UDivineBeastsRootLayout> Root(NewObject<UDivineBeastsRootLayout>(GetTransientPackage(), Class));
    Root->Initialize();
    if (!TestNotNull(TEXT("预置组合HUD已实际绑定"), Root->CombatHUD.Get()) ||
        !TestNotNull(TEXT("预置HUD层"), Root->HUDLayer.Get())) return false;
    TestEqual(TEXT("旅行前组合属于根布局"), Root->CombatHUD->GetParent(), static_cast<UPanelWidget*>(Root->HUDLayer.Get()));
    auto* Temporary = NewObject<UImage>(Root.Get());
    TestTrue(TEXT("真实加入临时世界HUD"), Root->AddHUDWidget(Temporary));
    Root->ClearHUD();
    Root->HandlePossessedPawnChanged(nullptr, nullptr);
    TestEqual(TEXT("旅行后刷新恢复作者组合"), Root->CombatHUD->GetParent(), static_cast<UPanelWidget*>(Root->HUDLayer.Get()));
    TestNull(TEXT("临时世界HUD清理后不恢复"), Temporary->GetParent());
    Root->HandlePossessedPawnChanged(nullptr, nullptr);
    TestEqual(TEXT("重复事件不会重复添加作者HUD"), Root->HUDLayer->GetChildrenCount(), 1);
    TestEqual(TEXT("无本地角色时恢复布局仍隐藏"), Root->CombatHUD->GetVisibility(), ESlateVisibility::Collapsed);
    // 同一LocalPlayer经历控制器换代，实际根上下文和委托来源必须同步；空控制器与退出均撤销旧来源。
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("独立控制器回归世界"), World) || !TestNotNull(TEXT("Engine"), GEngine)) return false;
    UWorld* DestinationWorld = UWorld::CreateWorld(EWorldType::Game, false);
    ON_SCOPE_EXIT { Root->NativeDestruct(); if (DestinationWorld) DestinationWorld->DestroyWorld(false); World->DestroyWorld(false); };
    if (!TestNotNull(TEXT("旅行目标独立世界"), DestinationWorld)) return false;
    auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
    auto* PreviousController = World->SpawnActor<APlayerController>();
    auto* CurrentController = DestinationWorld->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("旧控制器"), PreviousController) || !TestNotNull(TEXT("目标控制器"), CurrentController)) return false;
    // 夹具不启动世界玩法；显式注册真实控制器，使LocalPlayer可按各自世界查找，退出配对移除。
    World->AddController(PreviousController);
    DestinationWorld->AddController(CurrentController);
    ON_SCOPE_EXIT { World->RemoveController(PreviousController); DestinationWorld->RemoveController(CurrentController); };
    PreviousController->SetPlayer(LocalPlayer);
    TestEqual(TEXT("夹具旧世界控制器已注册"), LocalPlayer->GetPlayerController(World), PreviousController);
    TestTrue(TEXT("夹具旧上下文有效"), FLocalPlayerContext(PreviousController).IsValid());
    Root->SetPlayerContext(FLocalPlayerContext(PreviousController));
    Root->NativeConstruct();
    auto* AbilityPanel = Cast<UUserWidget>(Root->CombatHUD->GetWidgetFromName(TEXT("AbilityBar")));
    auto* StatusPanel = Cast<UUserWidget>(Root->CombatHUD->GetWidgetFromName(TEXT("PlayerStatus")));
    if (!TestNotNull(TEXT("真实技能面板"), AbilityPanel) || !TestNotNull(TEXT("真实属性面板"), StatusPanel)) return false;
    // 先实际读取旧World固化缓存；同世界换Controller无法检测遗漏的上下文传播。
    TestEqual(TEXT("根旧世界缓存"), Root->GetWorld(), World);
    TestEqual(TEXT("组合旧世界缓存"), Root->CombatHUD->GetWorld(), World);
    TestEqual(TEXT("技能旧世界缓存"), AbilityPanel->GetWorld(), World);
    TestEqual(TEXT("状态旧世界缓存"), StatusPanel->GetWorld(), World);
    Root->ClearHUD();
    CurrentController->SetPlayer(LocalPlayer);
    TestEqual(TEXT("夹具目标世界控制器已注册"), LocalPlayer->GetPlayerController(DestinationWorld), CurrentController);
    TestTrue(TEXT("夹具目标上下文有效"), FLocalPlayerContext(CurrentController).IsValid());
    Root->RefreshForPlayerController(CurrentController);
    TestEqual(TEXT("组合继承当前控制器上下文"), Root->CombatHUD->GetOwningPlayer(), CurrentController);
    TestEqual(TEXT("根世界缓存已换代"), Root->GetWorld(), DestinationWorld);
    TestEqual(TEXT("拆下的组合世界缓存已换代"), Root->CombatHUD->GetWorld(), DestinationWorld);
    TestEqual(TEXT("技能世界缓存已换代"), AbilityPanel->GetWorld(), DestinationWorld);
    TestEqual(TEXT("属性世界缓存已换代"), StatusPanel->GetWorld(), DestinationWorld);
    TestFalse(TEXT("旧控制器委托已撤销"), PreviousController->OnPossessedPawnChanged.IsAlreadyBound(Root.Get(), &UDivineBeastsRootLayout::HandlePossessedPawnChanged));
    TestTrue(TEXT("新控制器有真实Pawn事件订阅"), CurrentController->OnPossessedPawnChanged.IsAlreadyBound(Root.Get(), &UDivineBeastsRootLayout::HandlePossessedPawnChanged));
    Root->RefreshForPlayerController(CurrentController);
    TestEqual(TEXT("换代不复制HUD实例"), Root->HUDLayer->GetChildrenCount(), 1);
    Root->RefreshForPlayerController(nullptr);
    TestFalse(TEXT("断开后解除当前Pawn订阅"), CurrentController->OnPossessedPawnChanged.IsAlreadyBound(Root.Get(), &UDivineBeastsRootLayout::HandlePossessedPawnChanged));
    TestNull(TEXT("断开清空根控制器上下文"), Root->GetOwningPlayer());
    TestNull(TEXT("断开清空组合控制器上下文"), Root->CombatHUD->GetOwningPlayer());
    TestEqual(TEXT("断开隐藏组合HUD"), Root->CombatHUD->GetVisibility(), ESlateVisibility::Collapsed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsVillageMapProjectionTest,
    "DivineBeasts.UI.Village.MinimapProjection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsVillageMapProjectionTest::RunTest(const FString&)
{
    FVector2D UV;
    TestTrue(TEXT("地图中心有效"), UDivineBeastsVillageMinimapWidget::ProjectVillagePosition(FVector::ZeroVector, UV));
    TestEqual(TEXT("地图中心位于图像中心"), UV, FVector2D(0.5, 0.5));
    TestTrue(TEXT("北部边界有效"), UDivineBeastsVillageMinimapWidget::ProjectVillagePosition(FVector(25200, 0, 0), UV));
    TestEqual(TEXT("+X北方对应图像上边"), UV, FVector2D(0.5, 0));
    UDivineBeastsVillageMinimapWidget::ProjectVillagePosition(FVector(0, 25200, 0), UV);
    TestEqual(TEXT("+Y东方对应图像右边"), UV, FVector2D(1, 0.5));
    UDivineBeastsVillageMinimapWidget::ProjectVillagePosition(FVector(-99999, -99999, 0), UV);
    TestEqual(TEXT("地图外玩家有界显示"), UV, FVector2D(0, 1));
    TestFalse(TEXT("非法坐标不能污染最后一帧"), UDivineBeastsVillageMinimapWidget::ProjectVillagePosition(
        FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0), UV));
    TestEqual(TEXT("非法输入保持调用者原结果"), UV, FVector2D(0, 1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPortraitIdentityTest,
    "DivineBeasts.UI.Village.PortraitIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPortraitIdentityTest::RunTest(const FString&)
{
    // 十二个真实英雄资产必须各自存在；不把路径拼接成功误认为头像已交付，也不以白马作全角色默认。
    for (const TCHAR* Hero : {TEXT("Rat"), TEXT("Ox"), TEXT("Tiger"), TEXT("Rabbit"), TEXT("Dragon"), TEXT("Snake"),
                             TEXT("Horse"), TEXT("Goat"), TEXT("Monkey"), TEXT("Rooster"), TEXT("Dog"), TEXT("Boar")})
    {
        const FName HeroId(*FString::Printf(TEXT("Hero.Zodiac.%s"), Hero));
        const FSoftObjectPath Path = UDivineBeastsPlayerPortraitWidget::ResolvePortraitPath(HeroId);
        TestNotNull(TEXT("每个受信英雄均有真实头像纹理"), LoadObject<UTexture2D>(nullptr, *Path.ToString()));
    }
    TestEqual(TEXT("可信Rat肖像唯一归属英雄包"),
        UDivineBeastsPlayerPortraitWidget::ResolvePortraitPath(TEXT("Hero.Zodiac.Rat")).ToString(),
        FString(TEXT("/DBAHeroPack_Rat/UI/Portraits/T_DBA_Rat_Portrait.T_DBA_Rat_Portrait")));
    TestFalse(TEXT("未知英雄不得构造任意资源引用"),
        UDivineBeastsPlayerPortraitWidget::ResolvePortraitPath(TEXT("Hero.Zodiac.Unknown")).IsValid());
    TestFalse(TEXT("未绑定英雄没有默认假肖像"),
        UDivineBeastsPlayerPortraitWidget::ResolvePortraitPath(NAME_None).IsValid());
    // 验证取消后的蓝图值读取也清空；不能仅靠隐藏界面留下可被重新渲染的旧角色。
    TStrongObjectPtr<UDivineBeastsPlayerPortraitWidget> Portrait(NewObject<UDivineBeastsPlayerPortraitWidget>());
    FGamePlatformUIPortraitState Previous;
    Previous.DisplayId = TEXT("Hero.Zodiac.Rat");
    Previous.DisplayName = FText::FromString(TEXT("上一角色"));
    Previous.PortraitTexture = TSoftObjectPtr<UTexture2D>(UDivineBeastsPlayerPortraitWidget::ResolvePortraitPath(Previous.DisplayId));
    Portrait->ApplyPortraitState(Previous);
    Portrait->BindToPawn(nullptr);
    TestTrue(TEXT("解绑撤销旧角色软纹理"), Portrait->GetPortraitState().PortraitTexture.IsNull());
    TestTrue(TEXT("解绑清空旧角色名"), Portrait->GetPortraitState().DisplayName.IsEmpty());
    // 实际保存的头像框包含两个肖像显示，解绑必须同时清理主头像与装饰图腾，并移除旧等级。
    UClass* FrameClass = LoadClass<UDivineBeastsPlayerPortraitWidget>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Components/WBP_DBA_UI_PlayerFramePortrait.WBP_DBA_UI_PlayerFramePortrait_C"));
    if (!TestNotNull(TEXT("原稿头像框真实生成类"), FrameClass)) return false;
    TStrongObjectPtr<UDivineBeastsPlayerPortraitWidget> Frame(NewObject<UDivineBeastsPlayerPortraitWidget>(GetTransientPackage(), FrameClass));
    Frame->Initialize();
    auto* MainImage = Cast<UImage>(Frame->GetWidgetFromName(TEXT("PortraitImage")));
    auto* TotemImage = Cast<UImage>(Frame->GetWidgetFromName(TEXT("HeroTotemImage")));
    auto* UnknownLevel = Cast<UTextBlock>(Frame->GetWidgetFromName(TEXT("LevelUnavailableText")));
    if (!TestNotNull(TEXT("主肖像控件"), MainImage) || !TestNotNull(TEXT("英雄图腾控件"), TotemImage) ||
        !TestNotNull(TEXT("未知等级控件"), UnknownLevel)) return false;
    UTexture2D* OldTexture = LoadObject<UTexture2D>(nullptr, *Previous.PortraitTexture.ToSoftObjectPath().ToString());
    MainImage->SetBrushFromTexture(OldTexture);
    TotemImage->SetBrushFromTexture(OldTexture);
    Frame->ApplyPortraitState(Previous);
    Frame->BindToPawn(nullptr);
    TestNull(TEXT("解绑清除原角色主肖像"), MainImage->GetBrush().GetResourceObject());
    TestNull(TEXT("解绑清除原角色图腾"), TotemImage->GetBrush().GetResourceObject());
    TestEqual(TEXT("无角色时不留等级标记"), UnknownLevel->GetVisibility(), ESlateVisibility::Collapsed);
    TStrongObjectPtr<UDivineBeastsVillageMinimapWidget> Minimap(NewObject<UDivineBeastsVillageMinimapWidget>());
    FGamePlatformUIMinimapState OldMap;
    OldMap.MapId = TEXT("Village.Start");
    OldMap.Revision = 77;
    OldMap.MapTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/DBAUIPack_Core/UI/Maps/T_DBA_Village_Minimap.T_DBA_Village_Minimap")));
    TestTrue(TEXT("预置上一世界地图快照"), Minimap->ApplyMinimapState(OldMap));
    Minimap->BindToPawn(nullptr);
    TestTrue(TEXT("取消清空底图引用"), Minimap->GetMinimapStateView().MapTexture.IsNull());
    TestTrue(TEXT("取消保持单调版本，旧回调不得覆盖"), Minimap->GetMinimapStateView().Revision > OldMap.Revision);
    return true;
}

// 头脸裁剪显示契约：项目层客户端在游戏线程加载已保存的头像框蓝图与Rat/Ox英雄纹理，
// 通过公开只读快照入口及既有RenderPortrait消费真实Widget树；不创建替身资产、不启动世界或连接后端。
// 独立期望来自已审阅512×512原图的头脸范围：归一化UV(0.25,0.08)..(0.75,0.58)。
// 遗漏/错设UV、只处理一个图像、换英雄残留旧资源、解绑遗留裁剪均应失败；不以资产/框架存在判定成功。
// 本测试只验证画刷契约，不证明RHI像素、人工视觉、异步加载或联网准入；实际RED须由主任务编译运行观察。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPortraitFaceCropTest,
    "DivineBeasts.UI.Village.PortraitFaceCrop", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPortraitFaceCropTest::RunTest(const FString&)
{
    // 路径为独立夹具字面量，不调用生产映射求期望；强引用只覆盖本次测试，退出由局部所有者释放。
    UClass* FrameClass = LoadClass<UDivineBeastsPlayerPortraitWidget>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Components/WBP_DBA_UI_PlayerFramePortrait.WBP_DBA_UI_PlayerFramePortrait_C"));
    TStrongObjectPtr<UTexture2D> RatTexture(LoadObject<UTexture2D>(nullptr,
        TEXT("/DBAHeroPack_Rat/UI/Portraits/T_DBA_Rat_Portrait.T_DBA_Rat_Portrait")));
    TStrongObjectPtr<UTexture2D> OxTexture(LoadObject<UTexture2D>(nullptr,
        TEXT("/DBAHeroPack_Ox/UI/Portraits/T_DBA_Ox_Portrait.T_DBA_Ox_Portrait")));
    if (!TestNotNull(TEXT("已保存头像框的真实生成类"), FrameClass) ||
        !TestNotNull(TEXT("已保存Rat英雄纹理"), RatTexture.Get()) ||
        !TestNotNull(TEXT("已保存Ox英雄纹理"), OxTexture.Get())) { return false; }
    if (!TestNotEqual(TEXT("两英雄拥有不同的真实纹理对象，能够检测串用"), RatTexture.Get(), OxTexture.Get())) { return false; }

    // Initialize沿用同文件真实资产模式，复制作者控件树；不手造PortraitImage或写入预期画刷。
    TStrongObjectPtr<UDivineBeastsPlayerPortraitWidget> Frame(
        NewObject<UDivineBeastsPlayerPortraitWidget>(GetTransientPackage(), FrameClass));
    if (!TestTrue(TEXT("真实头像框实例初始化成功"), Frame->Initialize())) { return false; }
    ON_SCOPE_EXIT { Frame->BindToPawn(nullptr); };
    auto* PortraitImage = Cast<UImage>(Frame->GetWidgetFromName(TEXT("PortraitImage")));
    auto* TotemImage = Cast<UImage>(Frame->GetWidgetFromName(TEXT("HeroTotemImage")));
    if (!TestNotNull(TEXT("真实主头像图像"), PortraitImage) ||
        !TestNotNull(TEXT("真实英雄图腾图像"), TotemImage)) { return false; }

    // 检查消费者可见画刷：纹理指针须保持本英雄原资源，UV须有效且等于独立字面量。
    // 无效UV先报契约失败，不读取其未定义边界；聚合断言结果，同时继续检查另一控件及后续生命周期。
    const auto CheckFaceBrush = [this](UImage* Image, UTexture2D* ExpectedTexture, const TCHAR* Phase)
    {
        bool bPassed = TestEqual(FString::Printf(TEXT("%s：画刷保留本英雄原纹理对象"), Phase),
            Image->GetBrush().GetResourceObject(), static_cast<UObject*>(ExpectedTexture));
        const FBox2f UV = Image->GetBrush().GetUVRegion();
        bPassed &= TestTrue(FString::Printf(TEXT("%s：画刷启用有效头脸裁剪"), Phase), UV.bIsValid != 0);
        if (UV.bIsValid)
        {
            bPassed &= TestEqual(FString::Printf(TEXT("%s：头脸裁剪最小UV"), Phase), UV.Min, FVector2f(0.25f, 0.08f));
            bPassed &= TestEqual(FString::Printf(TEXT("%s：头脸裁剪最大UV"), Phase), UV.Max, FVector2f(0.75f, 0.58f));
        }
        return bPassed;
    };

    FGamePlatformUIPortraitState RatState;
    RatState.DisplayId = TEXT("Hero.Zodiac.Rat");
    RatState.PortraitTexture = TSoftObjectPtr<UTexture2D>(RatTexture.Get());
    Frame->ApplyPortraitState(RatState);
    Frame->RenderPortrait();
    bool bPassed = CheckFaceBrush(PortraitImage, RatTexture.Get(), TEXT("Rat主头像"));
    bPassed &= CheckFaceBrush(TotemImage, RatTexture.Get(), TEXT("Rat英雄图腾"));

    // 同一实例切到第二英雄，覆盖已渲染的第一英雄；不能靠新建空控件规避旧画刷残留。
    FGamePlatformUIPortraitState OxState;
    OxState.DisplayId = TEXT("Hero.Zodiac.Ox");
    OxState.PortraitTexture = TSoftObjectPtr<UTexture2D>(OxTexture.Get());
    Frame->ApplyPortraitState(OxState);
    Frame->RenderPortrait();
    bPassed &= CheckFaceBrush(PortraitImage, OxTexture.Get(), TEXT("切换Ox主头像"));
    bPassed &= CheckFaceBrush(TotemImage, OxTexture.Get(), TEXT("切换Ox英雄图腾"));

    // 使用真实解绑入口，让生产清理发布空快照并渲染；同时撤销资源和UV，隐藏控件本身不足以通过。
    Frame->BindToPawn(nullptr);
    bPassed &= TestNull(TEXT("解绑清空主头像真实画刷资源"), PortraitImage->GetBrush().GetResourceObject());
    bPassed &= TestNull(TEXT("解绑清空图腾真实画刷资源"), TotemImage->GetBrush().GetResourceObject());
    const FBox2f ClearedPortraitUV = PortraitImage->GetBrush().GetUVRegion();
    const FBox2f ClearedTotemUV = TotemImage->GetBrush().GetUVRegion();
    bPassed &= TestFalse(TEXT("解绑撤销主头像旧UV区域"), ClearedPortraitUV.bIsValid != 0);
    bPassed &= TestFalse(TEXT("解绑撤销图腾旧UV区域"), ClearedTotemUV.bIsValid != 0);
    return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsVillageHUDGarbageCollectionTest,
    "DivineBeasts.UI.Village.GarbageCollectionCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsVillageHUDGarbageCollectionTest::RunTest(const FString&)
{
    // 复现蓝图重设父类时的真实GC入口：对象已不可达，BeginDestroy不能经Apply*触发任何ProcessEvent。
    // 非空快照保证旧实现必定发布变化；保留画刷/可见性验证销毁路径没有操作Widget树。
    auto* Portrait = NewObject<UDivineBeastsPlayerPortraitWidget>();
    Portrait->WidgetTree = NewObject<UWidgetTree>(Portrait);
    auto* PortraitImage = Portrait->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("PortraitImage"));
    Portrait->WidgetTree->RootWidget = PortraitImage;
    auto* PortraitTexture = NewObject<UTexture2D>();
    FGamePlatformUIPortraitState PreviousPortrait;
    PreviousPortrait.DisplayId = TEXT("Hero.Zodiac.Rat");
    PreviousPortrait.DisplayName = FText::FromString(TEXT("销毁前快照"));
    Portrait->ApplyPortraitState(PreviousPortrait);
    PortraitImage->SetBrushFromTexture(PortraitTexture);
    Portrait->SetVisibility(ESlateVisibility::Visible);
    Portrait->MarkAsGarbage();
    Portrait->SetInternalFlags(EInternalObjectFlags::Unreachable);
    TestTrue(TEXT("肖像实际进入BeginDestroy"), Portrait->ConditionalBeginDestroy());
    TestEqual(TEXT("GC不得经ApplyPortraitState发布空快照/蓝图事件"), Portrait->GetPortraitState().DisplayId, PreviousPortrait.DisplayId);
    TestEqual(TEXT("GC不触碰肖像画刷"), PortraitImage->GetBrush().GetResourceObject(), static_cast<UObject*>(PortraitTexture));
    TestEqual(TEXT("GC不修改肖像可见性"), Portrait->GetVisibility(), ESlateVisibility::Visible);
    TestFalse(TEXT("重复销毁幂等"), Portrait->ConditionalBeginDestroy());
    // 恢复临时不可达标志，让后续正常GC自行完成回收；不复活已标记Garbage的对象。
    Portrait->ClearInternalFlags(EInternalObjectFlags::Unreachable);

    auto* Minimap = NewObject<UDivineBeastsVillageMinimapWidget>();
    Minimap->WidgetTree = NewObject<UWidgetTree>(Minimap);
    auto* MapImage = Minimap->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MapImage"));
    Minimap->WidgetTree->RootWidget = MapImage;
    auto* MapTexture = NewObject<UTexture2D>();
    FGamePlatformUIMinimapState PreviousMap;
    PreviousMap.MapId = TEXT("Village.Start");
    PreviousMap.Revision = 99;
    TestTrue(TEXT("销毁前有非空地图快照"), Minimap->ApplyMinimapState(PreviousMap));
    MapImage->SetBrushFromTexture(MapTexture);
    Minimap->SetVisibility(ESlateVisibility::Visible);
    Minimap->MarkAsGarbage();
    Minimap->SetInternalFlags(EInternalObjectFlags::Unreachable);
    TestTrue(TEXT("小地图实际进入BeginDestroy"), Minimap->ConditionalBeginDestroy());
    TestEqual(TEXT("GC不得经ApplyMinimapState发布新版本/蓝图事件"), Minimap->GetMinimapStateView().Revision, PreviousMap.Revision);
    TestEqual(TEXT("GC不触碰地图画刷"), MapImage->GetBrush().GetResourceObject(), static_cast<UObject*>(MapTexture));
    TestEqual(TEXT("GC不修改小地图可见性"), Minimap->GetVisibility(), ESlateVisibility::Visible);
    TestFalse(TEXT("地图重复销毁幂等"), Minimap->ConditionalBeginDestroy());
    Minimap->ClearInternalFlags(EInternalObjectFlags::Unreachable);
    return true;
}
#endif
