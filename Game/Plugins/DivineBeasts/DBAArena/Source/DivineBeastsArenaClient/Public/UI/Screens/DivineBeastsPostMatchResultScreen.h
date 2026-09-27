#pragma once

#include "UI/Screens/DivineBeastsArenaScreenBase.h"
#include "DivineBeastsPostMatchResultScreen.generated.h"

/** UDivineBeastsPostMatchResultScreen（神兽联盟赛后结算页面）：展示已提交/确认的比赛结果并提供返回世界入口。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSARENACLIENT_API UDivineBeastsPostMatchResultScreen
    : public UDivineBeastsArenaScreenBase
{
    GENERATED_BODY()

public:
    UDivineBeastsPostMatchResultScreen()
    {
        ExpectedSurfaceId = TEXT("UI.Screen.PostMatchResult");
    }
};
