#pragma once
#include "Types/GamePlatformExperienceState.h"
#include "Types/GamePlatformPlayerLifecycle.h"
#include "Types/GamePlatformResult.h"

class APlayerState;
class UWorld;

/** 世界内撤销句柄；不复用槽位。跨服务或旧世界句柄不能撤销当前注册。 */
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformGameplayRegistration
{
    /** 服务实例身份。 */
    FGuid ScopeId;
    /** 本次注册身份。 */
    FGuid RegistrationId;
    bool IsValid() const { return ScopeId.IsValid() && RegistrationId.IsValid(); }
};

/** 脱敏的当前服务诊断；只包含数量和错误码，无认证材料。 */
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformGameplayDiagnostics
{
    /** 当前体验公开快照。 */
    FGamePlatformExperienceSnapshot Experience;
    /** 当前端体验资源准备快照。 */
    FGamePlatformClientExperienceSnapshot LocalResources;
    /** 本组件仍拥有的有效Data租约数，不代表物理显存或内存。 */
    int32 HeldLeases = 0;
    /** 存活的装配对象数。 */
    int32 AssemblyCount = 0;
    /** 存活的弱观察者数。 */
    int32 ObserverCount = 0;
};

/** 世界作用域只读门面；全部接口只在游戏线程调用，无全局当前世界查找。 */
class GAMEPLATFORMGAMEPLAY_API IGamePlatformGameplayService
{
public:
    virtual ~IGamePlatformGameplayService() = default;
    /** 值快照；晚订阅者可直接取得当前事实。 */
    virtual FGamePlatformExperienceSnapshot GetExperienceSnapshot() const = 0;
    /** 读取当前进程独立准备结果；服务器Active不隐式修改客户端准备。 */
    virtual FGamePlatformClientExperienceSnapshot GetClientExperienceSnapshot() const = 0;
    /** 只读取同世界PlayerState公开快照；错世界或不兼容类型返回默认未登记值。 */
    virtual FGamePlatformPlayerLifecycleSnapshot GetPlayerLifecycleSnapshot(const APlayerState& Player) const = 0;
    /** 弱Owner必须属于本世界；最多128个。下一采样轮交付当前快照及后续变化，回调内禁止重入写操作。 */
    virtual FGamePlatformGameplayRegistration SubscribeExperienceChanged(TWeakObjectPtr<UObject> Owner,
        TFunction<void(const FGamePlatformExperienceSnapshot&)> Callback, FGamePlatformResult& OutResult) = 0;
    /** 精确撤销；无效或已经撤销返回false，不能删除别人登记。 */
    virtual bool Unsubscribe(const FGamePlatformGameplayRegistration& Registration) = 0;
    /** 同一采样点诊断副本，不借用定义或容器。 */
    virtual FGamePlatformGameplayDiagnostics GetDiagnostics() const = 0;
};
