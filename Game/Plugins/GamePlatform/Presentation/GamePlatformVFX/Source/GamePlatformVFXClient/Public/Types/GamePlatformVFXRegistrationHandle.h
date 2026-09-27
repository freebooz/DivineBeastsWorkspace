#pragma once

#include "CoreMinimal.h"
#include "GamePlatformVFXRegistrationHandle.generated.h"

/** Catalog 注册句柄。注销必须使用原句柄，避免按对象地址猜测所有权。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXRegistrationHandle
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VFX")
    FGuid Id;

    bool IsValid() const { return Id.IsValid(); }
};
