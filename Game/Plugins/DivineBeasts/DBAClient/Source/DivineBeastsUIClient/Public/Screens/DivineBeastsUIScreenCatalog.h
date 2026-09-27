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
    FString AndroidWidgetClassPath;
    bool bSurvivesTravel = false;
};

/** FDivineBeastsUIScreenCatalog（一期项目UI页面清单）。 */
class DIVINEBEASTSUICLIENT_API FDivineBeastsUIScreenCatalog
{
public:
    static const TArray<FDivineBeastsUISurfaceDescriptor>& GetSurfaces();
    static const FDivineBeastsUISurfaceDescriptor* Find(FName SurfaceId);
    static TArray<FName> GetScreenIds();
    static TArray<FName> GetHUDIds();
    static TArray<FName> GetNotificationIds();
};
