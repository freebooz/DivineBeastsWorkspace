#include "Resolution/GamePlatformVFXResolver.h"

FGamePlatformVFXResolvedDefinition FGamePlatformVFXResolver::Resolve(const FGamePlatformVFXRequest& Request) const
{
    return Registry.Resolve(Request);
}
