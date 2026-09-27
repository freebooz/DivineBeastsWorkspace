#include "Validation/GamePlatformVFXDependencyValidator.h"

void FGamePlatformVFXDependencyValidator::ValidatePlatformOwnedPath(
    const FSoftObjectPath& Path,
    TArray<FGamePlatformVFXValidationIssue>& OutIssues)
{
    if (!Path.IsValid())
    {
        return;
    }

    const FString Value = Path.ToString();
    const bool bForbiddenUpperLayer =
        Value.Contains(TEXT("/DivineBeasts/"), ESearchCase::IgnoreCase) ||
        Value.Contains(TEXT("/DBA"), ESearchCase::IgnoreCase) ||
        Value.Contains(TEXT("/Moba"), ESearchCase::IgnoreCase);

    if (bForbiddenUpperLayer)
    {
        FGamePlatformVFXValidationIssue& Issue = OutIssues.AddDefaulted_GetRef();
        Issue.RuleId = TEXT("GPVFX.Dependency.UpperLayer");
        Issue.Severity = EGamePlatformVFXValidationSeverity::Error;
        Issue.Message = FText::Format(
            NSLOCTEXT("GamePlatformVFX", "UpperLayerDependency", "GamePlatformVFX 基础层资产禁止引用上层资源：{0}"),
            FText::FromString(Value));
    }
}
