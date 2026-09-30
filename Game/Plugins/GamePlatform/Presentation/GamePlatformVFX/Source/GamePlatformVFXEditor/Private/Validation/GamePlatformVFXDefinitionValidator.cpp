#include "Validation/GamePlatformVFXDefinitionValidator.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "NiagaraSystem.h"

void FGamePlatformVFXDefinitionValidator::Validate(
    const UGamePlatformVFXDefinition& Definition,
    TArray<FGamePlatformVFXValidationIssue>& OutIssues)
{
    const FGamePlatformResult DefinitionResult = Definition.ValidateDefinition();
    if (!DefinitionResult.IsSuccess())
    {
        FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
        Issue.RuleId = TEXT("GPVFX.Definition.Invalid");
        Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
        Issue.Message = FText::FromString(DefinitionResult.Message);
    }

    if (Definition.GetDefinitionId().IsNone())
    {
        FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
        Issue.RuleId = TEXT("GPVFX.Definition.MissingId");
        Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
        Issue.Message = NSLOCTEXT("GamePlatformVFX", "MissingDefinitionId", "VFX Definition必须声明稳定DefinitionId。");
    }

    if (Definition.UsesScalability() && Definition.GetEffectType().IsNull())
    {
        FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
        Issue.RuleId = TEXT("GPVFX.Definition.MissingEffectType");
        Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
        Issue.Message = NSLOCTEXT("GamePlatformVFX", "MissingEffectType", "启用Scalability的VFX Definition必须关联Niagara Effect Type。");
    }

    const TSoftObjectPtr<UNiagaraSystem> SelectedSystem =
        Definition.ResolveNiagaraSystem(NAME_None, EGamePlatformVFXQualityTier::High);
    if (UNiagaraSystem* System = SelectedSystem.Get())
    {
        if (Definition.RequiresLargeWorldCoordinates() && !System->SupportsLargeWorldCoordinates())
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Definition.LWC");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = NSLOCTEXT("GamePlatformVFX", "MissingLWC", "Definition要求LWC，但Niagara System未启用Large World Coordinates支持。");
        }

        if (Definition.RequiresFixedBounds() && !System->bFixedBounds)
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Definition.FixedBounds");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = NSLOCTEXT("GamePlatformVFX", "MissingFixedBounds", "Definition要求Fixed Bounds，但Niagara System未启用固定边界。");
        }
    }

    if (Definition.GetBehavior() != EGamePlatformVFXBehavior::Composite &&
        Definition.GetNiagaraSystem().IsNull())
    {
        FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
        Issue.RuleId = TEXT("GPVFX.Definition.MissingNiagara");
        Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
        Issue.Message = NSLOCTEXT("GamePlatformVFX", "MissingNiagara", "非复合 VFX Definition 必须指定 Niagara System。");
    }

    if (const UGamePlatformVFXCompositeDefinition* Composite =
        Cast<UGamePlatformVFXCompositeDefinition>(&Definition))
    {
        if (Composite->Steps.IsEmpty())
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Composite.Empty");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = NSLOCTEXT("GamePlatformVFX", "EmptyComposite", "Composite Definition 至少需要一个子步骤。");
        }
    }
}
