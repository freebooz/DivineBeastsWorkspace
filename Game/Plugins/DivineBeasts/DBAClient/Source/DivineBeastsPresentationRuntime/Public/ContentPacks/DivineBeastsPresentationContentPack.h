#pragma once

#include "CoreMinimal.h"
#include "GamePlatformPresentationCatalog.h"
#include "DivineBeastsPresentationContentPack.generated.h"

/** FDivineBeastsPresentationContentPackHandle（项目表现内容包激活句柄）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSPRESENTATIONRUNTIME_API FDivineBeastsPresentationContentPackHandle
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid Id;
    bool IsValid() const { return Id.IsValid(); }
};

/**
 * FDivineBeastsPresentationContentPackFragment（项目表现内容包目录片段）。
 * 只使用逻辑DefinitionId；不携带具体资源路径。
 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSPRESENTATIONRUNTIME_API FDivineBeastsPresentationContentPackFragment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ContentPackId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Revision = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGamePlatformPresentationContextScope LifecycleScope =
        EGamePlatformPresentationContextScope::World;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGamePlatformPresentationCatalogFragment CatalogFragment;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> LogicalPreloadDefinitionIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequiredPreload = false;

    bool IsValid(FString& OutError) const;
};
