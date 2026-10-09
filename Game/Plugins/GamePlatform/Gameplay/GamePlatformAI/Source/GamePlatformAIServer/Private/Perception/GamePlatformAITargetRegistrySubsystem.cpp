#include "Perception/GamePlatformAITargetRegistrySubsystem.h"

#include "Components/GamePlatformAITargetComponent.h"
#include "Components/GamePlatformAIStateComponent.h"
#include "Controllers/GamePlatformAIController.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AIWorldPolicy.h"
#include "Engine/World.h"

bool UGamePlatformAITargetRegistrySubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{ return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }
bool UGamePlatformAITargetRegistrySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && Super::ShouldCreateSubsystem(Outer) && GamePlatformAIWorldPolicy::CanRun(
        DoesSupportWorldType(World->WorldType), World->GetNetMode() != NM_Client, IsRunningCommandlet());
}
bool UGamePlatformAITargetRegistrySubsystem::IsAuthorityRuntimeWorld() const
{
    const UWorld* World = GetWorld();
    return World && !World->bIsTearingDown && GamePlatformAIWorldPolicy::CanRun(
        DoesSupportWorldType(World->WorldType), World->GetNetMode() != NM_Client, IsRunningCommandlet());
}

void UGamePlatformAITargetRegistrySubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UWorld* World = GetWorld();
    if (!IsAuthorityRuntimeWorld())
    {
        return;
    }

    ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
        FOnActorSpawned::FDelegate::CreateUObject(
            this,
            &UGamePlatformAITargetRegistrySubsystem::HandleActorSpawned));

    // 一次性补登记关卡中已经存在的目标；不是逐帧全世界扫描。
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        RegisterActorIfEligible(*It);
        EnsureServerAIController(*It);
    }
}

void UGamePlatformAITargetRegistrySubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        if (ActorSpawnedHandle.IsValid())
        {
            World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
        }
    }

    ActorSpawnedHandle.Reset();
    Super::Deinitialize();
}

void UGamePlatformAITargetRegistrySubsystem::HandleActorSpawned(AActor* Actor)
{
    RegisterActorIfEligible(Actor);
    EnsureServerAIController(Actor);
}

void UGamePlatformAITargetRegistrySubsystem::RegisterActorIfEligible(
    AActor* Actor)
{
    if (!IsAuthorityRuntimeWorld() || !IsValid(Actor) || !Actor->HasAuthority() || Actor->GetWorld() != GetWorld() ||
        !Actor->FindComponentByClass<UGamePlatformAITargetComponent>())
    {
        return;
    }

    UAIPerceptionSystem::RegisterPerceptionStimuliSource(
        this,
        UAISense_Sight::StaticClass(),
        Actor);
}

void UGamePlatformAITargetRegistrySubsystem::EnsureServerAIController(
    AActor* Actor)
{
    APawn* Pawn = Cast<APawn>(Actor);
    if (!IsAuthorityRuntimeWorld() || !IsValid(Pawn) || !Pawn->HasAuthority() || Pawn->GetWorld() != GetWorld() ||
        Pawn->GetController() ||
        !Pawn->FindComponentByClass<UGamePlatformAIStateComponent>() ||
        !GetWorld() ||
        GetWorld()->GetNetMode() == NM_Client)
    {
        return;
    }

    FActorSpawnParameters Params;
    Params.Owner = Pawn;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AGamePlatformAIController* Controller =
        GetWorld()->SpawnActor<AGamePlatformAIController>(
            AGamePlatformAIController::StaticClass(),
            Pawn->GetActorLocation(),
            Pawn->GetActorRotation(),
            Params);

    if (Controller)
    {
        Controller->Possess(Pawn);
    }
}
