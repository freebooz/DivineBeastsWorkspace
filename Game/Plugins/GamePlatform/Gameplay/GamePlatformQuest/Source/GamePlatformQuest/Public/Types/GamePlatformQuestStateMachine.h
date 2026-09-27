#pragma once

#include "Types/GamePlatformQuestTypes.h"

class GAMEPLATFORMQUEST_API FGamePlatformQuestStateMachine
{
public:
    static bool CanTransition(
        EGamePlatformQuestState From,
        EGamePlatformQuestState To);

    static bool TryTransition(
        EGamePlatformQuestState& InOutState,
        EGamePlatformQuestState To);
};
