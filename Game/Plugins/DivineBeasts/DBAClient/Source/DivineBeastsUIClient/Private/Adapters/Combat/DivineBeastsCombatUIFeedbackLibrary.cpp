#include "Adapters/Combat/DivineBeastsCombatUIFeedbackLibrary.h"

#include "GameFramework/Actor.h"

namespace
{
FName BuildMergeKey(
    const FGamePlatformCombatEvent& Event,
    const TCHAR* Prefix)
{
    const int32 TargetUniqueId =
        IsValid(Event.TargetActor)
            ? Event.TargetActor->GetUniqueID()
            : 0;

    return FName(
        *FString::Printf(
            TEXT("%s_%d"),
            Prefix,
            TargetUniqueId));
}
}

bool UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
    const FGamePlatformCombatEvent& Event,
    FGamePlatformUIFeedbackRequest& OutRequest)
{
    OutRequest = FGamePlatformUIFeedbackRequest();

    if (!Event.EventId.IsValid())
    {
        return false;
    }

    OutRequest.OccurrenceId = Event.EventId;
    OutRequest.Channel = TEXT("FloatingText");
    OutRequest.Priority = 10;
    OutRequest.LifetimeSeconds = 0.9f;
    OutRequest.bUseWorldLocation = true;
    OutRequest.WorldLocation =
        !Event.ImpactPoint.IsNearlyZero()
            ? Event.ImpactPoint
            : (IsValid(Event.TargetActor)
                ? Event.TargetActor->GetActorLocation()
                : FVector::ZeroVector);

    switch (Event.EventType)
    {
    case EGamePlatformCombatEventType::Damage:
        OutRequest.StyleId =
            Event.AppliedToShield > 0.0f &&
            Event.AppliedToHealth <= KINDA_SMALL_NUMBER
                ? TEXT("DBA.UI.Feedback.ShieldDamage")
                : TEXT("DBA.UI.Feedback.Damage");
        OutRequest.NumericValue =
            FMath::Max(0.0f, Event.AppliedMagnitude);
        OutRequest.bHasNumericValue = true;
        OutRequest.bAllowMerge = true;
        OutRequest.MergeKey = BuildMergeKey(Event, TEXT("Damage"));
        OutRequest.Text = FText::AsNumber(
            FMath::RoundToInt(OutRequest.NumericValue));
        return OutRequest.NumericValue > KINDA_SMALL_NUMBER;

    case EGamePlatformCombatEventType::Healing:
        OutRequest.StyleId = TEXT("DBA.UI.Feedback.Healing");
        OutRequest.NumericValue =
            FMath::Max(0.0f, Event.AppliedMagnitude);
        OutRequest.bHasNumericValue = true;
        OutRequest.bAllowMerge = true;
        OutRequest.MergeKey = BuildMergeKey(Event, TEXT("Healing"));
        OutRequest.Text = FText::AsNumber(
            FMath::RoundToInt(OutRequest.NumericValue));
        return OutRequest.NumericValue > KINDA_SMALL_NUMBER;

    case EGamePlatformCombatEventType::Death:
        OutRequest.StyleId = TEXT("DBA.UI.Feedback.Death");
        OutRequest.Priority = 100;
        OutRequest.LifetimeSeconds = 1.2f;
        OutRequest.Text = FText::FromString(TEXT("击败"));
        return true;

    case EGamePlatformCombatEventType::RespawnReset:
    case EGamePlatformCombatEventType::ControlApplied:
    case EGamePlatformCombatEventType::ControlRemoved:
    default:
        // 控制状态和复活有独立HUD/状态图标，不强制转成浮动文字。
        return false;
    }
}
