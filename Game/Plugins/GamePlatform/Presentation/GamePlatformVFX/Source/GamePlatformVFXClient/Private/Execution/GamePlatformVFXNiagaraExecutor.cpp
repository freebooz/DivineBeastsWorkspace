#include "Execution/GamePlatformVFXNiagaraExecutor.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "Definitions/GamePlatformVFXAreaDefinition.h"
#include "Definitions/GamePlatformVFXAttachedDefinition.h"
#include "Definitions/GamePlatformVFXBeamDefinition.h"
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
        FName AttachPointName = Spawn.AttachPointName;
        if (AttachPointName.IsNone())
        {
            if (const UGamePlatformVFXAttachedDefinition* Attached = Cast<UGamePlatformVFXAttachedDefinition>(&Definition))
            {
                AttachPointName = Attached->DefaultAttachPoint;
            }
        }
        Component = UNiagaraFunctionLibrary::SpawnSystemAttached(
            System,
            Spawn.AttachComponent,
            AttachPointName,
            Spawn.Location,
            Spawn.Rotation,
            EAttachLocation::KeepWorldPosition,
            Definition.ShouldAutoDestroy(),
            false,
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
            false,
            PoolMethod,
            true);
    }

    if (IsValid(Component))
    {
        FGamePlatformVFXParameters EffectiveParameters = Definition.GetDefaultParameters();
        EffectiveParameters.Append(Request.Parameters);
        ApplyParameters(*Component, EffectiveParameters);

        // 行为差异尽量通过Niagara参数表达，避免平台层建立十套执行器。
        if (const UGamePlatformVFXBeamDefinition* Beam = Cast<UGamePlatformVFXBeamDefinition>(&Definition))
        {
            Component->SetVariablePosition(Beam->SourceParameterName, Spawn.Location);
            Component->SetVariablePosition(Beam->TargetParameterName, Spawn.TargetLocation);
        }
        if (const UGamePlatformVFXAreaDefinition* Area = Cast<UGamePlatformVFXAreaDefinition>(&Definition))
        {
            Component->SetVariableFloat(Area->RadiusParameterName, Area->DefaultRadius);
        }
        Component->SetVariablePosition(TEXT("User.ImpactLocation"), Spawn.ImpactLocation);
        Component->SetVariableVec3(TEXT("User.ImpactNormal"), Spawn.ImpactNormal);
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
