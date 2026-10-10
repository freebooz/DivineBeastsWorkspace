// 本文件属于GamePlatform平台层 GamePlatformVFX，负责对外稳定合同/值类型；所属线程、空值、代次和所有权按相邻说明。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Types/GamePlatformVFXParameters.h"
#include "Types/GamePlatformVFXTypes.h"
#include "GamePlatformVFXDefinition.generated.h"

class UNiagaraSystem;
class UNiagaraEffectType;

/** 所有平台 VFX Definition 的公共根类。只保存表现执行所需的中立数据。 */
UCLASS(Abstract, BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXDefinition : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()

public:
    /** 兼容旧调用方的只读逻辑ID视图；真实身份唯一来源为基类 LogicalId。 */
    FName GetDefinitionId() const
    {
        const FString Canonical = LogicalId.ToString();
        return Canonical.IsEmpty() ? NAME_None : FName(*Canonical);
    }
    EGamePlatformVFXBehavior GetBehavior() const { return Behavior; }
    EGamePlatformVFXContentCategory GetContentCategory() const { return ContentCategory; }
    const TSoftObjectPtr<UNiagaraSystem>& GetNiagaraSystem() const { return NiagaraSystem; }
    const TSoftObjectPtr<UNiagaraEffectType>& GetEffectType() const { return EffectType; }
    /** 编辑器验证使用：读取全部平台变体，运行时不得修改返回容器。 */
    const TMap<FName, TSoftObjectPtr<UNiagaraSystem>>& GetPlatformVariants() const { return PlatformVariants; }
    /** 编辑器验证使用：读取全部质量变体，运行时不得修改返回容器。 */
    const TMap<EGamePlatformVFXQualityTier, TSoftObjectPtr<UNiagaraSystem>>& GetQualityVariants() const { return QualityVariants; }
    const FGamePlatformVFXParameterSchema& GetParameterSchema() const { return ParameterSchema; }
    const FGamePlatformVFXParameters& GetDefaultParameters() const { return DefaultParameters; }
    const TArray<TSoftObjectPtr<UObject>>& GetPreloadAssets() const { return PreloadAssets; }
    bool AllowsPooling() const { return bAllowPooling; }
    bool UsesScalability() const { return bEnableScalability; }
    bool RequiresLargeWorldCoordinates() const { return bRequireLargeWorldCoordinates; }
    bool RequiresFixedBounds() const { return bRequireFixedBounds; }
    bool ShouldAutoDestroy() const { return bAutoDestroy; }
    float GetMaxLifetimeSeconds() const { return MaxLifetimeSeconds; }
    /** 基础定义软引用只作缺资源回退；实际加载必须经Data，不能同步加载或替代权威事实。 */
    const TSoftObjectPtr<UGamePlatformVFXDefinition>& GetFallbackDefinition() const { return FallbackDefinition; }

    TSoftObjectPtr<UNiagaraSystem> ResolveNiagaraSystem(
        FName PlatformId,
        EGamePlatformVFXQualityTier QualityTier) const;

    virtual FGamePlatformResult ValidateDefinition() const override;
    /** VFX领域附加校验；编辑器需要面向美术输出FText时使用。 */
    bool ValidateVFXDefinition(FText& OutReason) const;
    bool ValidateRequestParameters(
        const FGamePlatformVFXParameters& Parameters,
        FText& OutReason) const;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Identity")
    EGamePlatformVFXBehavior Behavior = EGamePlatformVFXBehavior::Instant;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Identity")
    EGamePlatformVFXContentCategory ContentCategory = EGamePlatformVFXContentCategory::Core;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Assets", meta=(AssetBundles="VFXRuntime"))
    TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;

    /** 用于Niagara原生Scalability/Validation；不在VFX框架中复制EffectType能力。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Scalability", meta=(AssetBundles="VFXRuntime"))
    TSoftObjectPtr<UNiagaraEffectType> EffectType;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Parameters")
    FGamePlatformVFXParameterSchema ParameterSchema;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Parameters")
    FGamePlatformVFXParameters DefaultParameters;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Variants", meta=(AssetBundles="VFXRuntime"))
    TMap<FName, TSoftObjectPtr<UNiagaraSystem>> PlatformVariants;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Variants", meta=(AssetBundles="VFXRuntime"))
    TMap<EGamePlatformVFXQualityTier, TSoftObjectPtr<UNiagaraSystem>> QualityVariants;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Fallback")
    TSoftObjectPtr<UGamePlatformVFXDefinition> FallbackDefinition;

    /** Definition之外需要与其共同预加载并由Lease持有的中立依赖。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Assets", meta=(AssetBundles="VFXRuntime"))
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

};
