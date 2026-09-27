#pragma once

#include "Navigation/NavLinkProxy.h"
#include "Types/GamePlatformNavigationTypes.h"
#include "GamePlatformNavigationSmartLinkProxy.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGamePlatformTraversalRequested,
    const FGamePlatformNavigationTraversalRequest&, Request);

UCLASS()
class GAMEPLATFORMNAVIGATIONSERVER_API AGamePlatformNavigationSmartLinkProxy
    : public ANavLinkProxy
{
    GENERATED_BODY()

public:
    AGamePlatformNavigationSmartLinkProxy(
        const FObjectInitializer& ObjectInitializer);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Navigation")
    void SetPlatformLinkEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Navigation")
    void NotifyTraversalRequested(
        AActor* Agent,
        const FVector& Destination);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Navigation")
    void CompleteTraversal(AActor* Agent, bool bSuccess);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
    FName NavigationLinkId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Navigation")
    FName TraversalType = NAME_None;

    UPROPERTY(BlueprintAssignable, Category="Navigation")
    FGamePlatformTraversalRequested OnTraversalRequested;
};
