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
    // 已有反射/Blueprint数值0..3保持稳定；不得重排或复用已发布身份。
    Submitted = 0,
    ProviderMissing = 1,
    InvalidRequest = 2,
    StaleWorld = 3,
    /**
     * 当前同作用域、同事实身份已有同步提交栈尚未得到Provider结果，本调用仅保留在途资格。
     * 不代表Provider已受理，也不代表播放成功；不得按Submitted处理。
     * 原提交栈返回后完成资格：已受理预测的确认仅升级去重；预测拒绝时确认需真实提交。
     * 原作用域关闭/换代则撤销留存资格。调用方可在原同步栈结束后以同身份查询/重试，
     * 依实际终态决定受理或失败；本枚举不建立独立异步队列/网络协议或通用完成事件。
     * 新值追加为4；消费插件/UHT需统一重编译，Blueprint需检查新增分支及默认分支。
     */
    Pending = 4
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
