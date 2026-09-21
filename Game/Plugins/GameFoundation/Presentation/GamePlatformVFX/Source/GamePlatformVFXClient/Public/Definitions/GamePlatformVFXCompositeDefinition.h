#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "UObject/PrimaryAssetId.h"
#include "GamePlatformVFXCompositeDefinition.generated.h"

USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXCompositeChild
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Composite")
    FPrimaryAssetId DefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Composite", meta=(ClampMin="0.0"))
    float DelaySeconds = 0.0f;
};

UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXCompositeDefinition : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()

public:
    UGamePlatformVFXCompositeDefinition()
    {
        Behavior = EGamePlatformVFXBehavior::Composite;
    }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Composite")
    TArray<FGamePlatformVFXCompositeChild> Children;

    /** 最后一个子效果启动后，父句柄额外保持的时间。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Composite", meta=(ClampMin="0.0"))
    float TailSeconds = 0.1f;
};
