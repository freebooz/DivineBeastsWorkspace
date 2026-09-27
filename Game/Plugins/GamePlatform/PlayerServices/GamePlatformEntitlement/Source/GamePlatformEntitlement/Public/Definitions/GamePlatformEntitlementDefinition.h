#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/GamePlatformEntitlementTypes.h"
#include "GamePlatformEntitlementDefinition.generated.h"

/**
 * 平台Entitlement Definition（权益定义）。
 * 只保存稳定ID和中立目标类型；具体Hero/Skin目录由项目层映射。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMENTITLEMENT_API UGamePlatformEntitlementDefinition final
    : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entitlement")
    FName EntitlementId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entitlement")
    FName Category = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entitlement")
    EGamePlatformEntitlementTargetType TargetType =
        EGamePlatformEntitlementTargetType::Unknown;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entitlement")
    FName TargetId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entitlement", meta=(ClampMin="1"))
    int32 Version = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entitlement")
    FName DisplayMetadataId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Entitlement")
    FName DefaultPolicy = TEXT("Locked");

    bool IsStructurallyValid() const
    {
        return !EntitlementId.IsNone() &&
               !Category.IsNone() &&
               TargetType != EGamePlatformEntitlementTargetType::Unknown &&
               !TargetId.IsNone() &&
               Version > 0;
    }
};
