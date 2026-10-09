#include "UI/DivineBeastsArenaUIScreenCatalog.h"

namespace
{
FDivineBeastsArenaUISurfaceDescriptor MakeScreen(
    const TCHAR* SurfaceId,
    const TCHAR* AssetName,
    EGamePlatformUILayer Layer,
    EGamePlatformUIInputMode InputMode,
    const TCHAR* FocusWidgetName)
{
    FDivineBeastsArenaUISurfaceDescriptor Descriptor;
    Descriptor.SurfaceId = FName(SurfaceId);
    Descriptor.Kind = EDivineBeastsArenaUISurfaceKind::Screen;
    Descriptor.Layer = Layer;
    Descriptor.InputMode = InputMode;
    Descriptor.DefaultFocusWidgetName = FName(FocusWidgetName);

    // 使用 DBAArena 插件真实 Mount Point（挂载点）规划软路径。
    // 资产尚未创建时路径仍只作为安全软引用，不伪造二进制资源。
    Descriptor.WidgetClassPath = FString::Printf(
        TEXT("/DBAArena/UI/Screens/WBP_DBA_UI_Arena_%s.WBP_DBA_UI_Arena_%s_C"),
        AssetName,
        AssetName);
    // 竞技移动端控件尚未通过Monolith编译/保存/Cook，禁止登记不存在的软类路径。
    // 空路径意味着平台安全回退共享布局；真实移动变体交付并验收后再逐项登记。
    Descriptor.MobileWidgetClassPath.Reset();
    return Descriptor;
}

FDivineBeastsArenaUISurfaceDescriptor MakeHUD()
{
    FDivineBeastsArenaUISurfaceDescriptor Descriptor;
    Descriptor.SurfaceId = TEXT("UI.HUD.Arena");
    Descriptor.Kind = EDivineBeastsArenaUISurfaceKind::HUD;
    Descriptor.Layer = EGamePlatformUILayer::HUD;
    Descriptor.InputMode = EGamePlatformUIInputMode::GameOnly;
    Descriptor.WidgetClassPath =
        TEXT("/DBAArena/UI/HUD/WBP_DBA_UI_ArenaHUD.WBP_DBA_UI_ArenaHUD_C");
    // HUD移动专用资源尚未交付，先使用公共布局而非制造必然失败的软引用。
    Descriptor.MobileWidgetClassPath.Reset();
    return Descriptor;
}

const TArray<FDivineBeastsArenaUISurfaceDescriptor>& BuildCatalog()
{
    static const TArray<FDivineBeastsArenaUISurfaceDescriptor> Catalog =
    {
        MakeScreen(
            TEXT("UI.Screen.Matchmaking"),
            TEXT("Matchmaking"),
            EGamePlatformUILayer::Screen,
            EGamePlatformUIInputMode::UIOnly,
            TEXT("ModeList")),
        MakeScreen(
            TEXT("UI.Screen.MatchFoundReady"),
            TEXT("MatchFoundReady"),
            EGamePlatformUILayer::Modal,
            EGamePlatformUIInputMode::UIOnly,
            TEXT("ReadyButton")),
        MakeScreen(
            TEXT("UI.Screen.ArenaHeroSelection"),
            TEXT("HeroSelection"),
            EGamePlatformUILayer::Screen,
            EGamePlatformUIInputMode::UIOnly,
            TEXT("HeroList")),
        MakeScreen(
            TEXT("UI.Screen.Scoreboard"),
            TEXT("Scoreboard"),
            EGamePlatformUILayer::System,
            EGamePlatformUIInputMode::GameAndUI,
            TEXT("ScoreboardList")),
        MakeScreen(
            TEXT("UI.Screen.PostMatchResult"),
            TEXT("PostMatch"),
            EGamePlatformUILayer::Screen,
            EGamePlatformUIInputMode::UIOnly,
            TEXT("ReturnWorldButton")),
        MakeHUD()
    };
    return Catalog;
}
}

const TArray<FDivineBeastsArenaUISurfaceDescriptor>&
FDivineBeastsArenaUIScreenCatalog::GetSurfaces()
{
    return BuildCatalog();
}

const FDivineBeastsArenaUISurfaceDescriptor*
FDivineBeastsArenaUIScreenCatalog::Find(FName SurfaceId)
{
    return BuildCatalog().FindByPredicate(
        [SurfaceId](const FDivineBeastsArenaUISurfaceDescriptor& Descriptor)
        {
            return Descriptor.SurfaceId == SurfaceId;
        });
}

TArray<FName> FDivineBeastsArenaUIScreenCatalog::GetScreenIds()
{
    TArray<FName> Result;
    Result.Reserve(5);
    for (const FDivineBeastsArenaUISurfaceDescriptor& Descriptor :
         BuildCatalog())
    {
        if (Descriptor.Kind == EDivineBeastsArenaUISurfaceKind::Screen)
        {
            Result.Add(Descriptor.SurfaceId);
        }
    }
    return Result;
}
