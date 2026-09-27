#pragma once

#include "Screens/GamePlatformUIScreen.h"
#include "GamePlatformSystemScreen.generated.h"

/**
 * UGamePlatformSystemScreen（游戏平台系统页面基类）。
 *
 * 用于设置、帮助、记分板等System Layer（系统层）页面。
 * 与普通Screen共享生命周期，但通过类型表达“高于普通业务页面、低于Loading/Debug”的系统语义。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformSystemScreen
    : public UGamePlatformUIScreen
{
    GENERATED_BODY()
};
