#pragma once

#include "NavAreas/NavArea.h"
#include "NavAreas/NavArea_Null.h"
#include "GamePlatformNavigationAreas.generated.h"

UCLASS()
class GAMEPLATFORMNAVIGATIONSERVER_API UGamePlatformNavArea_Default
    : public UNavArea
{
    GENERATED_BODY()
public:
    UGamePlatformNavArea_Default();
};

UCLASS()
class GAMEPLATFORMNAVIGATIONSERVER_API UGamePlatformNavArea_HighCost
    : public UNavArea
{
    GENERATED_BODY()
public:
    UGamePlatformNavArea_HighCost();
};

UCLASS()
class GAMEPLATFORMNAVIGATIONSERVER_API UGamePlatformNavArea_Blocked
    : public UNavArea_Null
{
    GENERATED_BODY()
};
