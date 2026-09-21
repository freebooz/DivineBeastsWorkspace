#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/GamePlatformFlowTypes.h"
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

    TUniquePtr<GamePlatform::ApplicationFlow::FApplicationFlowExecutor> Executor;
    FTSTicker::FDelegateHandle TickerHandle;
    FGuid ScopeId;
    uint64 LastPublishedRunId = 0;
    bool bClosing = false;
    bool bPublishing = false; // 广播内允许查询，变更须推迟到下一游戏线程任务。
    bool bDispatching = false; // Execute/Finish 重入控制在适配层同样拒绝。

    UPROPERTY(Transient)
    TObjectPtr<UObject> ActivePayload;

    // GC 可追踪强引用，避免 shared_ptr 持有隐式 UObject 根造成 GameInstance 引用环。
    UPROPERTY(Transient)
    TArray<TObjectPtr<UGamePlatformFlowNode>> OwnedNodes;

    FGamePlatformFlowFinished FinishedEvent;
};
