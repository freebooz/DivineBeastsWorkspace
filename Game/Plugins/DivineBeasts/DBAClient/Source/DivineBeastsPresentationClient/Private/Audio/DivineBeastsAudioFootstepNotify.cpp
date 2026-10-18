// 脚步声源归属第三层角色/地表动画事实；先追踪脚下真实物理材质，再提交项目中立SFX事件。
#include "Audio/DivineBeastsAudioFootstepNotify.h"

#include "DivineBeastsPresentationClientSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "CollisionQueryParams.h"

void UDivineBeastsAudioFootstepNotify::Notify(
    USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);
    if (!IsValid(MeshComp) || !IsValid(MeshComp->GetOwner())) return;
    UWorld* World = MeshComp->GetWorld();
    if (!World || World->bIsTearingDown || !World->IsGameWorld() ||
        World->GetNetMode() == NM_DedicatedServer) return;
    auto* Instance = World->GetGameInstance();
    auto* Player = Instance ? Instance->GetFirstGamePlayer() : nullptr;
    auto* Presentation = Player
        ? Player->GetSubsystem<UDivineBeastsPresentationClientSubsystem>() : nullptr;
    if (!Presentation) return;

    FVector Origin = MeshComp->GetComponentLocation();
    if (!FootSocketName.IsNone() && MeshComp->DoesSocketExist(FootSocketName))
        Origin = MeshComp->GetSocketLocation(FootSocketName);
    if (Origin.ContainsNaN() || !FMath::IsFinite(TraceUpCm) || !FMath::IsFinite(TraceDownCm))
        return;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(SSLMAudioFootstep), false);
    Params.AddIgnoredActor(MeshComp->GetOwner());
    Params.bReturnPhysicalMaterial = true;
    const FVector From = Origin + FVector(0, 0, FMath::Clamp(TraceUpCm, 0.0f, 100.0f));
    const FVector To = Origin - FVector(0, 0, FMath::Clamp(TraceDownCm, 30.0f, 300.0f));
    FHitResult Hit;
    if (!World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params) ||
        !Hit.bBlockingHit || Hit.ImpactPoint.ContainsNaN()) return;
    FName ClipId = DefaultClipId;
    if (const UPhysicalMaterial* Material = Hit.PhysMaterial.Get())
        if (const FName* Override = PhysicalMaterialToClip.Find(Material->GetFName()))
            ClipId = *Override;
    // 每个动画触地通知只提交一次；客户端平台并发负责高密度脚步抢占。
    Presentation->PlayAudioLibraryClip(ClipId, Hit.ImpactPoint);
}
