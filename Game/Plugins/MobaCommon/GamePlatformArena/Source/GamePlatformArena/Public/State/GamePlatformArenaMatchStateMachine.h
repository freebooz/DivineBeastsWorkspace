#pragma once

#include "CoreMinimal.h"
#include "Arena/GamePlatformArenaTypes.h"

/** FGamePlatformArenaMatchStateMachine（比赛阶段状态机）。
 *  所有阶段变更必须经由该对象验证，避免散落直接赋值。
 */
class GAMEPLATFORMARENA_API FGamePlatformArenaMatchStateMachine
{
public:
    EGamePlatformArenaMatchPhase GetPhase() const { return Phase; }
    int32 GetRevision() const { return Revision; }

    bool TryTransition(EGamePlatformArenaMatchPhase NewPhase, FString& OutError);
    static bool CanTransition(EGamePlatformArenaMatchPhase From, EGamePlatformArenaMatchPhase To);

private:
    EGamePlatformArenaMatchPhase Phase = EGamePlatformArenaMatchPhase::Uninitialized;
    int32 Revision = 0;
};
