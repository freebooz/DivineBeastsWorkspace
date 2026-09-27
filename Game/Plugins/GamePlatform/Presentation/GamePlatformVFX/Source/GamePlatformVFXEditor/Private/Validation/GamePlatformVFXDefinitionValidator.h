#pragma once

#include "Validation/GamePlatformVFXValidationRules.h"

class UGamePlatformVFXDefinition;

class FGamePlatformVFXDefinitionValidator
{
public:
    static void Validate(
        const UGamePlatformVFXDefinition& Definition,
        TArray<FGamePlatformVFXValidationIssue>& OutIssues);
};
