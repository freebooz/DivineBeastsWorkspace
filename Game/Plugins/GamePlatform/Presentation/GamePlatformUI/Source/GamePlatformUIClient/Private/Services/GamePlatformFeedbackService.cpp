#include "Services/GamePlatformFeedbackService.h"

#include "Engine/LocalPlayer.h"
#include "Feedback/GamePlatformFeedbackWidget.h"
#include "GameFramework/PlayerController.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "TimerManager.h"

namespace
{
constexpr int32 MaxActiveFeedback = 48;
constexpr int32 MaxPooledFeedback = 64;
}

void UGamePlatformFeedbackService::Initialize(ULocalPlayer* InLocalPlayer)
{
    LocalPlayer = InLocalPlayer;
    ActiveFeedback.Reserve(16);
    ActiveByMergeKey.Reserve(16);
    PooledWidgets.Reserve(24);
}

void UGamePlatformFeedbackService::SetRootLayout(
    UGamePlatformUILayerStack* InRootLayout)
{
    RootLayout = InRootLayout;
}

FGuid UGamePlatformFeedbackService::SubmitFeedback(
    FGamePlatformUIFeedbackRequest Request,
    TSubclassOf<UGamePlatformFeedbackWidget> WidgetClass)
{
    if (!Request.IsValid() ||
        !WidgetClass ||
        WidgetClass->HasAnyClassFlags(CLASS_Abstract) ||
        !IsValid(RootLayout) ||
        !IsValid(LocalPlayer))
    {
        return FGuid();
    }

    if (!Request.OccurrenceId.IsValid())
    {
        Request.OccurrenceId = FGuid::NewGuid();
    }

    if (Request.bAllowMerge && !Request.MergeKey.IsNone())
    {
        if (const FGuid* ExistingId = ActiveByMergeKey.Find(Request.MergeKey))
        {
            if (FActiveFeedback* Existing = ActiveFeedback.Find(*ExistingId))
            {
                if (UGamePlatformFeedbackWidget* ExistingWidget =
                    Existing->Widget.Get())
                {
                    if (ExistingWidget->MergeFeedbackRequest(Request))
                    {
                        RestartLifetime(
                            *ExistingId,
                            Request.LifetimeSeconds);
                        return *ExistingId;
                    }
                }
            }
        }
    }

    if (ActiveFeedback.Num() >= MaxActiveFeedback ||
        !ResolveInitialScreenPosition(Request))
    {
        return FGuid();
    }

    UGamePlatformFeedbackWidget* Widget = AcquireWidget(WidgetClass);
    if (!IsValid(Widget))
    {
        return FGuid();
    }

    Widget->ApplyFeedbackRequest(Request);
    if (!RootLayout->AddFeedbackWidget(Widget))
    {
        RecycleWidget(Widget);
        return FGuid();
    }

    FActiveFeedback Entry;
    Entry.Widget = Widget;
    Entry.MergeKey = Request.MergeKey;
    ActiveFeedback.Add(Request.OccurrenceId, Entry);

    if (Request.bAllowMerge && !Request.MergeKey.IsNone())
    {
        ActiveByMergeKey.Add(
            Request.MergeKey,
            Request.OccurrenceId);
    }

    RestartLifetime(
        Request.OccurrenceId,
        Request.LifetimeSeconds);
    return Request.OccurrenceId;
}

bool UGamePlatformFeedbackService::DismissFeedback(FGuid OccurrenceId)
{
    FActiveFeedback* Entry = ActiveFeedback.Find(OccurrenceId);
    if (!Entry)
    {
        return false;
    }

    if (UWorld* World = GetServiceWorld())
    {
        World->GetTimerManager().ClearTimer(Entry->TimerHandle);
    }

    UGamePlatformFeedbackWidget* Widget = Entry->Widget.Get();
    const FName MergeKey = Entry->MergeKey;

    if (!MergeKey.IsNone())
    {
        if (const FGuid* Current = ActiveByMergeKey.Find(MergeKey);
            Current && *Current == OccurrenceId)
        {
            ActiveByMergeKey.Remove(MergeKey);
        }
    }

    ActiveFeedback.Remove(OccurrenceId);
    RecycleWidget(Widget);
    return true;
}

UGamePlatformFeedbackWidget* UGamePlatformFeedbackService::AcquireWidget(
    TSubclassOf<UGamePlatformFeedbackWidget> WidgetClass)
{
    for (int32 Index = PooledWidgets.Num() - 1; Index >= 0; --Index)
    {
        UGamePlatformFeedbackWidget* Candidate =
            PooledWidgets[Index];

        if (!IsValid(Candidate))
        {
            PooledWidgets.RemoveAtSwap(Index, 1, EAllowShrinking::No);
            continue;
        }

        if (Candidate->GetClass() == WidgetClass)
        {
            PooledWidgets.RemoveAtSwap(Index, 1, EAllowShrinking::No);
            return Candidate;
        }
    }

    APlayerController* PlayerController =
        IsValid(LocalPlayer)
            ? LocalPlayer->GetPlayerController(GetServiceWorld())
            : nullptr;
    return IsValid(PlayerController)
        ? CreateWidget<UGamePlatformFeedbackWidget>(
            PlayerController,
            WidgetClass)
        : nullptr;
}

void UGamePlatformFeedbackService::RecycleWidget(
    UGamePlatformFeedbackWidget* Widget)
{
    if (!IsValid(Widget))
    {
        return;
    }

    Widget->RemoveFromParent();
    Widget->ResetFeedbackState();

    if (PooledWidgets.Num() < MaxPooledFeedback)
    {
        PooledWidgets.Add(Widget);
    }
}

void UGamePlatformFeedbackService::RestartLifetime(
    FGuid OccurrenceId,
    float LifetimeSeconds)
{
    FActiveFeedback* Entry = ActiveFeedback.Find(OccurrenceId);
    UWorld* World = GetServiceWorld();
    if (!Entry || !World)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(Entry->TimerHandle);
    const TWeakObjectPtr<UGamePlatformFeedbackService> WeakThis(this);
    World->GetTimerManager().SetTimer(
        Entry->TimerHandle,
        FTimerDelegate::CreateLambda([WeakThis, OccurrenceId]()
        {
            if (UGamePlatformFeedbackService* Self = WeakThis.Get())
            {
                Self->DismissFeedback(OccurrenceId);
            }
        }),
        FMath::Max(0.05f, LifetimeSeconds),
        false);
}

bool UGamePlatformFeedbackService::ResolveInitialScreenPosition(
    FGamePlatformUIFeedbackRequest& Request) const
{
    if (!Request.bUseWorldLocation)
    {
        return true;
    }

    APlayerController* PlayerController =
        IsValid(LocalPlayer)
            ? LocalPlayer->GetPlayerController(GetServiceWorld())
            : nullptr;
    return IsValid(PlayerController) &&
        PlayerController->ProjectWorldLocationToScreen(
            Request.WorldLocation,
            Request.ScreenPosition,
            true);
}

void UGamePlatformFeedbackService::Clear()
{
    TArray<FGuid> OccurrenceIds;
    ActiveFeedback.GetKeys(OccurrenceIds);
    for (const FGuid& OccurrenceId : OccurrenceIds)
    {
        DismissFeedback(OccurrenceId);
    }

    ActiveFeedback.Reset();
    ActiveByMergeKey.Reset();

    for (UGamePlatformFeedbackWidget* Widget : PooledWidgets)
    {
        if (IsValid(Widget))
        {
            Widget->RemoveFromParent();
            Widget->ResetFeedbackState();
        }
    }
    PooledWidgets.Reset();
}

UWorld* UGamePlatformFeedbackService::GetServiceWorld() const
{
    return IsValid(LocalPlayer) ? LocalPlayer->GetWorld() : nullptr;
}
