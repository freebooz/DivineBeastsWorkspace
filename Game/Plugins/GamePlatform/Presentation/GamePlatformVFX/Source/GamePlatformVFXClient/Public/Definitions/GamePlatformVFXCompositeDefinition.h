#pragma once

#include "Definitions/GamePlatformVFXDefinition.h"
#include "Types/GamePlatformVFXParameters.h"
#include "GamePlatformVFXCompositeDefinition.generated.h"

USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXCompositeStep
{
    GENERATED_BODY()

    /** 子Definition使用GamePlatformData逻辑身份；对应主资产ID必须同时列入RequiredDefinitions，缺边拒绝启动，间接环由Data完整租约图阻断。 */
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
    /** 运行时也执行Composite边界校验，不能只依赖编辑器门禁。 */
    virtual FGamePlatformResult ValidateDefinition() const override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    TArray<FGamePlatformVFXCompositeStep> Steps;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Composite", meta=(ClampMin="1", ClampMax="8"))
    int32 MaxDepth = 4;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Composite", meta=(ClampMin="1", ClampMax="64"))
    int32 MaxChildren = 24;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Composite", meta=(ClampMin="0.0", ClampMax="60.0"))
    float MaxStepDelaySeconds = 10.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX|Composite", meta=(ClampMin="0.1", ClampMax="120.0"))
    float MaxTotalLifetimeSeconds = 30.0f;
};
