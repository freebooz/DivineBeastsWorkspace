#pragma once
#include "Types/GamePlatformExperienceState.h"
#include "Types/GamePlatformResult.h"
class UWorld;

/**
 * 一次体验运行独占的可撤销装配项。全部方法游戏线程；不在这里执行支付、分配等不可内存回滚操作。
 * BeginPrepare启动后由PollPreparation返回NotExecuted等待或终态；取消必须使自己的迟到回调无效。
 */
class GAMEPLATFORMGAMEPLAY_API IGamePlatformExperienceAssembly
{
public:
    virtual ~IGamePlatformExperienceAssembly() = default;
    /** 启动本次准备；Context为值，World只在本次运行有效，禁止跨世界保存裸指针。 */
    virtual FGamePlatformResult BeginPrepare(UWorld& World, const FGamePlatformExperienceSnapshot& Context) = 0;
    /** NotExecuted表示仍在准备；组件统一负责单调截止，非成功终态按必需性回滚。 */
    virtual FGamePlatformResult PollPreparation() = 0;
    /** 准备成功后一次激活；失败也必须能由Release撤销所有已产生的本地资源。 */
    virtual FGamePlatformResult Activate() = 0;
    /** 首先停止接收新操作，允许重复调用；不得释放仍被玩家使用的类型资源。 */
    virtual void StopAccepting() = 0;
    /** 逆序释放本项注册、监听和异步请求；幂等、同步完成自身逻辑撤销。 */
    virtual void Release() = 0;
};
/** 每次启动创建新对象；返回空使本项失败。不得返回共享可变执行器。 */
using FGamePlatformAssemblyFactory = TFunction<TUniquePtr<IGamePlatformExperienceAssembly>()>;
