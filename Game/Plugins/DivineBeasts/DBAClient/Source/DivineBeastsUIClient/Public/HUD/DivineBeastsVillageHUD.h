#pragma once

#include "HUD/DivineBeastsHUDWidget.h"
#include "DivineBeastsVillageHUD.generated.h"

/**
 * UDivineBeastsVillageHUD（神兽联盟新手村 HUD C++ 基类）。
 *
 * 面向 Village.Main（主新手村）体验，强调新手任务、交互和基础操作提示。
 * 不把 Tutorial / Training 当作新的服务器角色。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsVillageHUD
    : public UDivineBeastsHUDWidget
{
    GENERATED_BODY()

public:
    UDivineBeastsVillageHUD()
    {
        HUDKind = EDivineBeastsHUDKind::Village;
        HUDSurfaceId = TEXT("UI.HUD.VillageMain");
    }
};
