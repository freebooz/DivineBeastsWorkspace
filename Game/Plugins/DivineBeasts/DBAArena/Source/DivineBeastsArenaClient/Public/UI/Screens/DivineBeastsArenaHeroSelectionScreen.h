#pragma once

#include "UI/Screens/DivineBeastsArenaScreenBase.h"
#include "DivineBeastsArenaHeroSelectionScreen.generated.h"

/** UDivineBeastsArenaHeroSelectionScreen（神兽联盟竞技英雄选择页面）：展示可选英雄与队伍只读状态。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSARENACLIENT_API UDivineBeastsArenaHeroSelectionScreen
    : public UDivineBeastsArenaScreenBase
{
    GENERATED_BODY()

public:
    UDivineBeastsArenaHeroSelectionScreen()
    {
        ExpectedSurfaceId = TEXT("UI.Screen.ArenaHeroSelection");
    }
};
