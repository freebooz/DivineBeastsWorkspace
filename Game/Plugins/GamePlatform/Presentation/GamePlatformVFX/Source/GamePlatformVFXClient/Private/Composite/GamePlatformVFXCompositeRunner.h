#pragma once

#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "TimerManager.h"
#include "Types/GamePlatformVFXHandle.h"
#include "Types/GamePlatformVFXRequest.h"

class UWorld;

/** Composite Definition 的轻量时序编排器。 */
class FGamePlatformVFXCompositeRunner
{
public:
    using FPlayChild = TFunction<void(
        FName,
        const FGamePlatformVFXRequest&,
        const FGamePlatformVFXHandle&)>;

    /** 由WorldSubsystem登记延迟步骤Timer，使父Handle取消时可以立即清除。 */
    using FRegisterTimer = TFunction<void(
        const FGamePlatformVFXHandle&,
        const FTimerHandle&)>;

    static void Run(
        UWorld& World,
        const UGamePlatformVFXCompositeDefinition& Definition,
        const FGamePlatformVFXRequest& Request,
        const FGamePlatformVFXHandle& ParentHandle,
        FPlayChild PlayChild,
        FRegisterTimer RegisterTimer);
};
