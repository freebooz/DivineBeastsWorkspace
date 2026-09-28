#include "Screens/DivineBeastsUIScreenCatalog.h"

namespace
{
    // DBAUIPack_Core（神兽联盟核心UI内容包）是项目公共前台美术的规划唯一所有者。
    // 当前仓库尚未生成该内容插件及二进制 .uasset；这里仅声明未来稳定挂载点，
    // 不把不存在的旧 /DivineBeastsUI 路径继续当成已交付资产。
    constexpr const TCHAR* ProjectUIContentRoot = TEXT("/DBAUIPack_Core/UI");

    FDivineBeastsUISurfaceDescriptor MakeScreen(
        const TCHAR* Id,
        const TCHAR* AssetName,
        EGamePlatformUILayer Layer,
        EGamePlatformUIInputMode InputMode,
        FName Focus = NAME_None,
        bool bSurvivesTravel = false)
    {
        FDivineBeastsUISurfaceDescriptor D;
        D.SurfaceId = FName(Id);
        D.Kind = EDivineBeastsUISurfaceKind::Screen;
        D.Layer = Layer;
        D.InputMode = InputMode;
        D.PausePolicy = EGamePlatformUIPausePolicy::Never;
        D.Transition = EGamePlatformUITransition::Default;
        D.DefaultFocusWidgetName = Focus;
        D.DefinitionAssetPath = FString::Printf(
            TEXT("%s/Screens/DA_DBA_UI_%s.DA_DBA_UI_%s"),
            ProjectUIContentRoot,
            AssetName,
            AssetName);
        D.WidgetClassPath = FString::Printf(
            TEXT("%s/Screens/WBP_DBA_UI_%s.WBP_DBA_UI_%s_C"),
            ProjectUIContentRoot,
            AssetName,
            AssetName);
        D.AndroidWidgetClassPath = FString::Printf(
            TEXT("%s/Screens/WBP_DBA_UI_%s_Android.WBP_DBA_UI_%s_Android_C"),
            ProjectUIContentRoot,
            AssetName,
            AssetName);
        D.bSurvivesTravel = bSurvivesTravel;
        return D;
    }

    FDivineBeastsUISurfaceDescriptor MakeHUD(
        const TCHAR* Id,
        const TCHAR* AssetName)
    {
        FDivineBeastsUISurfaceDescriptor D;
        D.SurfaceId = FName(Id);
        D.Kind = EDivineBeastsUISurfaceKind::HUD;
        D.Layer = EGamePlatformUILayer::HUD;
        D.InputMode = EGamePlatformUIInputMode::GameOnly;
        D.WidgetClassPath = FString::Printf(
            TEXT("%s/HUD/WBP_DBA_UI_%s.WBP_DBA_UI_%s_C"),
            ProjectUIContentRoot,
            AssetName,
            AssetName);
        D.AndroidWidgetClassPath = FString::Printf(
            TEXT("%s/HUD/WBP_DBA_UI_%s_Android.WBP_DBA_UI_%s_Android_C"),
            ProjectUIContentRoot,
            AssetName,
            AssetName);
        return D;
    }

    FDivineBeastsUISurfaceDescriptor MakeNotification(
        const TCHAR* Id,
        const TCHAR* AssetName)
    {
        FDivineBeastsUISurfaceDescriptor D;
        D.SurfaceId = FName(Id);
        D.Kind = EDivineBeastsUISurfaceKind::Notification;
        D.Layer = EGamePlatformUILayer::Notification;
        D.InputMode = EGamePlatformUIInputMode::GameOnly;
        D.WidgetClassPath = FString::Printf(
            TEXT("%s/Dialogs/WBP_DBA_UI_%s.WBP_DBA_UI_%s_C"),
            ProjectUIContentRoot,
            AssetName,
            AssetName);
        return D;
    }

    const TArray<FDivineBeastsUISurfaceDescriptor>& Build()
    {
        static const TArray<FDivineBeastsUISurfaceDescriptor> Value =
        {
            MakeScreen(TEXT("UI.Screen.Boot"), TEXT("Boot"), EGamePlatformUILayer::Loading, EGamePlatformUIInputMode::UIOnly),
            MakeScreen(TEXT("UI.Screen.Login"), TEXT("Login"), EGamePlatformUILayer::Screen, EGamePlatformUIInputMode::UIOnly, TEXT("AccountInput")),
            MakeScreen(TEXT("UI.Screen.CharacterRoster"), TEXT("CharacterRoster"), EGamePlatformUILayer::Screen, EGamePlatformUIInputMode::UIOnly, TEXT("CharacterList")),
            MakeScreen(TEXT("UI.Screen.CharacterCreate"), TEXT("CharacterCreate"), EGamePlatformUILayer::Screen, EGamePlatformUIInputMode::UIOnly, TEXT("CharacterNameInput")),
            MakeScreen(TEXT("UI.Screen.CharacterSelect"), TEXT("CharacterSelect"), EGamePlatformUILayer::Screen, EGamePlatformUIInputMode::UIOnly, TEXT("CharacterList")),
            MakeScreen(TEXT("UI.Screen.LoadingTravel"), TEXT("LoadingTravel"), EGamePlatformUILayer::Loading, EGamePlatformUIInputMode::UIOnly),
            MakeScreen(TEXT("UI.Screen.ErrorReconnect"), TEXT("ErrorReconnect"), EGamePlatformUILayer::Modal, EGamePlatformUIInputMode::UIOnly, TEXT("RetryButton")),
            MakeScreen(TEXT("UI.Screen.SystemMenu"), TEXT("SystemMenu"), EGamePlatformUILayer::System, EGamePlatformUIInputMode::GameAndUI, TEXT("ResumeButton")),
            MakeHUD(TEXT("UI.HUD.OpenWorld"), TEXT("OpenWorldHUD")),
            MakeHUD(TEXT("UI.HUD.VillageMain"), TEXT("VillageMainHUD")),
            MakeHUD(TEXT("UI.HUD.TutorialGuidance"), TEXT("TutorialGuidance")),
            MakeHUD(TEXT("UI.HUD.TrainingControls"), TEXT("TrainingControls")),
            MakeNotification(TEXT("UI.Notification.Toast"), TEXT("Toast"))
        };
        return Value;
    }
}

const TArray<FDivineBeastsUISurfaceDescriptor>&
FDivineBeastsUIScreenCatalog::GetSurfaces()
{
    return Build();
}

const FDivineBeastsUISurfaceDescriptor*
FDivineBeastsUIScreenCatalog::Find(FName SurfaceId)
{
    return Build().FindByPredicate(
        [SurfaceId](const FDivineBeastsUISurfaceDescriptor& D)
        {
            return D.SurfaceId == SurfaceId;
        });
}

TArray<FName> FDivineBeastsUIScreenCatalog::GetScreenIds()
{
    TArray<FName> Result;
    for (const FDivineBeastsUISurfaceDescriptor& D : Build())
    {
        if (D.Kind == EDivineBeastsUISurfaceKind::Screen)
        {
            Result.Add(D.SurfaceId);
        }
    }
    return Result;
}

TArray<FName> FDivineBeastsUIScreenCatalog::GetHUDIds()
{
    TArray<FName> Result;
    for (const FDivineBeastsUISurfaceDescriptor& D : Build())
    {
        if (D.Kind == EDivineBeastsUISurfaceKind::HUD)
        {
            Result.Add(D.SurfaceId);
        }
    }
    return Result;
}

TArray<FName> FDivineBeastsUIScreenCatalog::GetNotificationIds()
{
    TArray<FName> Result;
    for (const FDivineBeastsUISurfaceDescriptor& D : Build())
    {
        if (D.Kind == EDivineBeastsUISurfaceKind::Notification)
        {
            Result.Add(D.SurfaceId);
        }
    }
    return Result;
}
