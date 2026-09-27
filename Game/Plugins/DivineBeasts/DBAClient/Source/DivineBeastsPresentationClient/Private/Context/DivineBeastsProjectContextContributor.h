#pragma once

#include "CoreMinimal.h"
#include "GamePlatformPresentationContext.h"

class UDivineBeastsPresentationClientSubsystem;

/** FDivineBeastsProjectContextContributor（项目表现上下文贡献者）。 */
class FDivineBeastsProjectContextContributor final
    : public IGamePlatformPresentationContextContributor
{
public:
    explicit FDivineBeastsProjectContextContributor(
        UDivineBeastsPresentationClientSubsystem* InOwner)
        : Owner(InOwner)
    {
    }

    virtual FName GetContributorId() const override
    {
        return TEXT("DivineBeasts.ProjectPresentation.Context");
    }

    virtual int32 GetPriority() const override { return 100; }

    virtual EGamePlatformPresentationContextScope GetScope() const override
    {
        return EGamePlatformPresentationContextScope::LocalPlayer;
    }

    virtual EGamePlatformPresentationConflictPolicy GetConflictPolicy() const override
    {
        return EGamePlatformPresentationConflictPolicy::RejectConflict;
    }

    virtual FGamePlatformPresentationContextPatch BuildPatch() const override;

private:
    TWeakObjectPtr<UDivineBeastsPresentationClientSubsystem> Owner;
};
