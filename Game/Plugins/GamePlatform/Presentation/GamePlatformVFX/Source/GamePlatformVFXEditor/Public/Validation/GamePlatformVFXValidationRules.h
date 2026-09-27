#pragma once

#include "CoreMinimal.h"

enum class EGamePlatformVFXValidationSeverity : uint8
{
    Info,
    Warning,
    Error
};

struct GAMEPLATFORMVFXEDITOR_API FGamePlatformVFXValidationIssue
{
    EGamePlatformVFXValidationSeverity Severity = EGamePlatformVFXValidationSeverity::Error;
    FName RuleId = NAME_None;
    FText Message;
};
