#pragma once
#include "Framework/GamePlatformGameModeBase.h"
#include "DivineBeastsWorldGameMode.generated.h"

/** 第三层共享世界装配：复用平台准入/出生门禁，仅选择项目控制器。
 * 服务器可信Bootstrap完成World/Data装配后启动体验；客户端不依赖服务器私有实现。 */
UCLASS()
class DBAWORLDSRUNTIME_API ADivineBeastsWorldGameMode : public AGamePlatformGameModeBase
{
    GENERATED_BODY()
public:
    ADivineBeastsWorldGameMode();
};
