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
    if (!World->LineTraceSingleByChannel(Hit, From, To, TraceChannel.GetValue(), Params) ||
        !Hit.bBlockingHit || Hit.ImpactPoint.ContainsNaN()) return;
    FName ClipId = DefaultClipId;
    if (const UPhysicalMaterial* Material = Hit.PhysMaterial.Get())
    {
        // 按真实资产名优先，资产未专门配置时才使用物理SurfaceType；无映射退回默认语义。
        if (const FName* Override = PhysicalMaterialToClip.Find(Material->GetFName()))
            ClipId = *Override;
        else if (const FName* Surface = SurfaceTypeToClip.Find(Material->SurfaceType))
            ClipId = *Surface;
    }
    // 多蒙太奇/动画层在同一脚掌短时间内可能重复通知；由LocalPlayer子系统去重，不能在共享Notify对象存状态。
    Presentation->PlayAudioLibraryFootstepClip(ClipId, Hit.ImpactPoint, MeshComp, FootSocketName);
}
