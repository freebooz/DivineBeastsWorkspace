#pragma once

#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "Types/GamePlatformVFXHandle.h"
#include "Types/GamePlatformVFXRequest.h"

class UWorld;

/** Composite Definition 的轻量时序编排器。 */
class FGamePlatformVFXCompositeRunner
{
public:
    using FPlayChild = TFunction<void(
        const TSoftObjectPtr<UGamePlatformVFXDefinition>&,
        const FGamePlatformVFXRequest&,
        const FGamePlatformVFXHandle&)>;

    static void Run(
        UWorld& World,
        const UGamePlatformVFXCompositeDefinition& Definition,
        const FGamePlatformVFXRequest& Request,
        const FGamePlatformVFXHandle& ParentHandle,
        FPlayChild PlayChild);
};
