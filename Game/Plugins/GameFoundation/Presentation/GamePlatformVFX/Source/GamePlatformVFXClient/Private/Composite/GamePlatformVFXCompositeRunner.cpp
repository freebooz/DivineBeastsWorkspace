#include "Composite/GamePlatformVFXCompositeRunner.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "Subsystems/GamePlatformVFXWorldSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"

void FGamePlatformVFXCompositeRunner::Start(
    UGamePlatformVFXWorldSubsystem& Owner,
    const FGamePlatformVFXHandle& ParentHandle,
    const UGamePlatformVFXCompositeDefinition& Definition,
    const FGamePlatformVFXRequest& ParentRequest)
{
    UWorld* World = Owner.GetWorld();
    if (!World)
    {
        return;
    }

    float MaxDelay = 0.0f;
    for (const FGamePlatformVFXCompositeChild& Child : Definition.Children)
    {
        if (!Child.DefinitionId.IsValid())
        {
            continue;
        }

        MaxDelay = FMath::Max(MaxDelay, Child.DelaySeconds);
        FTimerDelegate Delegate;
        Delegate.BindLambda([WeakOwner = TWeakObjectPtr<UGamePlatformVFXWorldSubsystem>(&Owner), ParentHandle, ParentRequest, Child]()
        {
            if (UGamePlatformVFXWorldSubsystem* StrongOwner = WeakOwner.Get())
            {
                if (StrongOwner->GetState(ParentHandle) != EGamePlatformVFXLifecycleState::Active)
                {
                    return;
                }

                FGamePlatformVFXRequest ChildRequest = ParentRequest;
                ChildRequest.SemanticTag = FGameplayTag();
                ChildRequest.ExplicitDefinitionId = Child.DefinitionId;
                const FGamePlatformVFXPlayResult ChildResult = StrongOwner->Play(ChildRequest);
                if (ChildResult.IsAccepted())
                {
                    StrongOwner->AttachChildHandle(ParentHandle, ChildResult.Handle);
                }
            }
        });

        if (Child.DelaySeconds <= 0.0f)
        {
            Delegate.ExecuteIfBound();
        }
        else
        {
            FTimerHandle TimerHandle;
            World->GetTimerManager().SetTimer(TimerHandle, Delegate, Child.DelaySeconds, false);
            Owner.AttachCompositeTimer(ParentHandle, TimerHandle);
        }
    }

    FTimerHandle FinishTimer;
    const float FinishDelay = FMath::Max(0.01f, MaxDelay + Definition.TailSeconds);
    World->GetTimerManager().SetTimer(FinishTimer, FTimerDelegate::CreateLambda([
        WeakOwner = TWeakObjectPtr<UGamePlatformVFXWorldSubsystem>(&Owner), ParentHandle]()
    {
        if (UGamePlatformVFXWorldSubsystem* StrongOwner = WeakOwner.Get())
        {
            StrongOwner->CompleteComposite(ParentHandle);
        }
    }), FinishDelay, false);
    Owner.AttachCompositeTimer(ParentHandle, FinishTimer);
}
