#include "Characters/DivineBeastsCharacterAppearanceWorldSubsystem.h"

#include "Characters/DivineBeastsCharacterAppearanceComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"

void UDivineBeastsCharacterAppearanceWorldSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (UWorld* World = GetWorld())
    {
        ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
            FOnActorSpawned::FDelegate::CreateUObject(
                this,
                &UDivineBeastsCharacterAppearanceWorldSubsystem::HandleActorSpawned));
    }
}

void UDivineBeastsCharacterAppearanceWorldSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld();
        World && ActorSpawnedHandle.IsValid())
    {
        World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
    }
    ActorSpawnedHandle.Reset();

    Super::Deinitialize();
}

void UDivineBeastsCharacterAppearanceWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    // 子系统初始化前已存在的角色只补扫一次；后续全部走ActorSpawned事件。
    for (TActorIterator<ACharacter> It(&InWorld); It; ++It)
    {
        EnsureAppearanceComponent(*It);
    }
}

void UDivineBeastsCharacterAppearanceWorldSubsystem::HandleActorSpawned(AActor* Actor)
{
    EnsureAppearanceComponent(Actor);
}

void UDivineBeastsCharacterAppearanceWorldSubsystem::EnsureAppearanceComponent(
    AActor* Actor)
{
    ACharacter* Character = Cast<ACharacter>(Actor);
    if (!Character ||
        Character->FindComponentByClass<UDivineBeastsCharacterAppearanceComponent>())
    {
        return;
    }

    UDivineBeastsCharacterAppearanceComponent* Component =
        NewObject<UDivineBeastsCharacterAppearanceComponent>(
            Character,
            TEXT("DivineBeastsCharacterAppearance"));
    if (!Component)
    {
        return;
    }

    Character->AddInstanceComponent(Component);
    Component->RegisterComponent();
}
