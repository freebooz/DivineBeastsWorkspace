#pragma once

#include "Definitions/GamePlatformVFXDefinition.h"
#include "Types/GamePlatformVFXParameters.h"
#include "GamePlatformVFXCompositeDefinition.generated.h"

USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXCompositeStep
{
    GENERATED_BODY()

    /** 子Definition统一使用GamePlatformData逻辑身份，不直接保存第二套Definition软对象真源。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FName DefinitionId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX", meta=(ClampMin="0.0"))
    float DelaySeconds = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FGamePlatformVFXParameters ParameterOverrides;
};

UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXCompositeDefinition final : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()

public:
    UGamePlatformVFXCompositeDefinition();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    TArray<FGamePlatformVFXCompositeStep> Steps;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Composite", meta=(ClampMin="1", ClampMax="8"))
    int32 MaxDepth = 4;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Composite", meta=(ClampMin="1", ClampMax="64"))
    int32 MaxChildren = 24;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Composite", meta=(ClampMin="0.0", ClampMax="60.0"))
    float MaxStepDelaySeconds = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Composite", meta=(ClampMin="0.0", ClampMax="120.0"))
    float MaxTotalLifetimeSeconds = 30.0f;
};
