#pragma once

#include "Screens/GamePlatformModalScreen.h"
#include "DivineBeastsModalScreen.generated.h"

/**
 * UDivineBeastsModalScreen（神兽联盟模态页面基类）。
 *
 * 用于网络错误、确认、准备等必须阻断下层交互的项目界面。
 * 模态页面仍由平台 CommonUI Modal Layer（模态层）统一管理。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsModalScreen
    : public UGamePlatformModalScreen
{
    GENERATED_BODY()
};
