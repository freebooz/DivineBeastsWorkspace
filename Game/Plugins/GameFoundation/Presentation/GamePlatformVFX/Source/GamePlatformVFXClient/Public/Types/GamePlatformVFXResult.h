#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformVFXHandle.h"
#include "Types/GamePlatformVFXTypes.h"
#include "GamePlatformVFXResult.generated.h"

/** Play 调用的同步返回结果。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXPlayResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    EGamePlatformVFXPlayResultCode Code = EGamePlatformVFXPlayResultCode::InvalidRequest;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    FGamePlatformVFXHandle Handle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    FString Message;

    bool IsAccepted() const { return Code == EGamePlatformVFXPlayResultCode::Accepted && Handle.IsValid(); }
};
