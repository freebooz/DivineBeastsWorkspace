#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformVFXHandle.h"
#include "Types/GamePlatformVFXTypes.h"
#include "GamePlatformVFXResult.generated.h"

/** VFX 请求结果。Queued 表示异步加载已受理，Handle 立即可用于取消/停止。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    EGamePlatformVFXResultCode Code = EGamePlatformVFXResultCode::InvalidRequest;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    FGamePlatformVFXHandle Handle;

    bool IsAccepted() const
    {
        return Code == EGamePlatformVFXResultCode::Played || Code == EGamePlatformVFXResultCode::Queued || Code == EGamePlatformVFXResultCode::AlreadyCompleted;
    }
};
