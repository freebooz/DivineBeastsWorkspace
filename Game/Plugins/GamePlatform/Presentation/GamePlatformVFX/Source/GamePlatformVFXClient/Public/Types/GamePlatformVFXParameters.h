#pragma once

#include "CoreMinimal.h"
#include "GamePlatformVFXParameters.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformVFXParameterType : uint8
{
    Float,
    Vector,
    Position,
    Color,
    Integer,
    Boolean
};

/** Definition声明的单个允许参数；公共请求不得绕过该Schema。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXParameterRule
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FName Name = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    EGamePlatformVFXParameterType Type = EGamePlatformVFXParameterType::Float;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    bool bRequired = false;

    /** 仅Float/Integer使用；越界请求直接拒绝而不是静默猜值。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    float MinValue = -1000000000.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    float MaxValue = 1000000000.0f;
};

/** 运行时Niagara参数集合；Position与普通Vector显式区分以支持LWC。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXParameters
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    TMap<FName, float> FloatParameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    TMap<FName, FVector> VectorParameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    TMap<FName, FVector> PositionParameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    TMap<FName, FLinearColor> ColorParameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    TMap<FName, int32> IntegerParameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    TMap<FName, bool> BooleanParameters;

    int32 Num() const;
    void Append(const FGamePlatformVFXParameters& Other);
};

/** Definition参数白名单和数量/范围边界。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXParameterSchema
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX", meta=(ClampMin="0", ClampMax="64"))
    int32 MaxOverrideCount = 16;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    TArray<FGamePlatformVFXParameterRule> Rules;

    bool Validate(
        const FGamePlatformVFXParameters& Parameters,
        FText& OutReason,
        bool bCheckRequiredParameters = true) const;
};
