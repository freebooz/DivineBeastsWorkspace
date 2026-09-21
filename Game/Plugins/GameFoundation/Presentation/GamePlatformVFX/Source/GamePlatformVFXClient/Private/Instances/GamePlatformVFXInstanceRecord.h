#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "Types/GamePlatformVFXHandle.h"
#include "Types/GamePlatformVFXRequest.h"
#include "Types/GamePlatformVFXTypes.h"

class UNiagaraComponent;

struct FGamePlatformVFXInstanceRecord
{
    FGamePlatformVFXHandle Handle;
    FPrimaryAssetId DefinitionId;
    EGamePlatformVFXLifecycleState State = EGamePlatformVFXLifecycleState::Requested;
    TWeakObjectPtr<UNiagaraComponent> NiagaraComponent;
    FGamePlatformVFXRequest OriginalRequest;
    TSharedPtr<FStreamableHandle> DefinitionLoadHandle;
    TSharedPtr<FStreamableHandle> NiagaraLoadHandle;
    TArray<FGamePlatformVFXHandle> ChildHandles;
};
