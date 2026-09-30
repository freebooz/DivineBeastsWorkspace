#include "Definitions/GamePlatformVFXDefinition.h"
#include "NiagaraSystem.h"

#define LOCTEXT_NAMESPACE "GamePlatformVFXDefinition"

TSoftObjectPtr<UNiagaraSystem> UGamePlatformVFXDefinition::ResolveNiagaraSystem(
    FName PlatformId,
    EGamePlatformVFXQualityTier QualityTier) const
{
    if (!PlatformId.IsNone())
    {
        if (const TSoftObjectPtr<UNiagaraSystem>* Platform = PlatformVariants.Find(PlatformId))
        {
            if (!Platform->IsNull())
            {
                return *Platform;
            }
        }
    }

    if (const TSoftObjectPtr<UNiagaraSystem>* Quality = QualityVariants.Find(QualityTier))
    {
        if (!Quality->IsNull())
        {
            return *Quality;
        }
    }

    return NiagaraSystem;
}

FGamePlatformResult UGamePlatformVFXDefinition::ValidateDefinition() const
{
    const FGamePlatformResult BaseResult = Super::ValidateDefinition();
    if (!BaseResult.IsSuccess())
    {
        return BaseResult;
    }

    FText Reason;
    if (!ValidateVFXDefinition(Reason))
    {
        return FGamePlatformResult::Failure(TEXT("VFX.DefinitionInvalid"), Reason.ToString());
    }
    return FGamePlatformResult::Success();
}

bool UGamePlatformVFXDefinition::ValidateVFXDefinition(FText& OutReason) const
{
    if (Behavior != EGamePlatformVFXBehavior::Composite && NiagaraSystem.IsNull())
    {
        OutReason = LOCTEXT("MissingNiagara", "非Composite VFX Definition必须指定Niagara System。");
        return false;
    }

    if (!FMath::IsFinite(MaxLifetimeSeconds) || MaxLifetimeSeconds < 0.0f)
    {
        OutReason = LOCTEXT("InvalidLifetime", "VFX最大生命周期配置无效。");
        return false;
    }

    if (!ParameterSchema.Validate(DefaultParameters, OutReason, false))
    {
        return false;
    }

    OutReason = FText::GetEmpty();
    return true;
}

bool UGamePlatformVFXDefinition::ValidateRequestParameters(
    const FGamePlatformVFXParameters& Parameters,
    FText& OutReason) const
{
    return ParameterSchema.Validate(Parameters, OutReason);
}

#undef LOCTEXT_NAMESPACE
