#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformVFXParameters.h"
#include "Types/GamePlatformVFXSpawnContext.h"
#include "Types/GamePlatformVFXTypes.h"
#include "GamePlatformVFXRequest.generated.h"

/** 由表现层提交给 VFX 服务的中立语义请求。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXRequest
{
    GENERATED_BODY()

    /** 跨Presentation/VFX链路的请求标识；用于去重，不是网络安全令牌。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FGuid RequestId;

    /** 同一次能力/动作激活的表现关联标识。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FGuid ActivationId;

    /** 客户端预测关联键；0表示未提供。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    int64 PredictionKey = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FGameplayTag SemanticTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FName ContextId = NAME_None;

    /** Resolver使用的中立上下文标签；不承载Gameplay权威判断。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FGameplayTagContainer ContextTags;

    /** 已由上游Presentation Catalog解析出的逻辑Definition ID；不是资产路径。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FName DefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FName PlatformId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    EGamePlatformVFXQualityTier QualityTier = EGamePlatformVFXQualityTier::High;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    EGamePlatformVFXPredictionState PredictionState = EGamePlatformVFXPredictionState::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    EGamePlatformVFXImportance Importance = EGamePlatformVFXImportance::Combat;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FGamePlatformVFXSpawnContext SpawnContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FGamePlatformVFXParameters Parameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    bool bAllowFallback = true;

    /** Composite内部递归深度；外部正常请求保持0。 */
    UPROPERTY(Transient)
    int32 CompositeDepth = 0;

    bool IsStructurallyValid() const
    {
        return SemanticTag.IsValid() || !DefinitionId.IsNone();
    }
};
