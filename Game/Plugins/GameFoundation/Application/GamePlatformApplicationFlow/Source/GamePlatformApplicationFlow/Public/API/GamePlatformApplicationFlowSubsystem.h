#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/GamePlatformFlowTypes.h"
#include "Types/GamePlatformDataLease.h"
#include "GamePlatformApplicationFlowSubsystem.generated.h"

namespace GamePlatform::ApplicationFlow { class FApplicationFlowExecutor; }
class FGamePlatformFlowNodeAdapter;

/**
 * 每个 GameInstance 唯一主流程服务；客户端／服务器可用，不包含具体业务节点。
 * C++ 组合根通过 GameInstance->GetSubsystem 获取，因此只公开生命周期入口及稳定契约。
 * 跨地图持续存在；节点必须自行重新取得 World。编辑器预览／Commandlet 不创建运行服务。
 */
UCLASS(Transient)
class GAMEPLATFORMAPPLICATIONFLOW_API UGamePlatformApplicationFlowSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UGamePlatformApplicationFlowSubsystem();
    /** 在私有实现中定义，避免 UHT 生成构造函数对不完整执行器类型实例化删除器。 */
    UGamePlatformApplicationFlowSubsystem(FVTableHelper& Helper);
    virtual ~UGamePlatformApplicationFlowSubsystem() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 原子安装，失败保留旧配置。节点必须属于本 GameInstance；错误由 OutError 返回。 */
    bool Configure(const FGamePlatformFlowDefinition& Definition, FString& OutError);
    /** 启动一次完整流程；Payload 可为空，非空时须属于本 GameInstance，直到终态保活。 */
    FGamePlatformFlowHandle Start(UObject* Payload, FString& OutError);
    /** 只有本作用域正在执行的句柄有效。重复取消、旧代次、跨实例句柄均返回 false。 */
    bool Cancel(const FGamePlatformFlowHandle& Handle);
    /** 显式注册本实例工厂；重复ExecutorId、空函数、活动运行或重入均拒绝，失败返回无效句柄。 */
    FGamePlatformFlowFactoryHandle RegisterNodeFactory(FName ExecutorId, FGamePlatformFlowNodeFactory Factory,
        FGamePlatformResult& OutResult);
    /** 仅空闲时撤销本作用域的匹配注册；旧句柄或重复撤销失败，不影响其他注册。 */
    bool UnregisterNodeFactory(const FGamePlatformFlowFactoryHandle& Handle, FGamePlatformResult& OutResult);
    /**
     * 消费本实例Data服务已就绪的UGamePlatformFlowDefinition租约；游戏线程调用，Payload约束同Start。
     * 所有节点/路由/执行器预检后创建本run独立节点；成功接纳后转移租约并清空InOutReadyLease。
     * 同步失败不转移租约、不启动节点；终态或关闭先Finish节点再释放持有租约。不得继续使用租约副本。
     */
    FGamePlatformFlowHandle StartFlow(FGamePlatformDataLease& InOutReadyLease, UObject* Payload,
        FGamePlatformResult& OutResult);
    /** 精确取消已开始节点；令牌过期或代次不符失败。首节点开始前仍可用旧Cancel(运行句柄)。 */
    bool CancelFlow(const FGamePlatformFlowNodeToken& Token, FGamePlatformResult& OutResult);
    /** 游戏线程把外部完成事件投递到原执行邮箱；与Complete竞争，首次有效，拒绝旧节点/重复/跨实例。 */
    bool SubmitEvent(const FGamePlatformFlowNodeToken& Token, FGamePlatformFlowNodeResult Event,
        FGamePlatformResult& OutResult);
    /** 游戏线程读取快照。初始化前和关闭后的状态也有明确表达。 */
    FGamePlatformFlowSnapshot GetSnapshot() const;
    /** 每次运行仅广播一个终态；成功、失败、取消与 Deinitialize 中断都通知。 */
    FGamePlatformFlowFinished& OnFinished() { return FinishedEvent; }

private:
    friend class FGamePlatformFlowNodeAdapter;
    bool TickFlow(float DeltaSeconds);
    void PublishTerminal();
    bool CanControl(FString& OutError) const;
    void RemoveTicker();
    /** 等待节点／广播栈展开后完成幂等关闭，绝不在核心调用栈内销毁执行器。 */
    void CompleteDeinitialize();

    TUniquePtr<GamePlatform::ApplicationFlow::FApplicationFlowExecutor> Executor;
    FTSTicker::FDelegateHandle TickerHandle;
    FGuid ScopeId;
    uint64 LastPublishedRunId = 0;
    bool bClosing = false;
    bool bDeinitialized = false;
    bool bPublishing = false; // 广播内允许查询，变更须推迟到下一游戏线程任务。
    bool bDispatching = false; // Execute/Finish 重入控制在适配层同样拒绝。

    UPROPERTY(Transient)
    TObjectPtr<UObject> ActivePayload;

    // GC 可追踪强引用，避免 shared_ptr 持有隐式 UObject 根造成 GameInstance 引用环。
    UPROPERTY(Transient)
    TArray<TObjectPtr<UGamePlatformFlowNode>> OwnedNodes;

    FGamePlatformFlowFinished FinishedEvent;
};
