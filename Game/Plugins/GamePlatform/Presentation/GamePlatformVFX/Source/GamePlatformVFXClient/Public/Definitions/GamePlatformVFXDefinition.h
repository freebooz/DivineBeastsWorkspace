#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/GamePlatformVFXParameters.h"
#include "Types/GamePlatformVFXTypes.h"
#include "GamePlatformVFXDefinition.generated.h"

class UNiagaraSystem;
class UNiagaraEffectType;

/** 所有平台 VFX Definition 的公共根类。只保存表现执行所需的中立数据。 */
UCLASS(Abstract, BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    FName GetDefinitionId() const { return DefinitionId; }
    EGamePlatformVFXBehavior GetBehavior() const { return Behavior; }
    EGamePlatformVFXContentCategory GetContentCategory() const { return ContentCategory; }
    const TSoftObjectPtr<UNiagaraSystem>& GetNiagaraSystem() const { return NiagaraSystem; }
    const TSoftObjectPtr<UNiagaraEffectType>& GetEffectType() const { return EffectType; }
    const FGamePlatformVFXParameterSchema& GetParameterSchema() const { return ParameterSchema; }
    const FGamePlatformVFXParameters& GetDefaultParameters() const { return DefaultParameters; }
    const TArray<TSoftObjectPtr<UObject>>& GetPreloadAssets() const { return PreloadAssets; }
    bool AllowsPooling() const { return bAllowPooling; }
    bool UsesScalability() const { return bEnableScalability; }
    bool RequiresLargeWorldCoordinates() const { return bRequireLargeWorldCoordinates; }
    bool RequiresFixedBounds() const { return bRequireFixedBounds; }
    bool ShouldAutoDestroy() const { return bAutoDestroy; }
    float GetMaxLifetimeSeconds() const { return MaxLifetimeSeconds; }
    int32 GetVersion() const { return Version; }
    int32 GetRevision() const { return Revision; }

    TSoftObjectPtr<UNiagaraSystem> ResolveNiagaraSystem(
        FName PlatformId,
        EGamePlatformVFXQualityTier QualityTier) const;

    bool ValidateDefinition(FText& OutReason) const;
    bool ValidateRequestParameters(
        const FGamePlatformVFXParameters& Parameters,
        FText& OutReason) const;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Identity")
    FName DefinitionId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Identity")
    EGamePlatformVFXBehavior Behavior = EGamePlatformVFXBehavior::Instant;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Identity")
    EGamePlatformVFXContentCategory ContentCategory = EGamePlatformVFXContentCategory::Core;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Assets")
    TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;

    /** 用于Niagara原生Scalability/Validation；不在VFX框架中复制EffectType能力。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Scalability")
    TSoftObjectPtr<UNiagaraEffectType> EffectType;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Parameters")
    FGamePlatformVFXParameterSchema ParameterSchema;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Parameters")
    FGamePlatformVFXParameters DefaultParameters;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Variants")
    TMap<FName, TSoftObjectPtr<UNiagaraSystem>> PlatformVariants;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Variants")
    TMap<EGamePlatformVFXQualityTier, TSoftObjectPtr<UNiagaraSystem>> QualityVariants;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Fallback")
    TSoftObjectPtr<UGamePlatformVFXDefinition> FallbackDefinition;

    /** Definition之外需要与其共同预加载并由Lease持有的中立依赖。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Assets")
    TArray<TSoftObjectPtr<UObject>> PreloadAssets;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Pooling")
    bool bAllowPooling = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Scalability")
    bool bEnableScalability = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|World")
    bool bRequireLargeWorldCoordinates = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|World")
    bool bRequireFixedBounds = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Lifetime")
    bool bAutoDestroy = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Lifetime", meta=(ClampMin="0.0"))
    float MaxLifetimeSeconds = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Version", meta=(ClampMin="1"))
    int32 Version = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Version", meta=(ClampMin="1"))
    int32 Revision = 1;
};
