#pragma once

#include "UI/Screens/DivineBeastsArenaScreenBase.h"
#include "DivineBeastsScoreboardScreen.generated.h"

/** UDivineBeastsScoreboardScreen（神兽联盟竞技记分板页面）：只展示复制确认的公开比分和玩家统计。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSARENACLIENT_API UDivineBeastsScoreboardScreen
    : public UDivineBeastsArenaScreenBase
{
    GENERATED_BODY()

public:
    UDivineBeastsScoreboardScreen()
    {
        ExpectedSurfaceId = TEXT("UI.Screen.Scoreboard");
    }
};
