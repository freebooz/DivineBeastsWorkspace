#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "Types/GamePlatformId.h"

UGamePlatformVFXCompositeDefinition::UGamePlatformVFXCompositeDefinition()
{
    Behavior = EGamePlatformVFXBehavior::Composite;
    bAllowPooling = false;
}

FGamePlatformResult UGamePlatformVFXCompositeDefinition::ValidateDefinition() const
{
    const FGamePlatformResult BaseResult = Super::ValidateDefinition();
    if (!BaseResult.IsSuccess())
    {
        return BaseResult;
    }

    if (Steps.IsEmpty() ||
        Steps.Num() > MaxChildren ||
        MaxDepth < 1 || MaxDepth > 8 ||
        !FMath::IsFinite(MaxStepDelaySeconds) || MaxStepDelaySeconds < 0.0f ||
        !FMath::IsFinite(MaxTotalLifetimeSeconds) || MaxTotalLifetimeSeconds <= 0.0f)
    {
        return FGamePlatformResult::Failure(
            TEXT("VFX.CompositeLimitsInvalid"),
            TEXT("Composite的子节点、深度、步骤延迟或总生命周期配置非法。"));
    }

    const FName SelfId = GetDefinitionId();
    for (const FGamePlatformVFXCompositeStep& Step : Steps)
    {
        FGamePlatformId Parsed;
        if (Step.DefinitionId.IsNone() ||
            !FGamePlatformId::TryParse(Step.DefinitionId.ToString(), Parsed) ||
            !FMath::IsFinite(Step.DelaySeconds) ||
            Step.DelaySeconds < 0.0f ||
            Step.DelaySeconds > MaxStepDelaySeconds ||
            Step.DelaySeconds > MaxTotalLifetimeSeconds)
        {
            return FGamePlatformResult::Failure(
                TEXT("VFX.CompositeStepInvalid"),
                TEXT("Composite步骤的DefinitionId或DelaySeconds非法。"));
        }

        if (!SelfId.IsNone() && FName(*Parsed.ToString()) == SelfId)
        {
            return FGamePlatformResult::Failure(
                TEXT("VFX.CompositeSelfCycle"),
                TEXT("Composite不能直接引用自身DefinitionId。"));
        }
    }

    return FGamePlatformResult::Success();
}
