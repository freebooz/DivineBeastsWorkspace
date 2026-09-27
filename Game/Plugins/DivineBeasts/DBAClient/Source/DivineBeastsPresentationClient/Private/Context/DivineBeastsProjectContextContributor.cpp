#include "Context/DivineBeastsProjectContextContributor.h"

#include "DivineBeastsPresentationClientSubsystem.h"

FGamePlatformPresentationContextPatch
FDivineBeastsProjectContextContributor::BuildPatch() const
{
    return Owner.IsValid()
        ? Owner->BuildProjectContextPatch()
        : FGamePlatformPresentationContextPatch();
}
