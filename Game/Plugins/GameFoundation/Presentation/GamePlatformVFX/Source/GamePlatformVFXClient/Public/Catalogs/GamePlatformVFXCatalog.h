#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Catalogs/GamePlatformVFXCatalogTypes.h"
#include "GamePlatformVFXCatalog.generated.h"

/** 可由平台、MOBA、项目或内容包贡献的 VFX 映射目录资产。 */
UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXCatalog : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FName StableId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX")
    TArray<FGamePlatformVFXCatalogEntry> Entries;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
