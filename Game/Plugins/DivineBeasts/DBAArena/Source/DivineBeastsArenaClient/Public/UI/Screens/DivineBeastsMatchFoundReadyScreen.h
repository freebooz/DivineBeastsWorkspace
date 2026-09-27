#pragma once

#include "UI/GamePlatformMobaArenaModalScreenBase.h"
#include "DivineBeastsMatchFoundReadyScreen.generated.h"

/**
 * UDivineBeastsMatchFoundReadyScreen（神兽联盟匹配成功准备页面）。
 *
 * 使用 MOBA 通用 Modal（模态）基类，阻断下层输入；接受/拒绝结果仍由匹配服务权威处理。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSARENACLIENT_API UDivineBeastsMatchFoundReadyScreen
    : public UGamePlatformMobaArenaModalScreenBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Arena|UI")
    FName GetExpectedSurfaceId() const
    {
        return TEXT("UI.Screen.MatchFoundReady");
    }
};
