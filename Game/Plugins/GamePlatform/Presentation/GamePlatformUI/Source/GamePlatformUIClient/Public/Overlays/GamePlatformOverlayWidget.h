#pragma once

#include "Core/GamePlatformWidgetBase.h"
#include "GamePlatformOverlayWidget.generated.h"

/**
 * UGamePlatformOverlayWidget（游戏平台覆盖界面基类）。
 *
 * 用于技能瞄准、交互提示、Tooltip（悬浮提示）、比较信息等临时覆盖内容。
 * Overlay 默认不拥有 Gameplay 权威状态，只消费 ViewModel 或事件投影。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformOverlayWidget
    : public UGamePlatformWidgetBase
{
    GENERATED_BODY()
};
