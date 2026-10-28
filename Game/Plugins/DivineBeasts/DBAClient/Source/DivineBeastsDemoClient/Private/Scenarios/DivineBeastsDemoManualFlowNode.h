// 本地人工等待节点：执行/结束完全由唯一平台Flow调度，不自行推进流程。
#pragma once
#include "Interfaces/GamePlatformFlowNode.h"
#include "DivineBeastsDemoManualFlowNode.generated.h"
UCLASS(Transient)
class UDivineBeastsDemoManualFlowNode final : public UGamePlatformFlowNode
{
    GENERATED_BODY()
public:
    /** 只保留本次完成端；用户分支通过平台SubmitEvent，取消由平台Finish。 */
    void Execute(const FGamePlatformFlowContext&,FGamePlatformFlowCompletion Complete) override {Pending=MoveTemp(Complete);}
    void Finish(EGamePlatformFlowFinishReason) override {Pending={};++FinishCount;}
    int32 GetFinishCount() const {return FinishCount;}
private:
    FGamePlatformFlowCompletion Pending;
    int32 FinishCount=0;
};

