#pragma once
#include "Interfaces/GamePlatformFlowNode.h"
#include "Bootstrap/Online/DBAFoundationOnlineContext.h"
#include "DBAFoundationOnlineNode.generated.h"

class UDBAFoundationCoordinator;

/** 项目在线节点由组合根每运行创建；只组合公开Flow/Online接口，不把项目业务放入平台。 */
UCLASS(Transient)
class UDBAFoundationOnlineNode final : public UGamePlatformFlowNode
{
    GENERATED_BODY()
public:
    /** 游戏线程工厂配置；Root、节点与服务必须属于同一实例。 */
    void Configure(EDBAOnlineOperation InOperation, UDBAFoundationCoordinator& Root);
    /** 游戏线程异步进入，真实响应成功才通知流程；节点完成委托不被当作业务凭据。 */
    virtual void Execute(const FGamePlatformFlowContext& Context, FGamePlatformFlowCompletion Complete) override;
    /** 先失效再取消本次请求；迟到网络回调不得推进新节点或新运行。 */
    virtual void Finish(EGamePlatformFlowFinishReason Reason) override;
private:
    void CompleteOnce(FGamePlatformResult Result);
    bool TickReady(float DeltaSeconds);
    TWeakObjectPtr<UDBAFoundationCoordinator> Coordinator;
    TWeakObjectPtr<UGameInstance> Instance;
    EDBAOnlineOperation Operation = EDBAOnlineOperation::ValidateConfiguration;
    FGamePlatformOnlineRequestHandle Request;
    FGamePlatformFlowCompletion Completion;
    FTSTicker::FDelegateHandle Ticker;
    uint64 Generation = 0;
    bool bActive = false;
};
