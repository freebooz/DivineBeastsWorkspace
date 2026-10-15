// 平台中立世界级VFX请求软预算规则；画质由客户端设置决定，专服不执行纯表现。
#pragma once
#include "CoreMinimal.h"
#include "Types/GamePlatformVFXTypes.h"
#include "GamePlatformVFXWorldBudgetRule.generated.h"

/** 可选的WorldId前缀/画质请求软上限，非Niagara System或GPU粒子数量。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXWorldBudgetRule
{
    GENERATED_BODY()
    /** 完整WorldId或带分隔符的命名空间前缀；空值禁止匹配。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX|Scalability") FString WorldIdPrefix;
    /** 客户端效果画质。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX|Scalability") EGamePlatformVFXQualityTier QualityTier = EGamePlatformVFXQualityTier::High;
    /** 普通战斗及以下请求累计软限制。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX|Scalability", meta=(ClampMin="1")) int32 MaxCombatRequests = 80;
    /** 状态及以下请求累计软限制。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX|Scalability", meta=(ClampMin="1")) int32 MaxStatusRequests = 50;
    /** 环境请求累计软限制。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX|Scalability", meta=(ClampMin="1")) int32 MaxAmbientRequests = 10;
};
