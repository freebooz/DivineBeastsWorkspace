// 新手村HUD显示回归；只验证客户端坐标、真实英雄目录和生命周期隔离，不模拟后端准入或伪造等级。
#include "Components/DivineBeastsVillageMinimapWidget.h"
#include "Components/DivineBeastsPlayerPortraitWidget.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
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
