#pragma once

#include "UI/Screens/DivineBeastsArenaScreenBase.h"
#include "DivineBeastsMatchmakingScreen.generated.h"

/** UDivineBeastsMatchmakingScreen（神兽联盟竞技匹配页面）：展示1v1～5v5模式并提交匹配意图。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSARENACLIENT_API UDivineBeastsMatchmakingScreen
    : public UDivineBeastsArenaScreenBase
{
    GENERATED_BODY()

public:
    UDivineBeastsMatchmakingScreen()
    {
        ExpectedSurfaceId = TEXT("UI.Screen.Matchmaking");
    }
};
