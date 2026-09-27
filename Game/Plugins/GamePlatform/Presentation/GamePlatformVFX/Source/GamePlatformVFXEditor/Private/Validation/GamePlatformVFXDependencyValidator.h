#pragma once

#include "Validation/GamePlatformVFXValidationRules.h"

class FGamePlatformVFXDependencyValidator
{
public:
    static void ValidatePlatformOwnedPath(
        const FSoftObjectPath& Path,
        TArray<FGamePlatformVFXValidationIssue>& OutIssues);
};
