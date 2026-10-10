// 本文件属于平台客户端VFX复合排程；必需步骤拒绝即报告失败，不持有资源或权威事实。
// 中文参数、必需子拒绝/取消与资源生命周期见本插件Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#include "Composite/GamePlatformVFXCompositeRunner.h"
#include "Engine/World.h"

bool FGamePlatformVFXCompositeRunner::Run(
    UWorld& World,
    const UGamePlatformVFXCompositeDefinition& Definition,
    const FGamePlatformVFXRequest& Request,
    const FGamePlatformVFXHandle& ParentHandle,
    FPlayChild PlayChild,
    FRegisterTimer RegisterTimer)
{
    if (!PlayChild || !RegisterTimer || !ParentHandle.BelongsToWorld(&World) || World.bIsTearingDown || Definition.Steps.IsEmpty())
    {
        return false;
    }

    if (Request.CompositeDepth >= Definition.MaxDepth ||
        Definition.Steps.Num() > Definition.MaxChildren)
    {
        return false;
    }

    // 启动任何子实例前先检查全部必需步骤，避免部分排程后跳过非法项仍宣称Playing。
    for (const FGamePlatformVFXCompositeStep& Step : Definition.Steps)
    {
        if (Step.DefinitionId.IsNone() || !FMath::IsFinite(Step.DelaySeconds) || Step.DelaySeconds < 0.0f ||
            Step.DelaySeconds > Definition.MaxStepDelaySeconds || Step.DelaySeconds > Definition.MaxTotalLifetimeSeconds)
            return false;
    }
    for (const FGamePlatformVFXCompositeStep& Step : Definition.Steps)
    {
        FGamePlatformVFXRequest ChildRequest = Request;
        ChildRequest.Parameters.Append(Step.ParameterOverrides);
        ChildRequest.CompositeDepth = Request.CompositeDepth + 1;

        if (Step.DelaySeconds <= KINDA_SMALL_NUMBER)
        {
            if (!PlayChild(Step.DefinitionId, ChildRequest, ParentHandle)) return false;
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

        if (!RegisterTimer(ParentHandle, TimerHandle))
        {
            World.GetTimerManager().ClearTimer(TimerHandle);
            return false;
        }
    }
    return true;
}
