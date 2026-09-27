#pragma once

#include "Screens/GamePlatformUIScreen.h"
#include "GamePlatformMenuScreen.generated.h"

/**
 * UGamePlatformMenuScreen（游戏平台菜单页面基类）。
 *
 * 用于主菜单、系统菜单、暂停菜单和设置类页面。
 * 菜单继承 Screen 的事件驱动、焦点、Back 和输入模式能力，不额外创建第二套导航系统。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformMenuScreen : public UGamePlatformUIScreen
{
    GENERATED_BODY()
};
