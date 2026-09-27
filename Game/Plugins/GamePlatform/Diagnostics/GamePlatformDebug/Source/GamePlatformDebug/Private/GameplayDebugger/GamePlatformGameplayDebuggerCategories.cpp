#include "GamePlatformDebugPrivate.h"

#if WITH_GAMEPLAY_DEBUGGER && !UE_BUILD_SHIPPING

#include "GameplayDebugger.h"
#include "GameplayDebuggerCategory.h"
#include "Registry/GamePlatformDebugRegistry.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
    class FGamePlatformGameplayDebuggerCategory final
        : public FGameplayDebuggerCategory
    {
    public:
        explicit FGamePlatformGameplayDebuggerCategory(FName InProviderId)
            : ProviderId(InProviderId)
        {
        }

        virtual void CollectData(
            APlayerController* OwnerPC,
            AActor* DebugActor) override
        {
            UWorld* World = OwnerPC
                ? OwnerPC->GetWorld()
                : (DebugActor ? DebugActor->GetWorld() : nullptr);
            if (!World)
            {
                return;
            }

            FGamePlatformDebugCollectContext Context;
            Context.World = World;
            Context.Target = DebugActor
                ? DebugActor
                : (OwnerPC ? OwnerPC->GetPawn() : nullptr);
            Context.SourceView =
                World->GetNetMode() == NM_Client
                    ? EGamePlatformDebugSourceView::Client
                    : EGamePlatformDebugSourceView::Server;
            Context.RequestId = FGuid::NewGuid();
            Context.bAllowExpensive = false;

            FGamePlatformDebugSnapshot Snapshot;
            FGamePlatformDebugRegistry& Registry =
                FGamePlatformDebugRegistry::Get();

            const bool bCollected =
                Registry.CollectSnapshot(
                    ProviderId,
                    Context,
                    Snapshot);

            AddTextLine(FString::Printf(
                TEXT("{green}GP.%s {white}revision=%llu view=%s"),
                *ProviderId.ToString(),
                Snapshot.Revision,
                Context.SourceView == EGamePlatformDebugSourceView::Server
                    ? TEXT("Server")
                    : TEXT("Client")));

            if (!bCollected && !Snapshot.FailureReason.IsEmpty())
            {
                AddTextLine(FString::Printf(
                    TEXT("{yellow}%s"),
                    *Snapshot.FailureReason));
                return;
            }

            for (const FGamePlatformDebugField& Field : Snapshot.Fields)
            {
                AddTextLine(FString::Printf(
                    TEXT("{white}%s: {cyan}%s"),
                    *Field.DisplayName,
                    *Field.Value));
            }

            if (Snapshot.bTruncated)
            {
                AddTextLine(TEXT("{yellow}[snapshot truncated by safety limits]"));
            }
        }

        virtual void DrawData(
            APlayerController*,
            FGameplayDebuggerCanvasContext& CanvasContext) override
        {
            for (const FString& Line : GetReplicatedLinesCopy())
            {
                CanvasContext.Printf(TEXT("%s"), *Line);
            }
        }

    private:
        FName ProviderId;
    };

    TArray<FName> RegisteredCategories;

    void RegisterCategory(
        IGameplayDebugger& GameplayDebugger,
        FName CategoryName,
        FName ProviderId,
        int32 Slot)
    {
        GameplayDebugger.RegisterCategory(
            CategoryName,
            IGameplayDebugger::FOnGetCategory::CreateLambda(
                [ProviderId]()
                {
                    return MakeShareable(
                        new FGamePlatformGameplayDebuggerCategory(ProviderId));
                }),
            EGameplayDebuggerCategoryState::EnabledInGameAndSimulate,
            Slot);

        RegisteredCategories.Add(CategoryName);
    }
}

void GamePlatformDebugPrivate::RegisterGameplayDebuggerCategories()
{
    IGameplayDebugger& GameplayDebugger = IGameplayDebugger::Get();

    RegisterCategory(GameplayDebugger, TEXT("GP.World"), TEXT("World"), 0);
    RegisterCategory(GameplayDebugger, TEXT("GP.Character"), TEXT("Character"), 1);
    RegisterCategory(GameplayDebugger, TEXT("GP.Ability"), TEXT("Ability"), 2);
    RegisterCategory(GameplayDebugger, TEXT("GP.Combat"), TEXT("Combat"), 3);
    RegisterCategory(GameplayDebugger, TEXT("GP.AI"), TEXT("AI"), 4);
    RegisterCategory(GameplayDebugger, TEXT("GP.Navigation"), TEXT("Navigation"), 5);
    RegisterCategory(GameplayDebugger, TEXT("GP.Network"), TEXT("Network"), 6);
    RegisterCategory(GameplayDebugger, TEXT("GP.Session"), TEXT("Session"), 7);

    GameplayDebugger.NotifyCategoriesChanged();
}

void GamePlatformDebugPrivate::UnregisterGameplayDebuggerCategories()
{
    if (!IGameplayDebugger::IsAvailable())
    {
        RegisteredCategories.Reset();
        return;
    }

    IGameplayDebugger& GameplayDebugger = IGameplayDebugger::Get();
    for (const FName Category : RegisteredCategories)
    {
        GameplayDebugger.UnregisterCategory(Category);
    }
    RegisteredCategories.Reset();
    GameplayDebugger.NotifyCategoriesChanged();
}

#else

void GamePlatformDebugPrivate::RegisterGameplayDebuggerCategories()
{
}

void GamePlatformDebugPrivate::UnregisterGameplayDebuggerCategories()
{
}

#endif
