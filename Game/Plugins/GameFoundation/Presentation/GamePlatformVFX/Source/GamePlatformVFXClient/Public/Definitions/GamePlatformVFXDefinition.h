#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/GamePlatformVFXParameters.h"
#include "Types/GamePlatformVFXTypes.h"
#include "GamePlatformVFXDefinition.generated.h"

class UNiagaraSystem;

/**
 * VFX 主数据定义根类。
 * StableId 是稳定逻辑身份，不允许由资产文件名隐式推导。
 */
UCLASS(Abstract, BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FName StableId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Version", meta=(ClampMin="1"))
    int32 SchemaVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Version", meta=(ClampMin="0"))
    int32 ContentRevision = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX")
    FName ContentCategory = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    EGamePlatformVFXBehavior Behavior = EGamePlatformVFXBehavior::Instant;

    /** Client Bundle 用于客户端表现资源分组。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX", meta=(AssetBundles="Client"))
    TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX")
    FGamePlatformVFXParameters DefaultParameters;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX")
    EGamePlatformVFXPoolingMode PoolingMode = EGamePlatformVFXPoolingMode::AutoRelease;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX")
    EGamePlatformVFXImportance DefaultImportance = EGamePlatformVFXImportance::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX")
    bool bAutoDestroy = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX")
    bool bPreCullCheck = true;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
