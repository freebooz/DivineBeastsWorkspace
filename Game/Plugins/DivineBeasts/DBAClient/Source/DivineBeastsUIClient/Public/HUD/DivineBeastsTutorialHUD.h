#pragma once

#include "HUD/World/DivineBeastsWorldHUDBase.h"
#include "DivineBeastsTutorialHUD.generated.h"

/**
 * UDivineBeastsTutorialHUD（神兽联盟教学引导 HUD C++ 基类）。
 *
 * 用于 Village.Tutorial（教学体验）的步骤目标和输入提示。
 * 输入图标必须响应 CommonInput 设备切换事件，不通过 Tick 检测键鼠/手柄/触摸。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsTutorialHUD
    : public UDivineBeastsWorldHUDBase
{
    GENERATED_BODY()

public:
    UDivineBeastsTutorialHUD()
    {
        HUDKind = EDivineBeastsHUDKind::Tutorial;
        HUDSurfaceId = TEXT("UI.HUD.TutorialGuidance");
    }
};
