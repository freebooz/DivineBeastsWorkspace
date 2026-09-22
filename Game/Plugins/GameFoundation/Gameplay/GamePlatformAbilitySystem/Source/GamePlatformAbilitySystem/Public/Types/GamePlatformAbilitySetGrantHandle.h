#pragma once
#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "GamePlatformAbilitySetGrantHandle.generated.h"

/** 所有字段与ASC内部登记精确匹配。值拷贝没有第二份资源所有权，不能跨ASC释放。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMABILITYSYSTEM_API FGamePlatformAbilitySetGrantHandle
{
    GENERATED_BODY()
    /** 每个ASC独立随机作用域。 */
    UPROPERTY(BlueprintReadOnly, Category="AbilitySet") FGuid ScopeId;
    /** 本次授权随机身份，不能伪造可复用槽位。 */
    UPROPERTY(BlueprintReadOnly, Category="AbilitySet") FGuid GrantId;
    /** ASC内单调请求号，仅在ScopeId内部解释。 */
    UPROPERTY(BlueprintReadOnly, Category="AbilitySet") int64 Sequence = 0;
    /** 签发时本端Avatar代次，0无效。 */
    UPROPERTY(BlueprintReadOnly, Category="AbilitySet") int64 AvatarGeneration = 0;
    /** 请求的稳定主资产身份。 */
    UPROPERTY(BlueprintReadOnly, Category="AbilitySet") FPrimaryAssetId DefinitionId;
    bool IsValid() const { return ScopeId.IsValid() && GrantId.IsValid() && Sequence > 0 && AvatarGeneration > 0 && DefinitionId.IsValid(); }
    bool operator==(const FGamePlatformAbilitySetGrantHandle& Other) const
    { return ScopeId == Other.ScopeId && GrantId == Other.GrantId && Sequence == Other.Sequence && AvatarGeneration == Other.AvatarGeneration && DefinitionId == Other.DefinitionId; }
};
