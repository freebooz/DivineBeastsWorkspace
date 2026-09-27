#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "DivineBeastsComponentWidget.generated.h"

/**
 * UDivineBeastsComponentWidget（神兽联盟复用界面组件基类）。
 *
 * 用于神兽联盟按钮、卡片、技能槽、物品槽、头像、资源条和输入提示等可复用控件。
 * 组件保持轻量，不承担页面导航、网络请求路由或全局状态所有权。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsComponentWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()
};
