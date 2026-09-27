#include "Links/GamePlatformNavigationSmartLinkProxy.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"

AGamePlatformNavigationSmartLinkProxy::
AGamePlatformNavigationSmartLinkProxy(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bSmartLinkIsRelevant = true;
}

void AGamePlatformNavigationSmartLinkProxy::SetPlatformLinkEnabled(
    bool bEnabled)
{
    if (HasAuthority())
    {
        SetSmartLinkEnabled(bEnabled);
    }
}

void AGamePlatformNavigationSmartLinkProxy::NotifyTraversalRequested(
    AActor* Agent,
    const FVector& Destination)
{
    if (!HasAuthority() || !IsValid(Agent))
    {
        return;
    }

    FGamePlatformNavigationTraversalRequest Request;
    Request.NavigationLinkId = NavigationLinkId;
    Request.TraversalType = TraversalType;
    Request.Agent = Agent;
    Request.Destination = Destination;
    OnTraversalRequested.Broadcast(Request);
}

void AGamePlatformNavigationSmartLinkProxy::CompleteTraversal(
    AActor* Agent,
    bool bSuccess)
{
    if (!HasAuthority() || !IsValid(Agent))
    {
        return;
    }

    if (bSuccess)
    {
        ResumePathFollowing(Agent);
        return;
    }

    SetSmartLinkEnabled(false);

    if (const APawn* Pawn = Cast<APawn>(Agent))
    {
        if (AAIController* Controller =
            Cast<AAIController>(Pawn->GetController()))
        {
            Controller->StopMovement();
        }
    }
}
