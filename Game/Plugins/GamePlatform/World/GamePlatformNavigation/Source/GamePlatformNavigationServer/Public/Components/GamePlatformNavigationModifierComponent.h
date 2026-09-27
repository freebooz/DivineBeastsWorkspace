#pragma once

#include "NavModifierComponent.h"
#include "Types/GamePlatformNavigationTypes.h"
#include "GamePlatformNavigationModifierComponent.generated.h"

UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMNAVIGATIONSERVER_API UGamePlatformNavigationModifierComponent
    : public UNavModifierComponent
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Navigation")
    void SetPlatformArea(EGamePlatformNavigationAreaKind AreaKind);
};
