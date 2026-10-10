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

    if (!Input.EventId.IsValid() || !FMath::IsFinite(Input.Magnitude) ||
        Input.WorldLocation.ContainsNaN())
    {
        return false; // 非有限值不能传给UI数值格式化或屏幕世界坐标投影。
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

    // 本层只负责显示；极端非法/过大数值不能在int32文本转换时溢出变成负数。
    OutRequest.NumericValue =
        FMath::Clamp(Input.Magnitude, 0.0, 1000000000.0);
    OutRequest.bHasNumericValue = true;
    OutRequest.bAllowMerge =
        !Input.TargetVisualKey.IsNone();

    // 同一目标的护盾吸收、生命伤害和治疗必须使用独立合并键，不能互相吞掉。
    const TCHAR* MergePrefix =
        Input.Kind == EDivineBeastsCombatFeedbackKind::Healing
            ? TEXT("Healing")
            : (Input.Kind == EDivineBeastsCombatFeedbackKind::ShieldDamage
                ? TEXT("ShieldDamage")
                : TEXT("Damage"));
    OutRequest.MergeKey = BuildMergeKey(
        Input.TargetVisualKey, MergePrefix);
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
