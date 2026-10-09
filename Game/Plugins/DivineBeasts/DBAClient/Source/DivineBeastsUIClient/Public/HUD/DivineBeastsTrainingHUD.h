#pragma once

#include "HUD/World/DivineBeastsWorldHUDBase.h"
#include "DivineBeastsTrainingHUD.generated.h"

/**
 * UDivineBeastsTrainingHUD（神兽联盟训练控制 HUD C++ 基类）。
 *
 * 用于 Village.Training（训练体验）的重置、技能测试和训练说明。
 * PC 可显示快捷键提示，移动端可显示触控控制，但两端共享同一状态与命令。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsTrainingHUD
    : public UDivineBeastsWorldHUDBase
{
    GENERATED_BODY()

public:
    UDivineBeastsTrainingHUD()
    {
        HUDKind = EDivineBeastsHUDKind::Training;
        HUDSurfaceId = TEXT("UI.HUD.TrainingControls");
    }
};
