#pragma once

#include "Screens/GamePlatformUIScreen.h"
#include "GamePlatformLoadingScreen.generated.h"

/**
 * UGamePlatformLoadingScreen（游戏平台加载页面基类）。
 *
 * 职责：
 * - 为启动、地图切换、会话准入等加载页面提供统一页面类型。
 * - 实际进度必须来自 Loading Service（加载服务）事件，不允许使用假计时器伪造进度。
 * - 默认禁止用户通过 Back（返回）动作关闭加载页面。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformLoadingScreen
    : public UGamePlatformUIScreen
{
    GENERATED_BODY()

public:
    /**
     * 使用 UE 标准 ObjectInitializer 构造路径，并默认关闭 Back 返回。
     * 避免加载事务未结束时用户从 CommonUI 栈直接退出加载页面。
     */
    explicit UGamePlatformLoadingScreen(
        const FObjectInitializer& ObjectInitializer);
};
