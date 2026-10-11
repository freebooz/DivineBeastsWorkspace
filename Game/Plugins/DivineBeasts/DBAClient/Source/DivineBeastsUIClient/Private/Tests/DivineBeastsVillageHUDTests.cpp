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
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
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
