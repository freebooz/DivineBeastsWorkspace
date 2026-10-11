// World定义组合可选群落配置；保持唯一VFX机制及Data加载合同。
#pragma once
#include "Definitions/GamePlatformVFXDefinition.h"
#include "Environment/GamePlatformFaunaConfig.h"
#include "GamePlatformVFXWorldDefinition.generated.h"

/** 世界环境特效。群落是可选组合行为，普通鸟群、天气等定义默认不启用。 */
UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXWorldDefinition final : public UGamePlatformVFXDefinition
{
    GENERATED_BODY()
public:
    UGamePlatformVFXWorldDefinition();
    virtual FGamePlatformResult ValidateDefinition() const override;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX|Fauna") bool bEnableFauna = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX|Fauna", meta=(EditCondition="bEnableFauna")) FGamePlatformFaunaConfig Fauna;
};
