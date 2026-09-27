#pragma once

#include "Validation/GamePlatformVFXValidationRules.h"

class UGamePlatformVFXCatalog;

class FGamePlatformVFXCatalogValidator
{
public:
    static void Validate(
        const UGamePlatformVFXCatalog& Catalog,
        TArray<FGamePlatformVFXValidationIssue>& OutIssues);
};
