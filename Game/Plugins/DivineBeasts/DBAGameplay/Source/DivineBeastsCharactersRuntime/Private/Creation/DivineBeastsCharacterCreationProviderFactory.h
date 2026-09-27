#pragma once

#include "CoreMinimal.h"

class IDivineBeastsCharacterCreationProvider;

TUniquePtr<IDivineBeastsCharacterCreationProvider>
CreateDivineBeastsCharacterCreationProvider();
