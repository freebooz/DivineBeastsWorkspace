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
    /** 纯配置转换，无输入或暂停副作用；GameAndUI按下时临时捕获，UIOnly不捕获且忽略移动/视角。 */
    static FUIInputConfig BuildInputConfig(EGamePlatformUIInputMode InputMode);
    /** 只判断暂停资格；无World返回false，联网世界不允许通过UI暂停全世界。 */
    static bool CanPauseWorld(const UWorld* World, EGamePlatformUIPausePolicy PausePolicy);
};
