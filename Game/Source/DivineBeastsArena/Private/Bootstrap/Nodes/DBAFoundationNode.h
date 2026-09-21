#pragma once

#include "Containers/Ticker.h"
#include "Interfaces/GamePlatformFlowNode.h"
#include "Types/GamePlatformDataLease.h"
#include "DBAFoundationNode.generated.h"

class UDBAFoundationCoordinator;
class UWorld;

/** 项目开发操作，不代表正式业务；流程资产只引用注册键，不引用此类型。 */
enum class EDBAFoundationOperation : uint8 { Boot, ValidateConfiguration, LoadProbeDefinition, EnterSandbox, Ready };

/** 每次运行由本实例组合根创建；监听、异步请求及切图操作均属于单次激活。 */
UCLASS(Transient)
class UDBAFoundationNode final : public UGamePlatformFlowNode
{
    GENERATED_BODY()
public:
    /** 游戏线程工厂配置，Owner须属于本节点同一实例；只在首次Execute之前调用。 */
    void Configure(EDBAFoundationOperation InOperation, UDBAFoundationCoordinator& Owner);
    /** 游戏线程进入；Complete最多调用一次，数据及地图结果来自实际服务与世界。 */
    virtual void Execute(const FGamePlatformFlowContext& Context, FGamePlatformFlowCompletion Complete) override;
    /** 先使节点操作失效，再解绑/释放；同步或迟到回调不可再次完成。 */
    virtual void Finish(EGamePlatformFlowFinishReason Reason) override;
private:
    void CompleteOnce(FGamePlatformFlowNodeResult Result);
    void OnMapLoaded(UWorld* World);
    bool PollWorld(float DeltaSeconds);
    EDBAFoundationOperation Operation = EDBAFoundationOperation::Boot;
    TWeakObjectPtr<UDBAFoundationCoordinator> Coordinator;
    TWeakObjectPtr<UGameInstance> Instance;
    TWeakObjectPtr<UWorld> LoadedWorld;
    FGamePlatformFlowCompletion Completion;
    FGamePlatformDataLease PendingLease;
    FDelegateHandle MapLoadedHandle;
    FTSTicker::FDelegateHandle PollHandle;
    FString TravelOperation;
    FGamePlatformFlowHandle RunningFlow;
    uint64 Activation = 0;
    bool bActive = false;
};
