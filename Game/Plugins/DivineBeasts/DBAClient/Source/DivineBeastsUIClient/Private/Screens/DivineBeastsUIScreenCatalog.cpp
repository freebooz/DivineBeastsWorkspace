#include "Screens/DivineBeastsUIScreenCatalog.h"

namespace
{
    // DBAUIPack_Core（神兽联盟核心UI内容包）是项目公共前台美术的唯一所有者。
    // 根布局、登录、角色创建和选择页已交付真实资产；其余项仍是分期稳定软路径，
    // 调用方必须保留软加载失败路径，不能把规划项误认为均已交付。
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
        // UI机制源码由DBAClient承载，真实UMG资源由独立DBAUIPack_Core内容包拥有；
        // 目录同时包含已交付与分期规划路径，软加载失败必须保留可见错误或降级结果。
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
        // 移动端专用资产尚未由Monolith交付时保持空路径，让平台解析器回退公共Widget。
        // 只有真实变体完成编译、保存与Cook验证后，才允许在具体目录项显式登记。
        D.MobileWidgetClassPath.Reset();
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
        // HUD同样不能为尚不存在的移动端资产制造非空软路径。
        D.MobileWidgetClassPath.Reset();
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
            TEXT("%s/Notifications/WBP_DBA_UI_%s.WBP_DBA_UI_%s_C"),
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
            MakeScreen(TEXT("UI.Screen.CharacterCreate"), TEXT("CharacterCreate"), EGamePlatformUILayer::Screen, EGamePlatformUIInputMode::UIOnly, TEXT("CharacterNameInput")),
            MakeScreen(TEXT("UI.Screen.CharacterSelect"), TEXT("CharacterSelect"), EGamePlatformUILayer::Screen, EGamePlatformUIInputMode::UIOnly, TEXT("CharacterList")),
            MakeScreen(TEXT("UI.Screen.LoadingTravel"), TEXT("LoadingTravel"), EGamePlatformUILayer::Loading, EGamePlatformUIInputMode::UIOnly),
            MakeScreen(TEXT("UI.Screen.ErrorReconnect"), TEXT("ErrorReconnect"), EGamePlatformUILayer::Modal, EGamePlatformUIInputMode::UIOnly, TEXT("RetryButton")),
            MakeScreen(TEXT("UI.Screen.SystemMenu"), TEXT("SystemMenu"), EGamePlatformUILayer::System, EGamePlatformUIInputMode::GameAndUI, TEXT("ResumeButton")),
            MakeScreen(TEXT("UI.Screen.Inventory"), TEXT("Inventory"), EGamePlatformUILayer::Screen, EGamePlatformUIInputMode::GameAndUI, TEXT("InventoryGrid")),
            MakeScreen(TEXT("UI.Screen.Quest"), TEXT("Quest"), EGamePlatformUILayer::Screen, EGamePlatformUIInputMode::GameAndUI, TEXT("QuestList")),
            // 社交与运营是第三层公共项目页面，不属于MOBA竞技；软加载仍需提供可见失败结果。
            MakeScreen(TEXT("UI.Screen.Social"), TEXT("Social"), EGamePlatformUILayer::Screen, EGamePlatformUIInputMode::GameAndUI, TEXT("FriendsButton")),
            MakeScreen(TEXT("UI.Screen.LiveOps"), TEXT("LiveOps"), EGamePlatformUILayer::Screen, EGamePlatformUIInputMode::GameAndUI, TEXT("NoticesButton")),
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
