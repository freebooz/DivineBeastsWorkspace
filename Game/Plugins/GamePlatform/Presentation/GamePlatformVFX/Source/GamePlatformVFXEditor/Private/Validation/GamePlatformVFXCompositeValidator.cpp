#include "Validation/GamePlatformVFXCompositeValidator.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "Types/GamePlatformId.h"

void FGamePlatformVFXCompositeValidator::Validate(
    const UGamePlatformVFXCompositeDefinition& Definition,
    TArray<FGamePlatformVFXValidationIssue>& OutIssues)
{
    if (Definition.Steps.Num() > Definition.MaxChildren ||
        Definition.MaxDepth < 1 || Definition.MaxDepth > 8 ||
        Definition.MaxStepDelaySeconds < 0.0f ||
        Definition.MaxTotalLifetimeSeconds <= 0.0f)
    {
        FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
        Issue.RuleId = TEXT("GPVFX.Composite.Limits");
        Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
        Issue.Message = NSLOCTEXT("GamePlatformVFX", "CompositeLimits", "Composite的深度、子节点数量、延迟或总生命周期配置超出平台边界。");
    }

    const FName SelfId = Definition.GetDefinitionId();
    for (const FGamePlatformVFXCompositeStep& Step : Definition.Steps)
    {
        FGamePlatformId Parsed;
        if (Step.DefinitionId.IsNone() ||
            !FGamePlatformId::TryParse(Step.DefinitionId.ToString(), Parsed))
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Composite.InvalidDefinitionId");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = NSLOCTEXT("GamePlatformVFX", "CompositeInvalidDefinitionId", "Composite步骤必须使用有效的GamePlatform逻辑DefinitionId。");
            continue;
        }

        if (!SelfId.IsNone() && FName(*Parsed.ToString()) == SelfId)
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Composite.SelfCycle");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = NSLOCTEXT("GamePlatformVFX", "CompositeSelfCycle", "Composite不能直接引用自身DefinitionId；间接依赖环由GamePlatformData统一租约校验阻断。");
        }

        if (Step.DelaySeconds > Definition.MaxStepDelaySeconds ||
            Step.DelaySeconds > Definition.MaxTotalLifetimeSeconds)
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Composite.Delay");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = NSLOCTEXT("GamePlatformVFX", "CompositeDelay", "Composite步骤延迟超过Definition允许的编排时长。");
        }
    }
}
