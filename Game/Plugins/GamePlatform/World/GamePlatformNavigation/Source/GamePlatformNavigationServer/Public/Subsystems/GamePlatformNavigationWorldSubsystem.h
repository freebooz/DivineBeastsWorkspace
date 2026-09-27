#pragma once

#include "CoreMinimal.h"
#include "Interfaces/GamePlatformNavigationService.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavigationData.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "GamePlatformNavigationWorldSubsystem.generated.h"

class ANavigationData;
class UNavigationQueryFilter;
class UGamePlatformNavigationAgentProfile;

UCLASS()
class GAMEPLATFORMNAVIGATIONSERVER_API UGamePlatformNavigationWorldSubsystem final
    : public UWorldSubsystem
    , public IGamePlatformNavigationService
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual int32 GetWorldGeneration() const override
    {
        return WorldGeneration;
    }

    virtual FGamePlatformNavigationProjectionResult ProjectPoint(
        const FVector& Position,
        const FVector& Extent,
        FName AgentProfileId,
        FName FilterId) override;

    virtual bool TestPath(
        const FGamePlatformNavigationRequest& Request,
        EGamePlatformNavigationError& OutError) override;

    virtual FGamePlatformNavigationPathResult FindPath(
        const FGamePlatformNavigationRequest& Request) override;

    virtual FGamePlatformNavigationRequestHandle FindPathAsync(
        const FGamePlatformNavigationRequest& Request,
        FGamePlatformNavigationQueryCompleted Completion) override;

    virtual bool CancelRequest(
        const FGamePlatformNavigationRequestHandle& Handle) override;

    virtual bool FindRandomReachablePoint(
        const FVector& Origin,
        float Radius,
        FName AgentProfileId,
        FName FilterId,
        FVector& OutLocation,
        EGamePlatformNavigationError& OutError) override;

    virtual bool RegisterInvoker(
        AActor& Actor,
        FName AgentProfileId,
        EGamePlatformNavigationError& OutError) override;

    virtual void UnregisterInvoker(AActor& Actor) override;

    virtual FGamePlatformNavigationDiagnostics GetDiagnostics() const override;

    UFUNCTION(BlueprintCallable, Category="Navigation")
    bool RegisterAgentProfile(
        UGamePlatformNavigationAgentProfile* Profile);

    bool RegisterFilter(
        FName FilterId,
        TSubclassOf<UNavigationQueryFilter> FilterClass);

    /** 校验导航Profile不会比真实Character胶囊/NavAgent更“瘦”，避免路径安全假阳性。 */
    bool ValidateAgentProfileForActor(
        const AActor& Actor,
        FName AgentProfileId,
        FText& OutReason) const;

private:
    struct FAsyncRequestRecord
    {
        uint32 EngineQueryId = 0;
        int32 WorldGeneration = 0;
        TWeakObjectPtr<UObject> Owner;
        bool bHadOwner = false;
        FGamePlatformNavigationRequest Request;
        FGamePlatformNavigationQueryCompleted Completion;
        FTimerHandle TimeoutHandle;
    };

    int32 WorldGeneration = 0;
    int64 SyncQueryCount = 0;
    int64 AsyncQueryCount = 0;
    int64 CancelledRequestCount = 0;

    TMap<FName, TWeakObjectPtr<UGamePlatformNavigationAgentProfile>> AgentProfiles;
    TMap<FName, TSubclassOf<UNavigationQueryFilter>> Filters;
    TMap<FGuid, FAsyncRequestRecord> AsyncRequests;
    TSet<TWeakObjectPtr<AActor>> RegisteredInvokers;

    bool ResolveQueryContext(
        FName AgentProfileId,
        FName FilterId,
        const FVector& Location,
        FNavAgentProperties& OutAgent,
        ANavigationData*& OutNavData,
        FSharedConstNavQueryFilter& OutFilter,
        EGamePlatformNavigationError& OutError) const;

    bool BuildPathQuery(
        const FGamePlatformNavigationRequest& Request,
        FGuid EffectiveRequestId,
        FNavAgentProperties& OutAgent,
        FPathFindingQuery& OutQuery,
        FVector& OutResolvedStart,
        FVector& OutResolvedGoal,
        EGamePlatformNavigationError& OutError) const;

    FGamePlatformNavigationPathResult BuildPathResult(
        const FGamePlatformNavigationRequest& Request,
        FGuid EffectiveRequestId,
        int32 ExpectedWorldGeneration,
        ENavigationQueryResult::Type QueryResult,
        const FNavPathSharedPtr& Path,
        const FVector& ResolvedStart,
        const FVector& ResolvedGoal) const;

    void HandleAsyncPathCompleted(
        uint32 QueryId,
        ENavigationQueryResult::Type QueryResult,
        FNavPathSharedPtr Path,
        FGuid RequestId,
        int32 ExpectedWorldGeneration,
        FVector ResolvedStart,
        FVector ResolvedGoal);

    void HandleAsyncTimeout(
        FGuid RequestId,
        int32 ExpectedWorldGeneration);

    UFUNCTION()
    void HandleInvokerDestroyed(AActor* DestroyedActor);
};
