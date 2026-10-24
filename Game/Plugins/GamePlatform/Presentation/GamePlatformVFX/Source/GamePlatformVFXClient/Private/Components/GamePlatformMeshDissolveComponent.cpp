#include "Components/GamePlatformMeshDissolveComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

UGamePlatformMeshDissolveComponent::UGamePlatformMeshDissolveComponent()
{
    PrimaryComponentTick.bCanEverTick=false;
    SetIsReplicatedByDefault(false);
}
bool UGamePlatformMeshDissolveComponent::StartDissolve(int32 InstanceGeneration,
    const FGamePlatformMeshDissolveSettings& Settings,bool bInitiallyCompleted)
{
    check(IsInGameThread());
    UWorld* World=GetWorld();AActor* Owner=GetOwner();
    if(bClosing||!World||World->bIsTearingDown||World->GetNetMode()==NM_DedicatedServer||
        !IsValid(Owner)||Owner->IsActorBeingDestroyed()||InstanceGeneration<=0||
        !FMath::IsFinite(Settings.DelaySeconds)||Settings.DelaySeconds<0||Settings.DelaySeconds>30||
        !FMath::IsFinite(Settings.DurationSeconds)||Settings.DurationSeconds<.01f||Settings.DurationSeconds>30||
        !FMath::IsFinite(Settings.EdgeStrength)||Settings.EdgeStrength<0||Settings.EdgeStrength>100||
        !FMath::IsFinite(Settings.EdgeColor.R)||!FMath::IsFinite(Settings.EdgeColor.G)||
        !FMath::IsFinite(Settings.EdgeColor.B)||!FMath::IsFinite(Settings.EdgeColor.A)||
        (bActive&&Generation==InstanceGeneration))return false;
    ResetDissolve();
    Generation=InstanceGeneration;ActiveSettings=Settings;
    Operation=FGuid::NewGuid();bActive=true;bFinished=bInitiallyCompleted;
    MaterialStartTime=World->GetTimeSeconds()+Settings.DelaySeconds;
    RefreshDissolveMeshes();
    if(!bInitiallyCompleted)
    {
        const FGuid Expected=Operation;
        World->GetTimerManager().SetTimer(FinishTimer,FTimerDelegate::CreateWeakLambda(this,
            [this,Expected]{FinishDissolve(Expected);}),Settings.DelaySeconds+Settings.DurationSeconds,false);
    }
    return true;
}
void UGamePlatformMeshDissolveComponent::RefreshDissolveMeshes()
{
    check(IsInGameThread());
    AActor* Owner=GetOwner();UWorld* World=GetWorld();
    if(!bActive||bClosing||!IsValid(Owner)||!World||World->bIsTearingDown||
        World->GetNetMode()==NM_DedicatedServer)return;
    TArray<AActor*> Actors;Owner->GetAttachedActors(Actors,true,true);Actors.Add(Owner);
    TSet<UMeshComponent*> Seen;
    for(AActor* Actor:Actors)
    {
        if(!IsValid(Actor)||Actor->GetWorld()!=World||Actor->IsActorBeingDestroyed())continue;
        TInlineComponentArray<UMeshComponent*> Meshes(Actor);
        for(UMeshComponent* Mesh:Meshes)
        {
            if(!IsValid(Mesh)||Seen.Contains(Mesh))continue;
            Seen.Add(Mesh);
            FVisibility* V=Visibility.FindByPredicate([Mesh](const FVisibility& R){return R.Mesh==Mesh;});
            if(!V){FVisibility R;R.Mesh=Mesh;R.bInitiallyVisible=Mesh->IsVisible();Visibility.Add(R);V=&Visibility.Last();}
            if(bFinished)
            {
                if(Mesh->IsVisible()){Mesh->SetVisibility(false,false);V->bHiddenByThis=true;}
                continue;
            }
            for(int32 Index=0;Index<Mesh->GetNumMaterials();++Index)
            {
                UMaterialInterface* Source=Mesh->GetMaterial(Index);if(!IsValid(Source))continue;
                if(Slots.ContainsByPredicate([Mesh,Index,Source](const FSlot& S)
                    {return S.Mesh==Mesh&&S.Index==Index&&S.Dynamic.Get()==Source;}))continue;
                float Value=0;
                if(!Source->GetScalarParameterValue(FMaterialParameterInfo(TEXT("GP_DissolveEnabled")),Value))
                {
                    UE_LOG(LogTemp,Warning,TEXT("MeshDissolveMaterialUnsupported: %s；截止隐藏回退，不替换其原始材质。"),*Source->GetPathName());
                    continue;
                }
                UMaterialInstanceDynamic* MID=UMaterialInstanceDynamic::Create(Source,this);
                if(!MID)continue;
                MID->SetScalarParameterValue(TEXT("GP_DissolveEnabled"),1);
                MID->SetScalarParameterValue(TEXT("GP_DissolveStartTime"),MaterialStartTime);
                MID->SetScalarParameterValue(TEXT("GP_DissolveDuration"),ActiveSettings.DurationSeconds);
                MID->SetScalarParameterValue(TEXT("GP_DissolvePreviewProgress"),-1);
                MID->SetScalarParameterValue(TEXT("GP_DissolveEdgeStrength"),ActiveSettings.EdgeStrength);
                MID->SetVectorParameterValue(TEXT("GP_DissolveEdgeColor"),ActiveSettings.EdgeColor);
                FSlot Slot;Slot.Mesh=Mesh;Slot.Index=Index;Slot.Original=Source;Slot.Dynamic=MID;
                Slots.Add(Slot);DynamicMaterials.Add(MID);Mesh->SetMaterial(Index,MID);
            }
        }
    }
}
void UGamePlatformMeshDissolveComponent::FinishDissolve(FGuid ExpectedOperation)
{
    if(bClosing||!bActive||Operation!=ExpectedOperation||!ExpectedOperation.IsValid())return;
    if(UWorld* World=GetWorld())World->GetTimerManager().ClearTimer(FinishTimer);
    bFinished=true;RefreshDissolveMeshes();
}
void UGamePlatformMeshDissolveComponent::ResetDissolve()
{
    check(IsInGameThread());
    Operation.Invalidate();
    if(UWorld* World=GetWorld())World->GetTimerManager().ClearTimer(FinishTimer);
    for(const FSlot& Slot:Slots)
        if(auto* Mesh=Slot.Mesh.Get();IsValid(Mesh)&&Mesh->GetMaterial(Slot.Index)==Slot.Dynamic.Get())
            Mesh->SetMaterial(Slot.Index,Slot.Original.Get());
    for(const FVisibility& V:Visibility)
        if(auto* Mesh=V.Mesh.Get();IsValid(Mesh)&&V.bHiddenByThis&&V.bInitiallyVisible&&!Mesh->IsVisible())
            Mesh->SetVisibility(true,false);
    Slots.Reset();Visibility.Reset();DynamicMaterials.Reset();
    bActive=false;bFinished=false;Generation=0;
}
void UGamePlatformMeshDissolveComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bClosing=true;ResetDissolve();Super::EndPlay(Reason);
}
void UGamePlatformMeshDissolveComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    bClosing=true;ResetDissolve();Super::OnComponentDestroyed(bDestroyingHierarchy);
}
