#include "Execution/GamePlatformVFXNiagaraExecutor.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "Pooling/GamePlatformVFXPoolingPolicy.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

UNiagaraComponent* FGamePlatformVFXNiagaraExecutor::Spawn(UWorld* World, const UGamePlatformVFXDefinition& Definition, const FGamePlatformVFXRequest& Request)
{
    if (!World)
    {
        return nullptr;
    }

    UNiagaraSystem* System = Definition.NiagaraSystem.Get();
    if (!System)
    {
        return nullptr;
    }

    const ENCPoolMethod PoolingMethod = FGamePlatformVFXPoolingPolicy::ToNiagaraPoolingMethod(Definition.PoolingMode);
    UNiagaraComponent* Component = nullptr;

    if (USceneComponent* AttachComponent = Request.SpawnContext.AttachComponent.Get())
    {
        const FTransform& T = Request.SpawnContext.WorldTransform;
        Component = UNiagaraFunctionLibrary::SpawnSystemAttached(
            System,
            AttachComponent,
            Request.SpawnContext.AttachSocket,
            T.GetLocation(),
            T.Rotator(),
            T.GetScale3D(),
            EAttachLocation::KeepWorldPosition,
            Definition.bAutoDestroy,
            PoolingMethod,
            false,
            Definition.bPreCullCheck);
    }
    else
    {
        const FTransform& T = Request.SpawnContext.WorldTransform;
        Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World,
            System,
            T.GetLocation(),
            T.Rotator(),
            T.GetScale3D(),
            Definition.bAutoDestroy,
            false,
            PoolingMethod,
            Definition.bPreCullCheck);
    }

    if (!Component)
    {
        return nullptr;
    }

    FGamePlatformVFXParameters Parameters = Definition.DefaultParameters;
    Parameters.Append(Request.Parameters);
    ApplyParameters(*Component, Parameters);
    Component->Activate(true);
    return Component;
}

void FGamePlatformVFXNiagaraExecutor::ApplyParameters(UNiagaraComponent& Component, const FGamePlatformVFXParameters& Parameters)
{
    for (const TPair<FName, float>& Pair : Parameters.FloatValues)
    {
        Component.SetVariableFloat(Pair.Key, Pair.Value);
    }
    for (const TPair<FName, int32>& Pair : Parameters.IntValues)
    {
        Component.SetVariableInt(Pair.Key, Pair.Value);
    }
    for (const TPair<FName, FVector>& Pair : Parameters.VectorValues)
    {
        Component.SetVariableVec3(Pair.Key, Pair.Value);
    }
    for (const TPair<FName, FLinearColor>& Pair : Parameters.ColorValues)
    {
        Component.SetVariableLinearColor(Pair.Key, Pair.Value);
    }
}
