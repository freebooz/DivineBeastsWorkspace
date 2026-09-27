#pragma once

#include "GamePlatformPresentationTypes.h"

class UWorld;

/** 中立GamePlatformPresentation请求到VFX Service的正式适配器。 */
class FGamePlatformVFXPresentationProvider
{
public:
    explicit FGamePlatformVFXPresentationProvider(UWorld* InWorld)
        : World(InWorld)
    {
    }

    bool Handle(const FGamePlatformPresentationRequest& Request) const;

private:
    TWeakObjectPtr<UWorld> World;
};
