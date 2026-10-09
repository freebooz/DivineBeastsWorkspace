#pragma once

// 平台层VFX客户端私有适配器：由世界表现桥接调用，弱持有当前World，不拥有玩法事实。

#include "GamePlatformPresentationTypes.h"

class UWorld;

/** 中立GamePlatformPresentation请求到VFX Service的正式适配器。 */
class FGamePlatformVFXPresentationProvider
{
public:
    /** 游戏线程绑定所属世界；空或失效世界在Handle时拒绝，不保活世界，也不跨World复用。 */
    explicit FGamePlatformVFXPresentationProvider(UWorld* InWorld);

    /** 游戏线程转交中立请求；受理或完成取消返回true，语义/世界/服务无效返回false，不代表已渲染。 */
    bool Handle(const FGamePlatformPresentationRequest& Request) const;

private:
    /** 当前桥接所属世界的非拥有引用；World退出后不能访问新世界服务。 */
    TWeakObjectPtr<UWorld> World;
};
