// 足底表现使用移动组件已有地面快照，只有触地跨越才确认射线；无雪、静止、空中不写高度图。
#include "Components/GamePlatformSnowFootContactComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Interfaces/IGamePlatformSurfaceService.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Engine/SkeletalMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogSurfaceSnowFoot,Log,All);

UGamePlatformSnowFootContactComponent::UGamePlatformSnowFootContactComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}
void UGamePlatformSnowFootContactComponent::SetContactMesh(USkeletalMeshComponent* Mesh) { ContactMesh = Mesh; }
void UGamePlatformSnowFootContactComponent::SetContactBoneCandidates(const TArray<FName>& Left, const TArray<FName>& Right)
{
    BoneCandidates[0]=Left; BoneCandidates[1]=Right;
    for (auto& Candidates:BoneCandidates) if (Candidates.Num()>8) Candidates.SetNum(8);
    ResolvedMesh.Reset();
}
void UGamePlatformSnowFootContactComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer) return;
    if (auto* Mesh = ContactMesh.Get())
        BonesHandle = Mesh->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this,&ThisClass::OnBonesFinalized));
}
void UGamePlatformSnowFootContactComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* Mesh = ContactMesh.Get(); Mesh && BonesHandle.IsValid()) Mesh->UnregisterOnBoneTransformsFinalizedDelegate(BonesHandle);
    BonesHandle.Reset(); ContactMesh.Reset();
    Super::EndPlay(Reason);
}
void UGamePlatformSnowFootContactComponent::OnBonesFinalized()
{
    // 引擎最终姿态广播位于游戏线程；不在动画并行求值任务访问世界。
    if (!IsInGameThread()) return;
    auto* World = GetWorld(); auto* Mesh = ContactMesh.Get(); auto* Character = Cast<ACharacter>(GetOwner());
    if (!World || World->bIsTearingDown || !Mesh || !Character) return;
    auto* Surface = IGamePlatformSurfaceService::Get(*World);
    if (!Surface || Surface->GetEnvironmentState().GlobalSnowAmount < .01f) return;
    auto* Movement = Character->GetCharacterMovement();
    if (!Movement) return;
    const bool bGrounded = Movement->IsMovingOnGround() && Movement->CurrentFloor.IsWalkableFloor();
    const float Speed = Character->GetVelocity().Size2D();
    if (ResolvedMesh.Get()!=Mesh->GetSkeletalMeshAsset())
    {
        ResolvedMesh=Mesh->GetSkeletalMeshAsset();
        for (int32 Index=0;Index<2;++Index)
        {
            ContactBones[Index]=NAME_None; Feet[Index]={};
            for (const FName Candidate:BoneCandidates[Index])
                if (!Candidate.IsNone() && Mesh->DoesSocketExist(Candidate)) { ContactBones[Index]=Candidate; break; }
        }
        UE_LOG(LogSurfaceSnowFoot,Display,TEXT("GP_SNOW_FOOT_BONES owner=%s left=%s right=%s"),*Character->GetName(),*ContactBones[0].ToString(),*ContactBones[1].ToString());
    }
    for (int32 Index=0; Index<2; ++Index)
    {
        if (ContactBones[Index].IsNone()) continue;
        const FVector Foot = Mesh->GetSocketLocation(ContactBones[Index]);
        const auto& Floor = Movement->CurrentFloor.HitResult;
        const float Height = FVector::DotProduct(Foot-Floor.ImpactPoint,Floor.ImpactNormal);
        if (!Feet[Index].Update(Foot,Height,Speed,bGrounded,World->GetTimeSeconds())) continue;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(SurfaceSnowFoot),false,Character);
        FHitResult Hit;
        if (!World->LineTraceSingleByChannel(Hit,Foot+FVector(0,0,25),Foot-FVector(0,0,45),ECC_Visibility,Params) || !Hit.bBlockingHit || Hit.ImpactNormal.Z < .45f) continue;
        FGamePlatformSnowContact Contact; Contact.EventId=FGuid::NewGuid(); Contact.Location=Hit.ImpactPoint;
        Contact.Forward=Character->GetActorForwardVector();
        Surface->SubmitSnowContact(Contact);
    }
}
