// SSLM新手村NPC外观：通过GamePlatformData一次加载Arissa，复制生成与销毁均按事件处理。
#include "Creatures/DivineBeastsVillageNPCVisualWorldSubsystem.h"

#include "Animation/AnimInstance.h"
#include "Characters/DivineBeastsVillageNPC.h"
#include "Characters/DivineBeastsDeathDissolveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Misc/ConfigCacheIni.h"

bool UDivineBeastsVillageNPCVisualWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && !IsRunningCommandlet() && World->IsGameWorld() &&
        World->GetNetMode() != NM_DedicatedServer && Super::ShouldCreateSubsystem(Outer);
}

void UDivineBeastsVillageNPCVisualWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (bClosing || &InWorld != GetWorld() || InWorld.bIsTearingDown) return;
    Operation = FGuid::NewGuid();
    SpawnHandle = InWorld.AddOnActorSpawnedHandler(
        FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::ObserveActor));
    for (TActorIterator<ADivineBeastsVillageNPC> It(&InWorld); It; ++It) ObserveActor(*It);
}

void UDivineBeastsVillageNPCVisualWorldSubsystem::ObserveActor(AActor* Spawned)
{
    auto* NPC = Cast<ADivineBeastsVillageNPC>(Spawned);
    UWorld* World = GetWorld();
    if (bClosing || !IsValid(NPC) || NPC->IsActorBeingDestroyed() || !World ||
        World->bIsTearingDown || NPC->GetWorld() != World || ObservedNPCs.Contains(NPC)) return;
    ObservedNPCs.Add(NPC);
    NPC->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleNPCEndPlay);
    if (bReady) ApplyVisual(*NPC);
    else if (!bLoading) BeginLoadVisuals();
}

void UDivineBeastsVillageNPCVisualWorldSubsystem::BeginLoadVisuals()
{
    if (bClosing || bLoading || !GConfig) return;
    FString MeshName, AnimName;
    const TCHAR* Section = TEXT("DivineBeasts.VillageNPC");
    GConfig->GetString(Section, TEXT("ArissaSkeletalMesh"), MeshName, GGameIni);
    GConfig->GetString(Section, TEXT("ArissaAnimationBlueprint"), AnimName, GGameIni);
    MeshPath = FSoftObjectPath(MeshName);
    AnimPath = FSoftObjectPath(AnimName);
    UWorld* World = GetWorld();
    if (!MeshPath.IsValid() || !AnimPath.IsValid() || !World ||
        World->bIsTearingDown || !World->GetGameInstance()) return;
    auto* Data = IGamePlatformDataService::Get(*World->GetGameInstance());
    if (!Data) return;
    bLoading = true;
    const FGuid Expected = Operation;
    TWeakObjectPtr<ThisClass> WeakThis(this);
    FGamePlatformResult Accepted;
    VisualLease = Data->AcquireResources({MeshPath, AnimPath}, EGamePlatformDataLifetime::World, this,
        [WeakThis, Expected](const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
        {
            auto* Self = WeakThis.Get();
            if (!Self || Self->bClosing || Self->Operation != Expected ||
                !Self->VisualLease.IsValid() || Self->VisualLease.LeaseId != Lease.LeaseId ||
                Self->VisualLease.ScopeId != Lease.ScopeId || Self->VisualLease.Generation != Lease.Generation ||
                Self->VisualLease.IssuerProof != Lease.IssuerProof) return;
            Self->bLoading = false;
            UWorld* CurrentWorld = Self->GetWorld();
            if (!CurrentWorld || CurrentWorld->bIsTearingDown) return;
            auto* Mesh = Cast<USkeletalMesh>(Self->MeshPath.ResolveObject());
            auto* Anim = Cast<UClass>(Self->AnimPath.ResolveObject());
            Self->bReady = Result.IsSuccess() && IsValid(Mesh) && IsValid(Anim) &&
                Anim->IsChildOf(UAnimInstance::StaticClass());
            if (!Self->bReady)
            {
                UE_LOG(LogTemp, Warning, TEXT("VillageNPCVisualRejected: Arissa模型或动画资源未加载。"));
                return;
            }
            const auto NPCs = Self->ObservedNPCs.Array();
            for (const auto& WeakNPC : NPCs)
            {
                if (Self->bClosing || Self->Operation != Expected || CurrentWorld->bIsTearingDown) break;
                if (auto* NPC = WeakNPC.Get()) Self->ApplyVisual(*NPC);
            }
        }, Accepted);
    if (!Accepted.IsSuccess())
    {
        bLoading = false;
        UE_LOG(LogTemp, Warning, TEXT("VillageNPCVisualRequestDenied: Arissa外观租约请求失败。"));
    }
}

void UDivineBeastsVillageNPCVisualWorldSubsystem::ApplyVisual(ADivineBeastsVillageNPC& NPC)
{
    if (bClosing || !bReady || !IsValid(&NPC) || NPC.IsActorBeingDestroyed() ||
        NPC.GetWorld() != GetWorld() || !ObservedNPCs.Contains(&NPC)) return;
    auto* Mesh = NPC.GetMesh();
    auto* Asset = Cast<USkeletalMesh>(MeshPath.ResolveObject());
    auto* Animation = Cast<UClass>(AnimPath.ResolveObject());
    if (!IsValid(Mesh) || !IsValid(Asset) || !IsValid(Animation)) return;
    Mesh->SetSkeletalMeshAsset(Asset);
    Mesh->SetAnimInstanceClass(Animation);
    UDivineBeastsDeathDissolveComponent::NotifyVisualsChanged(&NPC);
    const FRotator Rotation(0.0f, -90.0f, 0.0f);
    const FVector Offset(0.0f, 0.0f, -NPC.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    Mesh->SetRelativeScale3D(FVector::OneVector);
    Mesh->SetRelativeRotation(Rotation);
    Mesh->SetRelativeLocation(Offset);
    NPC.CacheInitialMeshOffset(Offset, Rotation);
    UE_LOG(LogTemp, Log, TEXT("VillageNPCVisualApplied: Arissa模型与动画已应用。"));
}

void UDivineBeastsVillageNPCVisualWorldSubsystem::HandleNPCEndPlay(AActor* Ended, EEndPlayReason::Type)
{
    auto* NPC = Cast<ADivineBeastsVillageNPC>(Ended);
    if (!NPC) return;
    NPC->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleNPCEndPlay);
    ObservedNPCs.Remove(NPC);
}

void UDivineBeastsVillageNPCVisualWorldSubsystem::Deinitialize()
{
    if (bClosing) return;
    bClosing = true;
    Operation.Invalidate();
    UWorld* World = GetWorld();
    if (World && SpawnHandle.IsValid()) World->RemoveOnActorSpawnedHandler(SpawnHandle);
    SpawnHandle.Reset();
    const auto Old = MoveTemp(ObservedNPCs);
    ObservedNPCs.Reset();
    for (const auto& WeakNPC : Old)
        if (auto* NPC = WeakNPC.Get()) NPC->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleNPCEndPlay);
    const FGamePlatformDataLease OldLease = VisualLease;
    VisualLease = {};
    if (World && World->GetGameInstance())
        if (auto* Data = IGamePlatformDataService::Get(*World->GetGameInstance()); Data && OldLease.IsValid())
            Data->ReleaseResources(OldLease);
    Super::Deinitialize();
}
