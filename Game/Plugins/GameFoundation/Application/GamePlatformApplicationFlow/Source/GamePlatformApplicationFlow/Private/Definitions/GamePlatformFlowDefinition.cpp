#include "Definitions/GamePlatformFlowDefinition.h"
#include "Definitions/GamePlatformFlowDefinitionConversion.h"

namespace GamePlatform::ApplicationFlow
{
FDefinition ConvertAssetGraph(const UGamePlatformFlowDefinition& Definition)
{
    const auto Name = [](FName Value) { return Value.IsNone() ? std::string{} : std::string(TCHAR_TO_UTF8(*Value.ToString().ToLower())); };
    FDefinition Result;
    Result.Entry = Name(Definition.EntryNodeId);
    Result.bAllowCycles = Definition.bAllowCycles;
    Result.MaxImmediateCycleTransitions = static_cast<std::uint32_t>(Definition.MaxImmediateCycleTransitions);
    for (const auto& Node : Definition.Nodes)
    {
        FStep Step;
        Step.Id = Name(Node.NodeId);
        Step.Next = Name(Node.NextNodeId);
        Step.TimeoutSeconds = Node.TimeoutSeconds;
        for (const auto& Route : Node.Routes) Step.Routes.emplace(Name(Route.Key), Name(Route.Value));
        Result.Steps.push_back(std::move(Step));
    }
    return Result;
}
}

FGamePlatformResult UGamePlatformFlowDefinition::ValidateDefinition() const
{
    const FGamePlatformResult BaseResult = Super::ValidateDefinition();
    if (!BaseResult.IsSuccess()) return BaseResult;
    if (MaxImmediateCycleTransitions < 1)
        return FGamePlatformResult::Failure(TEXT("InvalidCycleBudget"), TEXT("即时循环预算必须为正数。"));
    for (const auto& Node : Nodes)
    {
        if (Node.ExecutorId.IsNone())
            return FGamePlatformResult::Failure(TEXT("MissingExecutorId"), TEXT("流程节点必须声明执行器键。"));
        if (Node.InputDefinitionId != FPrimaryAssetId())
        {
            FGamePlatformId Parsed;
            if (Node.InputDefinitionId.PrimaryAssetType != DefinitionAssetType() ||
                !FGamePlatformId::TryParse(Node.InputDefinitionId.PrimaryAssetName.ToString(), Parsed))
                return FGamePlatformResult::Failure(TEXT("InvalidInputDefinitionId"), TEXT("输入定义必须为空或具有平台定义类型与合法逻辑身份。"));
        }
    }
    std::string Error;
    if (!GamePlatform::ApplicationFlow::FApplicationFlowExecutor::ValidateGraph(
        GamePlatform::ApplicationFlow::ConvertAssetGraph(*this), Error))
        return FGamePlatformResult::Failure(TEXT("InvalidFlowGraph"), UTF8_TO_TCHAR(Error.c_str()));
    return FGamePlatformResult::Success();
}
