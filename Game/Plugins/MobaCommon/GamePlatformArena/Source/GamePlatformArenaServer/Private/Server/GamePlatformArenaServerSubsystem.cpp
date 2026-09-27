#include "Server/GamePlatformArenaServerSubsystem.h"

#include "Backend/GamePlatformArenaServerBackendClient.h"
#include "Framework/GamePlatformArenaGameMode.h"
#include "Server/GamePlatformArenaServerCoordinator.h"
#include "Server/GamePlatformArenaServerProjectExtension.h"
#include "Features/IModularFeatures.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"

bool UGamePlatformArenaServerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World != nullptr && World->GetNetMode() == NM_DedicatedServer;
}

void UGamePlatformArenaServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FGamePlatformArenaServerBackendConfig Config;
    Config.BaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("GAMESERVERCONTROL_BASE_URL"));
    Config.InternalToken = FPlatformMisc::GetEnvironmentVariable(TEXT("GAMESERVERCONTROL_INTERNAL_TOKEN"));
    Config.GameServerId = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_ID"));
    if (!Config.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Arena server backend config incomplete; assignment auto-load disabled."));
        return;
    }

    BackendClient = MakeShared<FGamePlatformArenaServerBackendClient>(MoveTemp(Config));
    Coordinator = MakeShared<FGamePlatformArenaServerCoordinator>(BackendClient.ToSharedRef());

    const TArray<IGamePlatformArenaServerProjectExtension*> Extensions =
        IModularFeatures::Get()
            .GetModularFeatureImplementations<IGamePlatformArenaServerProjectExtension>(
                IGamePlatformArenaServerProjectExtension::GetModularFeatureName());
    if (Extensions.Num() > 1)
    {
        UE_LOG(LogTemp, Error, TEXT("Arena server project extension conflict: %d providers registered."), Extensions.Num());
        Coordinator.Reset();
        BackendClient.Reset();
        return;
    }
    Coordinator->SetProjectExtension(Extensions.Num() == 1 ? Extensions[0] : nullptr);
}

void UGamePlatformArenaServerSubsystem::Deinitialize()
{
    Coordinator.Reset();
    BackendClient.Reset();
    Super::Deinitialize();
}

void UGamePlatformArenaServerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (!Coordinator.IsValid()) { return; }

    AGamePlatformArenaGameMode* ArenaGameMode = InWorld.GetAuthGameMode<AGamePlatformArenaGameMode>();
    if (ArenaGameMode == nullptr) { return; }

    const TWeakObjectPtr<AGamePlatformArenaGameMode> WeakArenaMode(ArenaGameMode);
    ArenaGameMode->OnResultPending().AddWeakLambda(
        this,
        [this, WeakArenaMode](const FGamePlatformArenaMatchResult&)
        {
            if (!Coordinator.IsValid()) { return; }
            AGamePlatformArenaGameMode* Mode = WeakArenaMode.Get();
            if (Mode == nullptr) { return; }
            Coordinator->SubmitPendingResult(
                Mode,
                [](bool bCommitted, const FString& Error)
                {
                    if (bCommitted)
                    {
                        UE_LOG(LogTemp, Log, TEXT("MainArena MatchResult committed; GamePlatformServer Drain/Release integration remains the next server-lifecycle handoff."));
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("MainArena MatchResult remains pending: %s"), *Error);
                    }
                });
        });

    Coordinator->LoadAndApplyAssignment(
        ArenaGameMode,
        [](bool bSuccess, const FString& Error)
        {
            if (bSuccess)
            {
                UE_LOG(LogTemp, Log, TEXT("MainArena assignment loaded and validated."));
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("MainArena assignment load failed: %s"), *Error);
            }
        });
}
