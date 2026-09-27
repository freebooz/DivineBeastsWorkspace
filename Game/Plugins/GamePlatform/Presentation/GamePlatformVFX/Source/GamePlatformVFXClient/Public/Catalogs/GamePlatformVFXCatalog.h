#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Catalogs/GamePlatformVFXCatalogTypes.h"
#include "GamePlatformVFXCatalog.generated.h"

/** 可按项目/英雄/世界作用域叠加注册的 VFX 语义目录。 */
UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXCatalog : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FName ScopeId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    int32 Priority = 0;

    /** 内容修订用于诊断和缓存失效证据，不参与Gameplay版本判断。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FString ContentRevision = TEXT("1");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    TArray<FGamePlatformVFXCatalogEntry> Entries;
};
