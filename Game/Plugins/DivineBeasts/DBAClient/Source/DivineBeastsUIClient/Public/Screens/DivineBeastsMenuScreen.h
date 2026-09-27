#pragma once

#include "Screens/GamePlatformMenuScreen.h"
#include "DivineBeastsMenuScreen.generated.h"

/**
 * UDivineBeastsMenuScreen（神兽联盟菜单页面基类）。
 *
 * 供系统菜单、设置菜单等项目页面继承。
 * 菜单状态通过 ViewModel 和事件更新，不在页面中直接读取业务服务。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsMenuScreen
    : public UGamePlatformMenuScreen
{
    GENERATED_BODY()
};
