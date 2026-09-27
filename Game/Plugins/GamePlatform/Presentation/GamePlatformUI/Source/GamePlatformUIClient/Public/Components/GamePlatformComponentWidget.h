#pragma once

#include "Core/GamePlatformWidgetBase.h"
#include "GamePlatformComponentWidget.generated.h"

/**
 * UGamePlatformComponentWidget（游戏平台可复用界面组件基类）。
 *
 * 用于按钮组合、卡片、列表项、技能槽、物品槽、输入提示和进度组件等可复用部件。
 * 组件可以绑定轻量 ViewModel，但不得承担页面导航或全局 UI 服务职责。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformComponentWidget
    : public UGamePlatformWidgetBase
{
    GENERATED_BODY()
};
