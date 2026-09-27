#pragma once

#include "CoreMinimal.h"
#include "Input/UIActionBindingHandle.h"
#include "GamePlatformUITypes.h"
#include "GamePlatformUIInputPolicy.generated.h"

class UWorld;

/** 将平台枚举转换为 CommonUI Input Config；不创建第二套输入路由器。 */
UCLASS()
class GAMEPLATFORMUICLIENT_API UGamePlatformUIInputPolicy : public UObject
{
    GENERATED_BODY()

public:
    static FUIInputConfig BuildInputConfig(EGamePlatformUIInputMode InputMode);
    static bool CanPauseWorld(const UWorld* World, EGamePlatformUIPausePolicy PausePolicy);
};
