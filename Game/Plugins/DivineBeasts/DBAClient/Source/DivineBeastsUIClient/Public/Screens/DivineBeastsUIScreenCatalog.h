#pragma once

#include "CoreMinimal.h"
#include "GamePlatformUITypes.h"
#include "DivineBeastsUIScreenCatalog.generated.h"

/** EDivineBeastsUISurfaceKind（项目UI表面类型）。 */
UENUM()
enum class EDivineBeastsUISurfaceKind : uint8
{
    Screen,
    HUD,
    Notification
};

/** FDivineBeastsUISurfaceDescriptor（项目页面/表面清单项）。 */
struct DIVINEBEASTSUICLIENT_API FDivineBeastsUISurfaceDescriptor
{
    FName SurfaceId = NAME_None;
    EDivineBeastsUISurfaceKind Kind = EDivineBeastsUISurfaceKind::Screen;
    EGamePlatformUILayer Layer = EGamePlatformUILayer::Screen;
    EGamePlatformUIInputMode InputMode = EGamePlatformUIInputMode::GameAndUI;
    EGamePlatformUIPausePolicy PausePolicy = EGamePlatformUIPausePolicy::Never;
    EGamePlatformUITransition Transition = EGamePlatformUITransition::Default;
    FName DefaultFocusWidgetName = NAME_None;
    FString DefinitionAssetPath;
    FString WidgetClassPath;
    /** Android/iOS共享的移动端Widget变体软路径；业务状态和ViewModel保持同一套。 */
    FString MobileWidgetClassPath;
    bool bSurvivesTravel = false;
};

/**
 * FDivineBeastsUIScreenCatalog（神兽联盟公共非竞技UI表面目录）。
 *
 * 仅登记DBAClient拥有的公共页面/HUD/通知；Arena专属表面由DBAArena独立目录拥有。
 */
class DIVINEBEASTSUICLIENT_API FDivineBeastsUIScreenCatalog
{
public:
    static const TArray<FDivineBeastsUISurfaceDescriptor>& GetSurfaces();
    static const FDivineBeastsUISurfaceDescriptor* Find(FName SurfaceId);
    static TArray<FName> GetScreenIds();
    static TArray<FName> GetHUDIds();
    static TArray<FName> GetNotificationIds();
};
