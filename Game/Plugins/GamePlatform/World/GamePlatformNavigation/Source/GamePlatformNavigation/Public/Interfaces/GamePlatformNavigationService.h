#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformNavigationTypes.h"

class AActor;
class UGamePlatformNavigationAgentProfile;

DECLARE_DELEGATE_OneParam(
    FGamePlatformNavigationQueryCompleted,
    const FGamePlatformNavigationPathResult&);

class IGamePlatformNavigationService
{
public:
    virtual ~IGamePlatformNavigationService() = default;

    virtual int32 GetWorldGeneration() const = 0;

    virtual FGamePlatformNavigationProjectionResult ProjectPoint(
        const FVector& Position,
        const FVector& Extent,
        FName AgentProfileId,
        FName FilterId) = 0;

    virtual bool TestPath(
        const FGamePlatformNavigationRequest& Request,
        EGamePlatformNavigationError& OutError) = 0;

    virtual FGamePlatformNavigationPathResult FindPath(
        const FGamePlatformNavigationRequest& Request) = 0;

    virtual FGamePlatformNavigationRequestHandle FindPathAsync(
        const FGamePlatformNavigationRequest& Request,
        FGamePlatformNavigationQueryCompleted Completion) = 0;

    virtual bool CancelRequest(
        const FGamePlatformNavigationRequestHandle& Handle) = 0;

    virtual bool FindRandomReachablePoint(
        const FVector& Origin,
        float Radius,
        FName AgentProfileId,
        FName FilterId,
        FVector& OutLocation,
        EGamePlatformNavigationError& OutError) = 0;

    virtual bool RegisterInvoker(
        AActor& Actor,
        FName AgentProfileId,
        EGamePlatformNavigationError& OutError) = 0;

    virtual void UnregisterInvoker(AActor& Actor) = 0;

    virtual FGamePlatformNavigationDiagnostics GetDiagnostics() const = 0;
};
