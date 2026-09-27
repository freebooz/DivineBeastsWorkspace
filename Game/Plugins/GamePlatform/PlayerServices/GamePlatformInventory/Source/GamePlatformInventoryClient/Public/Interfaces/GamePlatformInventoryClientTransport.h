#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformInventoryTypes.h"

using FGamePlatformInventorySnapshotCompletion =
    TFunction<void(
        FGamePlatformInventorySnapshot,
        EGamePlatformInventoryError)>;

using FGamePlatformInventoryMutationCompletion =
    TFunction<void(
        FGamePlatformInventoryMutationResult,
        EGamePlatformInventoryError)>;

class GAMEPLATFORMINVENTORYCLIENT_API IGamePlatformInventoryClientTransport
{
public:
    virtual ~IGamePlatformInventoryClientTransport() = default;

    virtual void CancelAllRequests() = 0;

    virtual bool BeginGetSnapshot(
        FGamePlatformInventorySnapshotCompletion Completion) = 0;

    virtual bool BeginGetOperation(
        const FGuid& OperationId,
        FGamePlatformInventoryMutationCompletion Completion) = 0;

    virtual bool BeginMove(
        const FGamePlatformInventoryMoveRequest& Request,
        FGamePlatformInventoryMutationCompletion Completion) = 0;

    virtual bool BeginSplit(
        const FGamePlatformInventorySplitRequest& Request,
        FGamePlatformInventoryMutationCompletion Completion) = 0;

    virtual bool BeginMerge(
        const FGamePlatformInventoryMergeRequest& Request,
        FGamePlatformInventoryMutationCompletion Completion) = 0;

    virtual bool BeginSetQuickbar(
        const FGamePlatformInventoryQuickbarRequest& Request,
        FGamePlatformInventoryMutationCompletion Completion) = 0;

    virtual bool BeginClearQuickbar(
        const FGamePlatformInventoryQuickbarRequest& Request,
        FGamePlatformInventoryMutationCompletion Completion) = 0;
};
