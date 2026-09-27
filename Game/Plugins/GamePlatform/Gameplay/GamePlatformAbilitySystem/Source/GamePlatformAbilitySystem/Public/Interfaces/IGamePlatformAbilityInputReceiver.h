#pragma once
#include "Types/GamePlatformAbilityInput.h"
#include "Types/GamePlatformResult.h"
/** 游戏线程当前拥有者输入端口；键盘键、InputClient类型和认证信息都不进入共享模块。 */
class GAMEPLATFORMABILITYSYSTEM_API IGamePlatformAbilityInputReceiver
{
public:
    virtual ~IGamePlatformAbilityInputReceiver() = default;
    /** 当前端令牌，每次ActorInfo失效后变化；调用者应在绑定当前接收器时重新获取。 */
    virtual FGamePlatformAbilityInputToken GetInputToken() const = 0;
    /** 只缓存精确匹配已复制Spec的批准输入标签；不会直接施加效果或授予技能。 */
    virtual FGamePlatformResult AbilityInputPressed(FGameplayTag Tag, const FGamePlatformAbilityInputToken& Token) = 0;
    /** 释放当前已按住标签；等待释放任务由原生GAS事件链处理。 */
    virtual FGamePlatformResult AbilityInputReleased(FGameplayTag Tag, const FGamePlatformAbilityInputToken& Token) = 0;
    /** 每帧至多由一个本地适配调用；请求成功不代表服务器最终激活确认。 */
    virtual void ProcessAbilityInput() = 0;
    /** 失焦、菜单或换接收器时清除自己缓存，并通知运行技能释放；不操作其他输入插件抑制令牌。 */
    virtual void ClearAbilityInput() = 0;
};
