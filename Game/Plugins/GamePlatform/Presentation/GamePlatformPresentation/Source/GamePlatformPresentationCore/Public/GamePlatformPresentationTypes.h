#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GamePlatformPresentationContext.h"
#include "GamePlatformPresentationTypes.generated.h"

/** 跨表现插件共享的中立语义事件，不携带UI/VFX/SFX具体实现类型。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPRESENTATIONCORE_API FGamePlatformPresentationEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FGameplayTag SemanticTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FName ContextId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FText UserFacingText;
};

/** EGamePlatformPresentationPriority（平台表现请求优先级）。 */
UENUM(BlueprintType)
enum class EGamePlatformPresentationPriority : uint8
{
    Low,
    Normal,
    High,
    Critical
};

/** EGamePlatformPresentationLifetime（平台表现生命周期类型）。 */
UENUM(BlueprintType)
enum class EGamePlatformPresentationLifetime : uint8
{
    Instant,
    Timed,
    Persistent
};

/** EGamePlatformPresentationPredictionState（平台表现预测状态）。 */
UENUM(BlueprintType)
enum class EGamePlatformPresentationPredictionState : uint8
{
    Confirmed,
    Predicted,
    Corrected,
    Cancelled
};

/** EGamePlatformPresentationSubmitResult（平台表现请求提交结果）。 */
UENUM(BlueprintType)
enum class EGamePlatformPresentationSubmitResult : uint8
{
    Submitted,
    ProviderMissing,
    InvalidRequest,
    StaleWorld
};

/**
 * FGamePlatformPresentationRequest（平台表现请求）。
 * 这是表现Provider（提供者）之间共享的中立请求，不携带Niagara、Sound、Widget等具体资源类型。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPRESENTATIONCORE_API FGamePlatformPresentationRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FGuid RequestId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FGameplayTag SemanticTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FName ContextId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FName SourceId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FName TargetId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FVector SourceLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FVector TargetLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FVector ImpactLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FVector ImpactNormal = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    float Magnitude = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FGameplayTagContainer ContextTags;

    /** 类型化项目/世界/Hero上下文；不含资源路径、Token或Backend DTO。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FGamePlatformPresentationContext Context;

    /** Catalog解析得到的中立Provider channel（提供者通道）。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FName ProviderChannel = NAME_None;

    /** Catalog解析得到的逻辑Definition ID，不是具体资产路径。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FName DefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FString ContentRevision;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    EGamePlatformPresentationPriority Priority = EGamePlatformPresentationPriority::Normal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    EGamePlatformPresentationLifetime Lifetime = EGamePlatformPresentationLifetime::Instant;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    EGamePlatformPresentationPredictionState PredictionState = EGamePlatformPresentationPredictionState::Confirmed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    int32 WorldGeneration = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    int32 RequestGeneration = 0;

    bool IsValid() const
    {
        return RequestId.IsValid() && SemanticTag.IsValid();
    }
};
