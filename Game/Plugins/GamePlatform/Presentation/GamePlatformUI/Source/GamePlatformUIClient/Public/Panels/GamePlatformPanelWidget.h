#pragma once

#include "Core/GamePlatformWidgetBase.h"
#include "GamePlatformPanelWidget.generated.h"

/**
 * UGamePlatformPanelWidget（游戏平台功能面板基类）。
 *
 * Panel 用于 Screen / HUD 内部的较大功能区域，例如属性、背包、任务或技能面板。
 * Panel 不拥有页面导航栈，不直接 AddToViewport，并继承事件驱动 ViewModel 与自适应能力。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformPanelWidget
    : public UGamePlatformWidgetBase
{
    GENERATED_BODY()
};
