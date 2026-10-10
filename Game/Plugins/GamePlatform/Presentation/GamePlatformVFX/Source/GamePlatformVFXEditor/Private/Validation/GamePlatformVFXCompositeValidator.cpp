// 平台编辑器复合定义校验：通过Data公开主资产合同检查Steps依赖声明，不加载或执行特效。
#include "Validation/GamePlatformVFXCompositeValidator.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
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

        const FPrimaryAssetId ChildId(Definition.DefinitionAssetType(), FName(*Parsed.ToString()));
        if (!Definition.RequiredDefinitions.Contains(ChildId))
        {
            auto& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Composite.DependencyMissing");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = NSLOCTEXT("GamePlatformVFX", "CompositeDependencyMissing", "每个Steps子定义必须声明到RequiredDefinitions，才能由Data完整预检缺失与间接环。");
        }
        if (!SelfId.IsNone() && FName(*Parsed.ToString()) == SelfId)
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Composite.SelfCycle");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = NSLOCTEXT("GamePlatformVFX", "CompositeSelfCycle", "Composite不能直接引用自身DefinitionId；已完整声明的RequiredDefinitions间接依赖环由GamePlatformData统一租约校验阻断。");
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
