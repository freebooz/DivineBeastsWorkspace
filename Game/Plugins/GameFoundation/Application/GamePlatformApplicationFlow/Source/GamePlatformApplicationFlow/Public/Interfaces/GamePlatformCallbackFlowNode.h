#pragma once

#include "Interfaces/GamePlatformFlowNode.h"
#include "GamePlatformCallbackFlowNode.generated.h"

/**
 * 组合根的函数式节点适配器，可直接注入真实服务操作，无须为每个步骤另建反射类型。
 * 执行与清理函数必须成对绑定且只能绑定一次；函数闭包不得强持有 GameInstance 形成引用环。
 * 跨 UObject 引用请捕获 TWeakObjectPtr，并在游戏线程检查有效性。
 */
UCLASS(Transient)
class GAMEPLATFORMAPPLICATIONFLOW_API UGamePlatformCallbackFlowNode final : public UGamePlatformFlowNode
{
    GENERATED_BODY()

public:
    using FExecuteAction = TFunction<void(const FGamePlatformFlowContext&, FGamePlatformFlowCompletion)>;
    using FFinishAction = TFunction<void(EGamePlatformFlowFinishReason)>;

    /** 游戏线程绑定一次。任一函数为空或已绑定时返回 false，保留旧绑定。 */
    bool Bind(FExecuteAction InExecute, FFinishAction InFinish);
    virtual void Execute(const FGamePlatformFlowContext& Context, FGamePlatformFlowCompletion Complete) override;
    virtual void Finish(EGamePlatformFlowFinishReason Reason) override;

private:
    FExecuteAction ExecuteAction;
    FFinishAction FinishAction;
    bool bExecuting = false;
};
