#pragma once

#include "CoreMinimal.h"

namespace GamePlatformVFXValidation
{
    inline const FName MissingStableId(TEXT("GPVFX.MissingStableId"));
    inline const FName MissingNiagaraSystem(TEXT("GPVFX.MissingNiagaraSystem"));
    inline const FName InvalidCatalogEntry(TEXT("GPVFX.InvalidCatalogEntry"));
    inline const FName AmbiguousCatalogEntry(TEXT("GPVFX.AmbiguousCatalogEntry"));
    inline const FName CompositeCycle(TEXT("GPVFX.CompositeCycle"));
}
