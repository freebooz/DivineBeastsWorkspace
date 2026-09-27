#pragma once

#include "Core/GamePlatformWidgetBase.h"
#include "GamePlatformHUDWidget.generated.h"

/**
 * UGamePlatformHUDWidget（游戏平台 HUD 基类）。
 *
 * HUD 在世界与战斗期间长期存在，不参与 CommonUI 页面栈导航。
 * 本类继承普通 UI 基类，因此自动获得 ViewModel 事件驱动刷新和 PC / 移动端自适应能力。
 *
 * 性能约束：
 * - 默认不 Tick。
 * - 生命、护盾、技能、任务等业务变化必须由事件驱动。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformHUDWidget
    : public UGamePlatformWidgetBase
{
    GENERATED_BODY()
};
