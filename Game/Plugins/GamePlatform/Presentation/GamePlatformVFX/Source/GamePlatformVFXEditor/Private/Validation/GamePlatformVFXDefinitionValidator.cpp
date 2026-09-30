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

    // 编辑器验证允许同步装载软引用，确保默认、平台和质量变体全部接受同一LWC/Bounds约束。
    const auto ValidateSystem = [&Definition, &OutIssues](
        const TSoftObjectPtr<UNiagaraSystem>& SystemRef,
        const FString& VariantLabel)
    {
        if (SystemRef.IsNull())
        {
            return;
        }

        UNiagaraSystem* System = SystemRef.LoadSynchronous();
        if (!IsValid(System))
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Definition.VariantLoadFailed");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = FText::Format(
                NSLOCTEXT("GamePlatformVFX", "VariantLoadFailed", "VFX Niagara变体无法加载：{0}"),
                FText::FromString(VariantLabel));
            return;
        }

        if (Definition.RequiresLargeWorldCoordinates() && !System->SupportsLargeWorldCoordinates())
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Definition.LWC");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = FText::Format(
                NSLOCTEXT("GamePlatformVFX", "MissingLWCVariant", "VFX Niagara变体未启用Large World Coordinates支持：{0}"),
                FText::FromString(VariantLabel));
        }

        if (Definition.RequiresFixedBounds() && !System->bFixedBounds)
        {
            FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
            Issue.RuleId = TEXT("GPVFX.Definition.FixedBounds");
            Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
            Issue.Message = FText::Format(
                NSLOCTEXT("GamePlatformVFX", "MissingFixedBoundsVariant", "VFX Niagara变体未启用Fixed Bounds：{0}"),
                FText::FromString(VariantLabel));
        }
    };

    ValidateSystem(Definition.GetNiagaraSystem(), TEXT("Default"));
    for (const TPair<FName, TSoftObjectPtr<UNiagaraSystem>>& Pair : Definition.GetPlatformVariants())
    {
        ValidateSystem(Pair.Value, FString::Printf(TEXT("Platform:%s"), *Pair.Key.ToString()));
    }
    for (const TPair<EGamePlatformVFXQualityTier, TSoftObjectPtr<UNiagaraSystem>>& Pair : Definition.GetQualityVariants())
    {
        ValidateSystem(Pair.Value, FString::Printf(TEXT("Quality:%d"), static_cast<int32>(Pair.Key)));
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
