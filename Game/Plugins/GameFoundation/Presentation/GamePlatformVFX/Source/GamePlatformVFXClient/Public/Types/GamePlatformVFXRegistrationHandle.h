#pragma once

#include "CoreMinimal.h"
#include "GamePlatformVFXRegistrationHandle.generated.h"

/** Catalog 注册句柄；内容包失活前使用该句柄撤销注册。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXRegistrationHandle
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    FGuid RegistrationId;

    bool IsValid() const { return RegistrationId.IsValid(); }
};
