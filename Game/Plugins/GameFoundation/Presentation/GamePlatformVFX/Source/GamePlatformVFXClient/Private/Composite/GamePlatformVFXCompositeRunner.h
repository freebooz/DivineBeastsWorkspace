#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformVFXHandle.h"

class UGamePlatformVFXCompositeDefinition;
class UGamePlatformVFXWorldSubsystem;
struct FGamePlatformVFXRequest;

class FGamePlatformVFXCompositeRunner
{
public:
    static void Start(
        UGamePlatformVFXWorldSubsystem& Owner,
        const FGamePlatformVFXHandle& ParentHandle,
        const UGamePlatformVFXCompositeDefinition& Definition,
        const FGamePlatformVFXRequest& ParentRequest);
};
