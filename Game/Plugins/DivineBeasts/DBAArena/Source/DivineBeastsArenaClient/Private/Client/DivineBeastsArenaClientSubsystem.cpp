#include "Client/DivineBeastsArenaClientSubsystem.h"

#include "Catalog/DivineBeastsArenaModeCatalog.h"
#include "DivineBeastsApplicationFlowSubsystem.h"
#include "Extensions/DivineBeastsApplicationFlowExtension.h"

namespace
{
    const FName ArenaFlowExtensionId(TEXT("DivineBeasts.Arena.ApplicationFlow"));

    class FDivineBeastsArenaApplicationFlowExtension final
        : public IDivineBeastsApplicationFlowExtension
    {
    public:
        explicit FDivineBeastsArenaApplicationFlowExtension(
            TWeakObjectPtr<UDivineBeastsArenaClientSubsystem> InOwner)
            : Owner(InOwner)
        {
        }

        virtual void OnEnteredInWorld(
            UDivineBeastsApplicationFlowSubsystem&) override
        {
            if (Owner.IsValid())
            {
                Owner->SetApplicationFlowInWorld(true);
            }
        }

        virtual void OnLeavingInWorld(
            UDivineBeastsApplicationFlowSubsystem&) override
        {
            if (Owner.IsValid())
            {
                Owner->SetApplicationFlowInWorld(false);
            }
        }

    private:
        TWeakObjectPtr<UDivineBeastsArenaClientSubsystem> Owner;
    };
}

void UDivineBeastsArenaClientSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UDivineBeastsApplicationFlowSubsystem>();
    ApplicationFlow =
        GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UDivineBeastsApplicationFlowSubsystem>()
            : nullptr;
    if (!ApplicationFlow)
    {
        return;
    }

    FlowExtension =
        MakeShared<FDivineBeastsArenaApplicationFlowExtension>(
            TWeakObjectPtr<UDivineBeastsArenaClientSubsystem>(this));
    if (!ApplicationFlow->RegisterExtension(
            ArenaFlowExtensionId,
            FlowExtension.ToSharedRef()))
    {
        FlowExtension.Reset();
    }
}

void UDivineBeastsArenaClientSubsystem::Deinitialize()
{
    if (ApplicationFlow && FlowExtension)
    {
        ApplicationFlow->UnregisterExtension(ArenaFlowExtensionId);
    }
    FlowExtension.Reset();
    ApplicationFlow = nullptr;
    bApplicationFlowInWorld = false;
    Super::Deinitialize();
}

TArray<FName> UDivineBeastsArenaClientSubsystem::GetProjectArenaModeIds() const
{
    TArray<FName> Result;
    for (const FDivineBeastsArenaProjectModeSpec& Spec :
         FDivineBeastsArenaModeCatalog::GetAll())
    {
        Result.Add(Spec.ArenaModeId);
    }
    return Result;
}

bool UDivineBeastsArenaClientSubsystem::BuildMatchmakingRequest(
    FName ArenaModeId,
    const FString& PartyId,
    const FString& PreferredRegion,
    const FString& ClientRequestId,
    FGamePlatformArenaMatchmakingRequest& OutRequest,
    FString& OutError) const
{
    const FDivineBeastsArenaProjectModeSpec* Spec =
        FDivineBeastsArenaModeCatalog::Find(ArenaModeId);
    if (!Spec || !Spec->ValidateProduction(OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("ArenaMode未配置为ProductionReady。");
        }
        return false;
    }

    OutRequest = FGamePlatformArenaMatchmakingRequest{};
    OutRequest.ArenaModeId = ArenaModeId;
    OutRequest.PartyId = PartyId;
    OutRequest.PreferredRegion = PreferredRegion;
    OutRequest.ClientRequestId = ClientRequestId;
    return OutRequest.IsValid(OutError);
}

bool UDivineBeastsArenaClientSubsystem::RequestPostMatchReturnToWorld()
{
    return ApplicationFlow != nullptr &&
        ApplicationFlow->RequestPostMatchReturnToWorld();
}
