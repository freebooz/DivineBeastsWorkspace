#include "Definitions/GamePlatformVFXDefinition.h"
#include "NiagaraSystem.h"

#define LOCTEXT_NAMESPACE "GamePlatformVFXDefinition"

FPrimaryAssetId UGamePlatformVFXDefinition::GetPrimaryAssetId() const
{
    const FName AssetName = DefinitionId.IsNone() ? GetFName() : DefinitionId;
    return FPrimaryAssetId(FPrimaryAssetType(TEXT("GamePlatformVFXDefinition")), AssetName);
}

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

bool UGamePlatformVFXDefinition::ValidateDefinition(FText& OutReason) const
{
    if (DefinitionId.IsNone())
    {
        OutReason = LOCTEXT("MissingDefinitionId", "VFX DefinitionId不能为空。");
        return false;
    }

    if (Behavior != EGamePlatformVFXBehavior::Composite && NiagaraSystem.IsNull())
    {
        OutReason = LOCTEXT("MissingNiagara", "非Composite VFX Definition必须指定Niagara System。");
        return false;
    }

    if (Version < 1 || Revision < 1 || MaxLifetimeSeconds < 0.0f)
    {
        OutReason = LOCTEXT("InvalidVersionOrLifetime", "VFX版本、修订号或生命周期配置无效。");
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
