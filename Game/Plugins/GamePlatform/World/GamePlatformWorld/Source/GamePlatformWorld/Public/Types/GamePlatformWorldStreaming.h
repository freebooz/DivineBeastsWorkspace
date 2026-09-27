#pragma once

#include "CoreMinimal.h"

/** 最低需求；Activated包含Loaded，不会为了Loaded隐藏已经可见的内容。 */
enum class EGamePlatformWorldStreamingRequestedState : uint8
{
    Loaded, // 资源驻留；不保证Actor已加入运行世界。
    Activated // WP激活或传统关卡已加入并可见；不代表导航/网络/业务就绪。
};

/** 游戏线程提交的世界作用域需求，不创建地图实例，不改变Data Layer状态。 */
struct FGamePlatformWorldStreamingRequest
{
    /** 必须匹配协调器的当前世界代次，不接受默认空GUID。 */
    FGuid ContextGeneration;
    /** 弱拥有者必须属于当前World；不会因请求阻止Owner被回收。 */
    TWeakObjectPtr<UObject> Owner;
    /** WP源位置，厘米；使用各网格原生加载半径。传统关卡忽略位置但仍校验有限值。 */
    FVector TargetLocation = FVector::ZeroVector;
    /** 传统地图须为已登记关卡的完整包名（支持移除PIE前缀）；WP必须为空。 */
    FName LevelPackage;
    /** 默认请求激活；只是提交需求，不是提交时就已完成。 */
    EGamePlatformWorldStreamingRequestedState RequestedState = EGamePlatformWorldStreamingRequestedState::Activated;
    /** 与WP原生一致：0最高，255最低；传统流送转换为255-Priority且只提高现有优先级。 */
    int32 Priority = 128;
    /** 每段连续Pending的真实时间预算，秒且必须有限、正数；Ready退化时重新计时。 */
    double TimeoutSeconds = 30.0;
    /** 纳入流送就绪屏障；自动失败会保留屏障，显式Cancel才撤销本项义务。 */
    bool bRequiredForReadiness = false;
};

/** 不拥有资源的值句柄；两个GUID均有效只表示结构有效，仍须由原协调器核验。 */
struct FGamePlatformWorldStreamingHandle
{
    FGuid ContextGeneration; // 世界代次，阻止旧世界访问新世界。
    FGuid RequestId; // 每次接受请求新建GUID，不复用槽位。
    bool IsValid() const { return ContextGeneration.IsValid() && RequestId.IsValid(); }
};

/** Ready是可退化的驻留快照，不是永久终态；Failed/Cancelled不会被迟到加载改写。 */
enum class EGamePlatformWorldStreamingState : uint8 { Pending, Ready, Failed, Cancelled };

/** 游戏线程值快照；Pending/Ready无错误，失败/取消的Error用于程序诊断。 */
struct FGamePlatformWorldStreamingResult
{
    EGamePlatformWorldStreamingState State = EGamePlatformWorldStreamingState::Pending;
    FName Error;
};
