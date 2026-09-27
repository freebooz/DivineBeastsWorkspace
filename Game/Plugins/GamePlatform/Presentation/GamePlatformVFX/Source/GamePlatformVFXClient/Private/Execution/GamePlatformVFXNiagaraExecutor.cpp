#include "Execution/GamePlatformVFXNiagaraExecutor.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

UNiagaraComponent* FGamePlatformVFXNiagaraExecutor::Spawn(
    UWorld& World,
    const UGamePlatformVFXDefinition& Definition,
    const FGamePlatformVFXRequest& Request,
    bool bUsePool)
{
    UNiagaraSystem* System = Definition.ResolveNiagaraSystem(
        Request.PlatformId,
        Request.QualityTier).Get();
    if (!IsValid(System))
    {
        return nullptr;
    }

    const ENCPoolMethod PoolMethod = bUsePool ? ENCPoolMethod::AutoRelease : ENCPoolMethod::None;
    const FGamePlatformVFXSpawnContext& Spawn = Request.SpawnContext;

    UNiagaraComponent* Component = nullptr;
    if (IsValid(Spawn.AttachComponent))
    {
        Component = UNiagaraFunctionLibrary::SpawnSystemAttached(
            System,
            Spawn.AttachComponent,
            Spawn.AttachPointName,
            Spawn.Location,
            Spawn.Rotation,
            EAttachLocation::KeepWorldPosition,
            Definition.ShouldAutoDestroy(),
            true,
            PoolMethod,
            true);

        if (IsValid(Component))
        {
            Component->SetWorldScale3D(Spawn.Scale);
        }
    }
    else
    {
        Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            &World,
            System,
            Spawn.Location,
            Spawn.Rotation,
            Spawn.Scale,
            Definition.ShouldAutoDestroy(),
            true,
            PoolMethod,
            true);
    }

    if (IsValid(Component))
    {
        FGamePlatformVFXParameters EffectiveParameters = Definition.GetDefaultParameters();
        EffectiveParameters.Append(Request.Parameters);
        ApplyParameters(*Component, EffectiveParameters);
    }

    return Component;
}

void FGamePlatformVFXNiagaraExecutor::ApplyParameters(
    UNiagaraComponent& Component,
    const FGamePlatformVFXParameters& Parameters)
{
    for (const TPair<FName, float>& Pair : Parameters.FloatParameters)
    {
        Component.SetVariableFloat(Pair.Key, Pair.Value);
    }

    for (const TPair<FName, FVector>& Pair : Parameters.VectorParameters)
    {
        Component.SetVariableVec3(Pair.Key, Pair.Value);
    }

    for (const TPair<FName, FVector>& Pair : Parameters.PositionParameters)
    {
        Component.SetVariablePosition(Pair.Key, Pair.Value);
    }

    for (const TPair<FName, FLinearColor>& Pair : Parameters.ColorParameters)
    {
        Component.SetVariableLinearColor(Pair.Key, Pair.Value);
    }

    for (const TPair<FName, int32>& Pair : Parameters.IntegerParameters)
    {
        Component.SetVariableInt(Pair.Key, Pair.Value);
    }

    for (const TPair<FName, bool>& Pair : Parameters.BooleanParameters)
    {
        Component.SetVariableBool(Pair.Key, Pair.Value);
    }
}
