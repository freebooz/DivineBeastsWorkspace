#include "Composite/GamePlatformVFXCompositeRunner.h"
#include "Engine/World.h"

void FGamePlatformVFXCompositeRunner::Run(
    UWorld& World,
    const UGamePlatformVFXCompositeDefinition& Definition,
    const FGamePlatformVFXRequest& Request,
    const FGamePlatformVFXHandle& ParentHandle,
    FPlayChild PlayChild,
    FRegisterTimer RegisterTimer)
{
    if (!PlayChild)
    {
        return;
    }

    if (Request.CompositeDepth >= Definition.MaxDepth ||
        Definition.Steps.Num() > Definition.MaxChildren)
    {
        return;
    }

    for (const FGamePlatformVFXCompositeStep& Step : Definition.Steps)
    {
        if (Step.DefinitionId.IsNone())
        {
            continue;
        }

        FGamePlatformVFXRequest ChildRequest = Request;
        ChildRequest.Parameters.Append(Step.ParameterOverrides);
        ChildRequest.CompositeDepth = Request.CompositeDepth + 1;

        if (Step.DelaySeconds > Definition.MaxStepDelaySeconds ||
            Step.DelaySeconds > Definition.MaxTotalLifetimeSeconds)
        {
            continue;
        }

        if (Step.DelaySeconds <= KINDA_SMALL_NUMBER)
        {
            PlayChild(Step.DefinitionId, ChildRequest, ParentHandle);
            continue;
        }

        FTimerHandle TimerHandle;
        World.GetTimerManager().SetTimer(
            TimerHandle,
            FTimerDelegate::CreateLambda(
                [DefinitionId = Step.DefinitionId, ChildRequest, ParentHandle, PlayChild]() mutable
                {
                    PlayChild(DefinitionId, ChildRequest, ParentHandle);
                }),
            Step.DelaySeconds,
            false);

        if (RegisterTimer)
        {
            RegisterTimer(ParentHandle, TimerHandle);
        }
    }
}
