#include "GamePlatformDebugPrivate.h"

#include "Registry/GamePlatformDebugRegistry.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDevice.h"
#include "Subsystems/GamePlatformTelemetrySubsystem.h"

#if !UE_BUILD_SHIPPING

namespace
{
    TArray<IConsoleObject*> RegisteredConsoleObjects;
    TArray<FName> RegisteredCommandNames;

    TAutoConsoleVariable<int32> CVarGamePlatformDebugAllowMutating(
        TEXT("gp.Debug.AllowMutating"),
        0,
        TEXT("Allow low-risk mutating development debug commands. 0=read-only, 1=allow."),
        ECVF_Default);

    TAutoConsoleVariable<int32> CVarGamePlatformDebugRemote(
        TEXT("gp.Debug.Remote"),
        0,
        TEXT("Remote debug opt-in. V1 has no arbitrary remote query RPC; default remains 0."),
        ECVF_Default);

    bool ValidateArgumentLength(const TArray<FString>& Args)
    {
        for (const FString& Arg : Args)
        {
            if (Arg.Len() > GamePlatformDebugLimits::MaxCommandArgumentCharacters)
            {
                UE_LOG(
                    LogGamePlatformDebug,
                    Warning,
                    TEXT("Debug command argument rejected: exceeds %d characters."),
                    GamePlatformDebugLimits::MaxCommandArgumentCharacters);
                return false;
            }
        }
        return true;
    }

    bool IsMutatingAllowed()
    {
        return CVarGamePlatformDebugAllowMutating.GetValueOnGameThread() == 1;
    }

    AActor* ResolveConsoleTarget(UWorld* World)
    {
        if (!World)
        {
            return nullptr;
        }

        APlayerController* PC = World->GetFirstPlayerController();
        if (!PC)
        {
            return nullptr;
        }

        if (AActor* ViewTarget = PC->GetViewTarget())
        {
            return ViewTarget;
        }

        return PC->GetPawn();
    }

    EGamePlatformDebugSourceView ResolveSourceView(UWorld* World)
    {
        if (!World)
        {
            return EGamePlatformDebugSourceView::Client;
        }

        return World->GetNetMode() == NM_Client
            ? EGamePlatformDebugSourceView::Client
            : EGamePlatformDebugSourceView::Server;
    }

    void LogSnapshot(FName ProviderId, UWorld* World)
    {
        FGamePlatformDebugCollectContext Context;
        Context.World = World;
        Context.Target = ResolveConsoleTarget(World);
        Context.SourceView = ResolveSourceView(World);
        Context.RequestId = FGuid::NewGuid();
        Context.bAllowExpensive = true;

        FGamePlatformDebugSnapshot Snapshot;
        FGamePlatformDebugRegistry& Registry =
            FGamePlatformDebugRegistry::Get();
        Registry.CollectSnapshot(ProviderId, Context, Snapshot);

        UE_LOG(
            LogGamePlatformDebug,
            Display,
            TEXT("%s"),
            *Registry.BuildSanitizedSummary(Snapshot));
    }

    void RegisterDescriptor(
        const TCHAR* Name,
        const TCHAR* Help,
        const TCHAR* Category,
        const TCHAR* Args,
        EGamePlatformDebugCommandKind Kind)
    {
        FGamePlatformDebugCommandDescriptor Descriptor;
        Descriptor.Name = FName(Name);
        Descriptor.Help = Help;
        Descriptor.Category = FName(Category);
        Descriptor.Args = Args;
        Descriptor.Kind = Kind;
        Descriptor.RequiredPrivilege = TEXT("Developer");
        Descriptor.AllowedBuilds =
        {
            TEXT("DebugGame"),
            TEXT("Development"),
            TEXT("Test")
        };

        if (FGamePlatformDebugRegistry::Get().RegisterCommandDescriptor(Descriptor))
        {
            RegisteredCommandNames.Add(Descriptor.Name);
        }
    }

    void RegisterSnapshotCommand(
        const TCHAR* Name,
        const TCHAR* Help,
        FName ProviderId)
    {
        IConsoleObject* Object =
            IConsoleManager::Get().RegisterConsoleCommand(
                Name,
                Help,
                FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
                    [ProviderId](
                        const TArray<FString>& Args,
                        UWorld* World)
                    {
                        if (!ValidateArgumentLength(Args) || Args.Num() != 0)
                        {
                            UE_LOG(
                                LogGamePlatformDebug,
                                Warning,
                                TEXT("%s accepts no arguments."),
                                *ProviderId.ToString());
                            return;
                        }

                        LogSnapshot(ProviderId, World);
                    }),
                ECVF_Default);

        if (Object)
        {
            RegisteredConsoleObjects.Add(Object);
        }

        RegisterDescriptor(
            Name,
            Help,
            *ProviderId.ToString(),
            TEXT(""),
            EGamePlatformDebugCommandKind::ReadOnly);
    }

    void RegisterPlayerAlias()
    {
        const TCHAR* Name = TEXT("gp.Debug.Player");
        const TCHAR* Help = TEXT("Show sanitized local player/character debug snapshot.");

        IConsoleObject* Object =
            IConsoleManager::Get().RegisterConsoleCommand(
                Name,
                Help,
                FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
                    [](const TArray<FString>& Args, UWorld* World)
                    {
                        if (!ValidateArgumentLength(Args) || Args.Num() != 0)
                        {
                            return;
                        }
                        LogSnapshot(TEXT("Character"), World);
                    }),
                ECVF_Default);

        if (Object)
        {
            RegisteredConsoleObjects.Add(Object);
        }

        RegisterDescriptor(
            Name,
            Help,
            TEXT("Character"),
            TEXT(""),
            EGamePlatformDebugCommandKind::ReadOnly);
    }

    void RegisterCategoryCommand()
    {
        const TCHAR* Name = TEXT("gp.Debug.Category");
        const TCHAR* Help =
            TEXT("Enable/disable a registered provider: gp.Debug.Category <ProviderId> <0|1>.");

        IConsoleObject* Object =
            IConsoleManager::Get().RegisterConsoleCommand(
                Name,
                Help,
                FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
                    [](const TArray<FString>& Args, UWorld*)
                    {
                        if (!ValidateArgumentLength(Args) ||
                            Args.Num() != 2 ||
                            (Args[1] != TEXT("0") && Args[1] != TEXT("1")))
                        {
                            UE_LOG(
                                LogGamePlatformDebug,
                                Warning,
                                TEXT("Usage: gp.Debug.Category <ProviderId> <0|1>"));
                            return;
                        }

                        if (!IsMutatingAllowed())
                        {
                            UE_LOG(
                                LogGamePlatformDebug,
                                Warning,
                                TEXT("Mutating debug commands are disabled. Set gp.Debug.AllowMutating=1 explicitly."));
                            return;
                        }

                        const FName ProviderId(*Args[0]);
                        const bool bEnabled = Args[1] == TEXT("1");
                        if (!FGamePlatformDebugRegistry::Get().SetProviderEnabled(
                                ProviderId,
                                bEnabled))
                        {
                            UE_LOG(
                                LogGamePlatformDebug,
                                Warning,
                                TEXT("Unknown ProviderId: %s"),
                                *Args[0]);
                            return;
                        }

                        UE_LOG(
                            LogGamePlatformDebug,
                            Display,
                            TEXT("Provider %s => %s"),
                            *Args[0],
                            bEnabled ? TEXT("enabled") : TEXT("disabled"));
                    }),
                ECVF_Default);

        if (Object)
        {
            RegisteredConsoleObjects.Add(Object);
        }

        RegisterDescriptor(
            Name,
            Help,
            TEXT("Control"),
            TEXT("<ProviderId> <0|1>"),
            EGamePlatformDebugCommandKind::Mutating);
    }

    void RegisterRefreshCommand()
    {
        const TCHAR* Name = TEXT("gp.Debug.Refresh");
        const TCHAR* Help = TEXT("Advance debug snapshot revision without changing gameplay state.");

        IConsoleObject* Object =
            IConsoleManager::Get().RegisterConsoleCommand(
                Name,
                Help,
                FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
                    [](const TArray<FString>& Args, UWorld*)
                    {
                        if (!ValidateArgumentLength(Args) || Args.Num() != 0)
                        {
                            return;
                        }

                        if (!IsMutatingAllowed())
                        {
                            UE_LOG(
                                LogGamePlatformDebug,
                                Warning,
                                TEXT("Refresh requires gp.Debug.AllowMutating=1."));
                            return;
                        }

                        const uint64 Revision =
                            FGamePlatformDebugRegistry::Get().ForceRefreshRevision();
                        UE_LOG(
                            LogGamePlatformDebug,
                            Display,
                            TEXT("Debug revision advanced to %llu."),
                            Revision);
                    }),
                ECVF_Default);

        if (Object)
        {
            RegisteredConsoleObjects.Add(Object);
        }

        RegisterDescriptor(
            Name,
            Help,
            TEXT("Control"),
            TEXT(""),
            EGamePlatformDebugCommandKind::Mutating);
    }

    void RegisterTraceBridge(
        const TCHAR* DebugName,
        const TCHAR* EngineCommand)
    {
        const FString Help = FString::Printf(
            TEXT("Bridge to UE5.8 Console Manager command '%s'."),
            EngineCommand);

        IConsoleObject* Object =
            IConsoleManager::Get().RegisterConsoleCommand(
                DebugName,
                *Help,
                FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
                    [EngineCommand = FString(EngineCommand)](
                        const TArray<FString>& Args,
                        UWorld* World)
                    {
                        if (!ValidateArgumentLength(Args) || Args.Num() != 0)
                        {
                            return;
                        }

                        if (!IsMutatingAllowed())
                        {
                            UE_LOG(
                                LogGamePlatformDebug,
                                Warning,
                                TEXT("Trace bridge requires gp.Debug.AllowMutating=1."));
                            return;
                        }

                        if (!GLog)
                        {
                            return;
                        }

                        IConsoleManager::Get().ProcessUserConsoleInput(
                            *EngineCommand,
                            *GLog,
                            World);
                    }),
                ECVF_Default);

        if (Object)
        {
            RegisteredConsoleObjects.Add(Object);
        }

        RegisterDescriptor(
            DebugName,
            *Help,
            TEXT("Trace"),
            TEXT(""),
            EGamePlatformDebugCommandKind::Mutating);
    }

    void RegisterTelemetryFlush()
    {
        const TCHAR* Name = TEXT("gp.Debug.Telemetry.Flush");
        const TCHAR* Help =
            TEXT("Development/Test only best-effort Telemetry flush; does not change telemetry schema.");

        IConsoleObject* Object =
            IConsoleManager::Get().RegisterConsoleCommand(
                Name,
                Help,
                FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
                    [](const TArray<FString>& Args, UWorld* World)
                    {
                        if (!ValidateArgumentLength(Args) ||
                            Args.Num() != 0 ||
                            !IsMutatingAllowed())
                        {
                            return;
                        }

                        UGameInstance* GameInstance =
                            World ? World->GetGameInstance() : nullptr;
                        UGamePlatformTelemetrySubsystem* Telemetry =
                            GameInstance
                                ? GameInstance->GetSubsystem<UGamePlatformTelemetrySubsystem>()
                                : nullptr;

                        const bool bFlushed =
                            Telemetry && Telemetry->FlushBestEffort();
                        UE_LOG(
                            LogGamePlatformDebug,
                            Display,
                            TEXT("Telemetry flush result: %s"),
                            bFlushed ? TEXT("accepted") : TEXT("not available"));
                    }),
                ECVF_Default);

        if (Object)
        {
            RegisteredConsoleObjects.Add(Object);
        }

        RegisterDescriptor(
            Name,
            Help,
            TEXT("Telemetry"),
            TEXT(""),
            EGamePlatformDebugCommandKind::Mutating);
    }
}

#endif // !UE_BUILD_SHIPPING

void GamePlatformDebugPrivate::RegisterConsoleCommands()
{
#if !UE_BUILD_SHIPPING
    RegisterSnapshotCommand(TEXT("gp.Debug.Status"), TEXT("Show GamePlatformDebug status."), TEXT("Status"));
    RegisterSnapshotCommand(TEXT("gp.Debug.World"), TEXT("Show world/streaming summary."), TEXT("World"));
    RegisterPlayerAlias();
    RegisterSnapshotCommand(TEXT("gp.Debug.Character"), TEXT("Show selected/local character summary."), TEXT("Character"));
    RegisterSnapshotCommand(TEXT("gp.Debug.Ability"), TEXT("Show bounded GAS summary."), TEXT("Ability"));
    RegisterSnapshotCommand(TEXT("gp.Debug.Combat"), TEXT("Show read-only combat summary."), TEXT("Combat"));
    RegisterSnapshotCommand(TEXT("gp.Debug.AI"), TEXT("Show replicated AI public state summary."), TEXT("AI"));
    RegisterSnapshotCommand(TEXT("gp.Debug.Navigation"), TEXT("Show navigation availability/request summary."), TEXT("Navigation"));
    RegisterSnapshotCommand(TEXT("gp.Debug.Online"), TEXT("Show safe online diagnostic summary."), TEXT("Online"));
    RegisterSnapshotCommand(TEXT("gp.Debug.Session"), TEXT("Show safe session/transfer summary."), TEXT("Session"));
    RegisterSnapshotCommand(TEXT("gp.Debug.Loading"), TEXT("Show loading diagnostic summary."), TEXT("Loading"));
    RegisterSnapshotCommand(TEXT("gp.Debug.Telemetry"), TEXT("Show telemetry diagnostics."), TEXT("Telemetry"));
    RegisterSnapshotCommand(TEXT("gp.Debug.Network"), TEXT("Show bounded network summary."), TEXT("Network"));

    RegisterCategoryCommand();
    RegisterRefreshCommand();
    RegisterTraceBridge(TEXT("gp.Debug.Trace.Start"), TEXT("Trace.Start"));
    RegisterTraceBridge(TEXT("gp.Debug.Trace.Stop"), TEXT("Trace.Stop"));
    RegisterTraceBridge(TEXT("gp.Debug.Trace.Status"), TEXT("Trace.Status"));
    RegisterTelemetryFlush();
#endif
}

void GamePlatformDebugPrivate::UnregisterConsoleCommands()
{
#if !UE_BUILD_SHIPPING
    IConsoleManager& ConsoleManager = IConsoleManager::Get();
    for (IConsoleObject* Object : RegisteredConsoleObjects)
    {
        if (Object)
        {
            ConsoleManager.UnregisterConsoleObject(Object, false);
        }
    }
    RegisteredConsoleObjects.Reset();

    FGamePlatformDebugRegistry& Registry =
        FGamePlatformDebugRegistry::Get();
    for (const FName CommandName : RegisteredCommandNames)
    {
        Registry.UnregisterCommandDescriptor(CommandName);
    }
    RegisteredCommandNames.Reset();
#endif
}
