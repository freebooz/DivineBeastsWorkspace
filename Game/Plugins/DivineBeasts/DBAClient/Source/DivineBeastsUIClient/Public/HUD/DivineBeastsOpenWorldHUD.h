#pragma once

#include "HUD/DivineBeastsHUDWidget.h"
#include "DivineBeastsOpenWorldHUD.generated.h"

/**
 * UDivineBeastsOpenWorldHUD（神兽联盟开放世界 HUD C++ 基类）。
 *
 * 面向登录大厅、主城和开放世界区域的常驻 HUD。
 * 具体视觉由 Blueprint 组合 PlayerStatus、AbilityBar、QuestTracker、Minimap 等 Panel。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsOpenWorldHUD
    : public UDivineBeastsHUDWidget
{
    GENERATED_BODY()

public:
    UDivineBeastsOpenWorldHUD()
    {
        HUDKind = EDivineBeastsHUDKind::OpenWorld;
        HUDSurfaceId = TEXT("UI.HUD.OpenWorld");
    }
};
