#pragma once

#include "Validation/GamePlatformVFXValidationRules.h"

class UGamePlatformVFXCompositeDefinition;

class FGamePlatformVFXCompositeValidator
{
public:
    static void Validate(
        const UGamePlatformVFXCompositeDefinition& Definition,
        TArray<FGamePlatformVFXValidationIssue>& OutIssues);
};
