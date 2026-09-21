#pragma once
#include "Interfaces/GamePlatformFlowNode.h"
#include "Types/GamePlatformLoadingTypes.h"
#include "Containers/Ticker.h"
#include "DBALoadingFlowNode.generated.h"
class UDBAFoundationCoordinator;

/** 开发组合节点：Flow决定进入阶段，Loading负责资源与世界屏障；不包含会话替身。 */
UCLASS(Transient)
class UDBALoadingFlowNode final : public UGamePlatformFlowNode
{
    GENERATED_BODY()
public:
    /** 仅游戏线程，在Execute前注入同一实例的bootstrap。 */
    void Configure(UDBAFoundationCoordinator& Owner);
    /** 提交公开Flow事件，保留RunId/NodeGeneration和Loading完整身份，不同步重入推进。 */
    virtual void Execute(const FGamePlatformFlowContext& Context,FGamePlatformFlowCompletion Complete) override;
    /** 所有退出路径先撤订阅再释放本操作；成功后组合根原有租约继续负责开发场景资源。 */
    virtual void Finish(EGamePlatformFlowFinishReason Reason) override;
private:
    bool CheckWorld(float DeltaSeconds);
    TWeakObjectPtr<UDBAFoundationCoordinator> Coordinator;
    TWeakObjectPtr<UGameInstance> Instance;
    FGamePlatformLoadingHandle Operation;
    FGamePlatformLoadingRegistration Subscription;
    FGamePlatformFlowNodeToken FlowToken;
    FTSTicker::FDelegateHandle WorldCheck;
    bool bIsActive = false;
};
