#include "Validation/GamePlatformVFXCompositeValidator.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "Definitions/GamePlatformVFXDefinition.h"

namespace
{
bool VisitComposite(
    const UGamePlatformVFXCompositeDefinition& Definition,
    TSet<FSoftObjectPath>& Visiting,
    TSet<FSoftObjectPath>& Visited)
{
    const FSoftObjectPath Path(Definition.GetPathName());
    if (Visiting.Contains(Path))
    {
        return true;
    }
    if (Visited.Contains(Path))
    {
        return false;
    }

    Visiting.Add(Path);
    for (const FGamePlatformVFXCompositeStep& Step : Definition.Steps)
    {
        UGamePlatformVFXDefinition* Child = Step.Definition.LoadSynchronous();
        if (const UGamePlatformVFXCompositeDefinition* ChildComposite =
            Cast<UGamePlatformVFXCompositeDefinition>(Child))
        {
            if (VisitComposite(*ChildComposite, Visiting, Visited))
            {
                return true;
            }
        }
    }

    Visiting.Remove(Path);
    Visited.Add(Path);
    return false;
}
}

void FGamePlatformVFXCompositeValidator::Validate(
    const UGamePlatformVFXCompositeDefinition& Definition,
    TArray<FGamePlatformVFXValidationIssue>& OutIssues)
{
    if (Definition.Steps.Num() > Definition.MaxChildren ||
        Definition.MaxDepth < 1 || Definition.MaxDepth > 8 ||
        Definition.MaxStepDelaySeconds < 0.0f ||
        Definition.MaxTotalLifetimeSeconds < 0.0f)
    {
        FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
        Issue.RuleId = TEXT("GPVFX.Composite.Limits");
        Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
        Issue.Message = NSLOCTEXT("GamePlatformVFX", "CompositeLimits", "Composite的深度、子节点数量、延迟或总生命周期配置超出平台边界。");
    }

    for (const FGamePlatformVFXCompositeStep& Step : Definition.Steps)
    {
        if (Step.DelaySeconds > Definition.MaxStepDelaySeconds ||
            Step.DelaySeconds > Definition.MaxTotalLifetimeSeconds)
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Composite.Delay");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = NSLOCTEXT("GamePlatformVFX", "CompositeDelay", "Composite步骤延迟超过Definition允许的编排时长。");
            break;
        }
    }

    TSet<FSoftObjectPath> Visiting;
    TSet<FSoftObjectPath> Visited;
    if (VisitComposite(Definition, Visiting, Visited))
    {
        FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
        Issue.RuleId = TEXT("GPVFX.Composite.Cycle");
        Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
        Issue.Message = NSLOCTEXT(
            "GamePlatformVFX",
            "CompositeCycle",
            "Composite Definition 存在直接或间接循环引用。");
    }
}
