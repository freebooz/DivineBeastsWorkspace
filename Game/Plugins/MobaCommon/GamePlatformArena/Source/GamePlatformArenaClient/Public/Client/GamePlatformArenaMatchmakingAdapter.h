#pragma once

#include "CoreMinimal.h"
#include "Client/GamePlatformArenaClientTypes.h"

/** IGamePlatformArenaMatchmakingAdapter（客户端匹配适配接口）。
 *  真正Queue/MMR/Region/Party原子匹配由Go MatchService负责；UE只调用Gateway稳定接口。
 */
class GAMEPLATFORMARENACLIENT_API IGamePlatformArenaMatchmakingAdapter
{
public:
    virtual ~IGamePlatformArenaMatchmakingAdapter() = default;

    virtual void RequestMatchmaking(
        const FGamePlatformArenaMatchmakingRequest& Request,
        TFunction<void(bool bAccepted, const FString& Error)> Completion) = 0;

    virtual void CancelMatchmaking(
        const FString& ClientRequestId,
        TFunction<void(bool bCancelled, const FString& Error)> Completion) = 0;

    virtual EGamePlatformArenaClientFlowState GetObservedState() const = 0;
};
