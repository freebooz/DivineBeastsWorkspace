#pragma once

#include "UI/GamePlatformMobaArenaScreenBase.h"
#include "DivineBeastsArenaScreenBase.generated.h"

/**
 * UDivineBeastsArenaScreenBase（神兽联盟竞技页面基类）。
 *
 * 只增加项目层稳定 SurfaceId（界面表面身份）；
 * 竞技状态、页面生命周期和输入策略继续复用 MobaCommon / GamePlatform。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSARENACLIENT_API UDivineBeastsArenaScreenBase
    : public UGamePlatformMobaArenaScreenBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Arena|UI")
    FName GetExpectedSurfaceId() const { return ExpectedSurfaceId; }

protected:
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Arena|UI")
    FName ExpectedSurfaceId = NAME_None;
};
