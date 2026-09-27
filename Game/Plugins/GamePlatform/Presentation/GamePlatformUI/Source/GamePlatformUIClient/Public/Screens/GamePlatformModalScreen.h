#pragma once

#include "Screens/GamePlatformUIScreen.h"
#include "GamePlatformModalScreen.generated.h"

/**
 * UGamePlatformModalScreen（游戏平台模态页面基类）。
 *
 * 用途：
 * - 承载需要占据 Modal Layer（模态层）并阻断下层 UI 交互的页面。
 * - 例如确认、网络错误、匹配准备和强制提示。
 *
 * 业务项目应继续派生项目层 Modal 基类，不应直接从 UCommonActivatableWidget 派生。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformModalScreen
    : public UGamePlatformUIScreen
{
    GENERATED_BODY()
};
