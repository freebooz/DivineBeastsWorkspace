#include "Adapters/Combat/DivineBeastsCombatUIFeedbackLibrary.h"

#include "Engine/LocalPlayer.h"
#include "Feedback/GamePlatformFeedbackWidget.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"

namespace
{
FName BuildMergeKey(
    FName TargetVisualKey,
    const TCHAR* Prefix)
{
    if (TargetVisualKey.IsNone())
    {
        return NAME_None;
    }

    return FName(
        *FString::Printf(
            TEXT("%s_%s"),
            Prefix,
            *TargetVisualKey.ToString()));
}
}

bool UDivineBeastsCombatUIFeedbackLibrary::BuildFloatingTextRequest(
    const FDivineBeastsCombatFeedbackInput& Input,
    FGamePlatformUIFeedbackRequest& OutRequest)
{
    OutRequest = FGamePlatformUIFeedbackRequest();

    if (!Input.EventId.IsValid())
    {
        return false;
    }

    OutRequest.OccurrenceId = Input.EventId;
    OutRequest.Channel = TEXT("FloatingText");
    OutRequest.Priority = 10;
    OutRequest.LifetimeSeconds = 0.9f;
    OutRequest.bUseWorldLocation = true;
    OutRequest.WorldLocation = Input.WorldLocation;

    switch (Input.Kind)
    {
    case EDivineBeastsCombatFeedbackKind::Damage:
        OutRequest.StyleId = TEXT("DBA.UI.Feedback.Damage");
        break;

    case EDivineBeastsCombatFeedbackKind::ShieldDamage:
        OutRequest.StyleId = TEXT("DBA.UI.Feedback.ShieldDamage");
        break;

    case EDivineBeastsCombatFeedbackKind::Healing:
        OutRequest.StyleId = TEXT("DBA.UI.Feedback.Healing");
        break;

    case EDivineBeastsCombatFeedbackKind::Death:
        OutRequest.StyleId = TEXT("DBA.UI.Feedback.Death");
        OutRequest.Priority = 100;
        OutRequest.LifetimeSeconds = 1.2f;
        OutRequest.Text = FText::FromString(TEXT("击败"));
        return true;

    default:
        return false;
    }

    OutRequest.NumericValue =
        FMath::Max(0.0, Input.Magnitude);
    OutRequest.bHasNumericValue = true;
    OutRequest.bAllowMerge =
        !Input.TargetVisualKey.IsNone();
    OutRequest.MergeKey = BuildMergeKey(
        Input.TargetVisualKey,
        Input.Kind == EDivineBeastsCombatFeedbackKind::Healing
            ? TEXT("Healing")
            : TEXT("Damage"));
    OutRequest.Text = FText::AsNumber(
        FMath::RoundToInt(OutRequest.NumericValue));
    return OutRequest.NumericValue > KINDA_SMALL_NUMBER;
}

FGuid UDivineBeastsCombatUIFeedbackLibrary::SubmitFloatingText(
    ULocalPlayer* LocalPlayer,
    const FDivineBeastsCombatFeedbackInput& Input,
    TSubclassOf<UGamePlatformFeedbackWidget> WidgetClass)
{
    if (!IsValid(LocalPlayer) ||
        !WidgetClass ||
        WidgetClass->HasAnyClassFlags(CLASS_Abstract))
    {
        return FGuid();
    }

    FGamePlatformUIFeedbackRequest Request;
    if (!BuildFloatingTextRequest(Input, Request))
    {
        return FGuid();
    }

    UGamePlatformUIManagerSubsystem* UIManager =
        LocalPlayer->GetSubsystem<UGamePlatformUIManagerSubsystem>();
    return IsValid(UIManager)
        ? UIManager->SubmitFeedback(
            MoveTemp(Request),
            WidgetClass)
        : FGuid();
}
