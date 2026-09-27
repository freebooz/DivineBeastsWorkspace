#include "Perception/GamePlatformAITargetRegistrySubsystem.h"

#include "Components/GamePlatformAITargetComponent.h"
#include "Components/GamePlatformAIStateComponent.h"
#include "Controllers/GamePlatformAIController.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISense_Sight.h"

void UGamePlatformAITargetRegistrySubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client)
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
    if (!IsValid(Actor) ||
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
    if (!IsValid(Pawn) ||
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
