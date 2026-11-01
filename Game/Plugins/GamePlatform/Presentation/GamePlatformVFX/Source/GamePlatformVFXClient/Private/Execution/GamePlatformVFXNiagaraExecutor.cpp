// 本文件属于GamePlatform平台层 GamePlatformVFX，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
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
#include "GameFramework/Actor.h"
#include "Interfaces/GamePlatformMotionSource.h"

bool FGamePlatformVFXNiagaraExecutor::IsAttachmentValid(const UWorld& World,
    const UGamePlatformVFXDefinition& Definition, const FGamePlatformVFXSpawnContext& Spawn)
{
    check(IsInGameThread());
    USceneComponent* Target = Spawn.AttachComponent.Get();
    if (!Target)
    {
        return Spawn.AttachComponent.IsExplicitlyNull() &&
            Definition.GetBehavior() != EGamePlatformVFXBehavior::Attached;
    }
    AActor* Owner = Target->GetOwner();
    return IsValid(Target) && Target->IsRegistered() && IsValid(Owner) &&
        !Owner->IsActorBeingDestroyed() && Target->GetWorld() == &World && !World.bIsTearingDown;
}

UNiagaraComponent* FGamePlatformVFXNiagaraExecutor::Spawn(
    UWorld& World,
    const UGamePlatformVFXDefinition& Definition,
    const FGamePlatformVFXRequest& Request,
    bool bUsePool)
{
    if (!IsAttachmentValid(World, Definition, Request.SpawnContext))
    {
        return nullptr;
    }
    UNiagaraSystem* System = (Request.bUseBaseNiagaraSystem ? Definition.GetNiagaraSystem() :
        Definition.ResolveNiagaraSystem(Request.PlatformId, Request.QualityTier)).Get();
    if (!IsValid(System))
    {
        return nullptr;
    }

    const ENCPoolMethod PoolMethod = bUsePool ? ENCPoolMethod::AutoRelease : ENCPoolMethod::None;
    const FGamePlatformVFXSpawnContext& Spawn = Request.SpawnContext;

    UNiagaraComponent* Component = nullptr;
    USceneComponent* AttachTarget = Spawn.AttachComponent.Get();
    const bool bWorldOwnedProjectile = Definition.GetBehavior() == EGamePlatformVFXBehavior::Projectile;
    if (AttachTarget && !bWorldOwnedProjectile)
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
            AttachTarget,
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
        // 投射物组件由WorldSettings/原生World池拥有；附着只跟随变换，不随来源Actor销毁。
        if (bWorldOwnedProjectile && AttachTarget)
        {
            // SpawnSystemAtLocation默认绝对变换；只挂父指针不会随弹体移动，需恢复相对变换。
            Component->SetAbsolute(false, false, false);
            Component->AttachToComponent(AttachTarget, FAttachmentTransformRules::KeepWorldTransform, Spawn.AttachPointName);
        }
        // 默认参数先写、请求覆盖后写，避免每次Spawn复制并合并六个TMap。
        ApplyParameters(*Component, Definition.GetDefaultParameters());
        ApplyParameters(*Component, Request.Parameters);

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

int32 FGamePlatformVFXNiagaraExecutor::ApplyMotionParameters(UNiagaraComponent& Component,const FGamePlatformMotionSnapshot& S)
{
    const UNiagaraSystem* System=Component.GetAsset();
    if(!System || !S.bValid || !S.bEnableSpeedBasedVFX || !FMath::IsFinite(S.Speed) ||
        !FMath::IsFinite(S.SpeedNormalized) || !FMath::IsFinite(S.AccelerationNormalized) ||
        !FMath::IsFinite(S.TrailLengthScale) || S.FlightDirection.ContainsNaN())return 0;
    const auto Has=[&](const FNiagaraTypeDefinition& Type,const FName Name)
    {return System->GetExposedParameters().IndexOf(FNiagaraVariable(Type,Name))!=INDEX_NONE;};
    int32 Count=0;
    const auto Float=[&](const TCHAR* Name,float Value)
    {if(Has(FNiagaraTypeDefinition::GetFloatDef(),Name)){Component.SetVariableFloat(Name,Value);++Count;}};
    Float(TEXT("User.ProjectileSpeed"),S.Speed);
    Float(TEXT("User.SpeedNormalized"),S.SpeedNormalized);
    Float(TEXT("User.AccelerationNormalized"),S.AccelerationNormalized);
    Float(TEXT("User.TrailLengthScale"),S.TrailLengthScale);
    if(Has(FNiagaraTypeDefinition::GetVec3Def(),TEXT("User.FlightDirection")))
    {Component.SetVariableVec3(TEXT("User.FlightDirection"),S.FlightDirection);++Count;}
    if(Has(FNiagaraTypeDefinition::GetIntDef(),TEXT("User.FlightPhase")))
    {Component.SetVariableInt(TEXT("User.FlightPhase"),S.FlightPhase);++Count;}
    return Count;
}

bool FGamePlatformVFXNiagaraExecutor::UpdateProjectileMotion(UNiagaraComponent& Component,TFunctionRef<bool()> StillOwned)
{
    const auto* Parent=Component.GetAttachParent();
    AActor* Source=Parent ? Parent->GetOwner() : nullptr;
    if(!IsValid(Source) || Source->IsActorBeingDestroyed() || Source->GetWorld()!=Component.GetWorld() ||
        !Source->GetClass()->ImplementsInterface(UGamePlatformMotionSource::StaticClass()))return false;
    if(!StillOwned())return false;
    const TWeakObjectPtr<UNiagaraComponent> WeakComponent(&Component);
    const TWeakObjectPtr<USceneComponent> OriginalParent(Component.GetAttachParent());
    const TWeakObjectPtr<AActor> OriginalSource(Source);
    const TWeakObjectPtr<UWorld> OriginalWorld(Component.GetWorld());
    const TWeakObjectPtr<UNiagaraSystem> OriginalSystem(Component.GetAsset());
    const auto Snapshot=IGamePlatformMotionSource::Execute_GetMotionSnapshot(Source);
    // 查询是外部可重入边界：原生池可在回调中把同一UObject交给新Owner，IsValid不足以证明所有权。
    if(!StillOwned() || !WeakComponent.IsValid() || !OriginalWorld.IsValid() || OriginalWorld->bIsTearingDown ||
        !OriginalParent.IsValid() || !OriginalSource.IsValid() || OriginalSource->IsActorBeingDestroyed() ||
        WeakComponent->GetWorld()!=OriginalWorld.Get() || WeakComponent->GetAttachParent()!=OriginalParent.Get() ||
        OriginalParent->GetOwner()!=OriginalSource.Get() || WeakComponent->GetAsset()!=OriginalSystem.Get())return false;
    const int32 Written=ApplyMotionParameters(*WeakComponent,Snapshot);
    // 初始复制尚未齐全时保留已有绑定，后续真实运动帧可重新读到有效样本。
    return !Snapshot.bValid || (Snapshot.bEnableSpeedBasedVFX && Written>0);
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
