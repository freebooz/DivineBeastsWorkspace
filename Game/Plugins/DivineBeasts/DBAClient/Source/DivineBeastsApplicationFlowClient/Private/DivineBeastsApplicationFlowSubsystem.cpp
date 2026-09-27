#include "DivineBeastsApplicationFlowSubsystem.h"

#include "Backend/DivineBeastsApplicationBackend.h"
#include "Context/DivineBeastsProjectContext.h"
#include "Creation/GamePlatformCharacterCreationProvider.h"
#include "Features/IModularFeatures.h"
#include "Flow/DivineBeastsFlowNodes.h"
#include "GamePlatformApplicationFlowSubsystem.h"
#include "GamePlatformApplicationFlowTypes.h"
#include "GamePlatformLoadingClientSubsystem.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "GamePlatformSessionClientSubsystem.h"
#include "Identity/DivineBeastsProjectCatalog.h"

namespace
{
    const FName TaskSessionAdmission(TEXT("SessionAdmission"));
    const FName TaskExpectedWorld(TEXT("ExpectedWorld"));
    const FName TaskExpectedExperience(TEXT("ExpectedExperience"));
    const FName TaskCharacterBinding(TEXT("CharacterBinding"));
    const FName TaskGameplayData(TEXT("GameplayData"));
    const FName TaskProjectReadiness(TEXT("ProjectReadiness"));

    IGamePlatformCharacterCreationProvider* GetCharacterCreationProvider()
    {
        const TArray<IGamePlatformCharacterCreationProvider*> Providers =
            IModularFeatures::Get()
                .GetModularFeatureImplementations<
                    IGamePlatformCharacterCreationProvider>(
                    IGamePlatformCharacterCreationProvider::GetModularFeatureName());
        return Providers.Num() == 1 ? Providers[0] : nullptr;
    }

    FGamePlatformFlowNodeDefinition Node(
        FName NodeId,
        std::initializer_list<FName> Next)
    {
        FGamePlatformFlowNodeDefinition Result;
        Result.NodeId = NodeId;
        for (const FName Value : Next)
        {
            Result.AllowedNextNodes.Add(Value);
        }
        return Result;
    }
}

void UDivineBeastsApplicationFlowSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    PlatformFlow =
        GetGameInstance()->GetSubsystem<UGamePlatformApplicationFlowSubsystem>();
    Online =
        GetGameInstance()->GetSubsystem<UGamePlatformOnlineClientSubsystem>();
    Session =
        GetGameInstance()->GetSubsystem<UGamePlatformSessionClientSubsystem>();
    Loading =
        GetGameInstance()->GetSubsystem<UGamePlatformLoadingClientSubsystem>();

    if (PlatformFlow)
    {
        RegisterProjectNodes();
        FlowHandle = PlatformFlow->OnSnapshotChanged().AddUObject(
            this,
            &UDivineBeastsApplicationFlowSubsystem::HandleFlowSnapshot);
    }
    if (Online)
    {
        AuthHandle = Online->OnAuthStateChanged().AddUObject(
            this,
            &UDivineBeastsApplicationFlowSubsystem::HandleAuthSnapshot);
    }
    if (Session)
    {
        SessionHandle = Session->OnSessionChanged().AddUObject(
            this,
            &UDivineBeastsApplicationFlowSubsystem::HandleSessionSnapshot);
    }
    if (Loading)
    {
        LoadingHandle = Loading->OnLoadingChanged().AddUObject(
            this,
            &UDivineBeastsApplicationFlowSubsystem::HandleLoadingSnapshot);
    }

    Backend = MakeShared<FDivineBeastsHttpApplicationBackend>(Online);
    ResetProjection();
}

void UDivineBeastsApplicationFlowSubsystem::Deinitialize()
{
    if (Backend.IsValid())
    {
        Backend->CancelAll();
        Backend.Reset();
    }
    if (Loading && ActiveLoadingOperationId.IsValid())
    {
        Loading->CancelOperation(ActiveLoadingOperationId);
    }
    if (Session)
    {
        Session->CancelTransfer();
    }
    if (PlatformFlow)
    {
        PlatformFlow->InvalidateRun();
        if (FlowHandle.IsValid())
        {
            PlatformFlow->OnSnapshotChanged().Remove(FlowHandle);
        }

        for (const FName NodeId : {
            FDivineBeastsFlowNodes::Boot(),
            FDivineBeastsFlowNodes::Initialize(),
            FDivineBeastsFlowNodes::Authentication(),
            FDivineBeastsFlowNodes::LoadProfile(),
            FDivineBeastsFlowNodes::LoadRoster(),
            FDivineBeastsFlowNodes::CharacterEntry(),
            FDivineBeastsFlowNodes::CreateCharacter(),
            FDivineBeastsFlowNodes::ValidateSelection(),
            FDivineBeastsFlowNodes::RequestWorld(),
            FDivineBeastsFlowNodes::TransferWorld(),
            FDivineBeastsFlowNodes::WorldReady(),
            FDivineBeastsFlowNodes::InWorld(),
            FDivineBeastsFlowNodes::Recovering()})
        {
            PlatformFlow->UnregisterNode(NodeId);
        }
    }
    if (Online && AuthHandle.IsValid())
    {
        Online->OnAuthStateChanged().Remove(AuthHandle);
    }
    if (Session && SessionHandle.IsValid())
    {
        Session->OnSessionChanged().Remove(SessionHandle);
    }
    if (Loading && LoadingHandle.IsValid())
    {
        Loading->OnLoadingChanged().Remove(LoadingHandle);
    }

    Extensions.Reset();
    Super::Deinitialize();
}

bool UDivineBeastsApplicationFlowSubsystem::RegisterProjectNodes()
{
    if (!PlatformFlow)
    {
        return false;
    }

    const TArray<FGamePlatformFlowNodeDefinition> Definitions =
    {
        Node(FDivineBeastsFlowNodes::Boot(),
             {FDivineBeastsFlowNodes::Initialize()}),
        Node(FDivineBeastsFlowNodes::Initialize(),
             {FDivineBeastsFlowNodes::Authentication()}),
        Node(FDivineBeastsFlowNodes::Authentication(),
             {FDivineBeastsFlowNodes::LoadProfile()}),
        Node(FDivineBeastsFlowNodes::LoadProfile(),
             {FDivineBeastsFlowNodes::LoadRoster(),
              FDivineBeastsFlowNodes::Authentication()}),
        Node(FDivineBeastsFlowNodes::LoadRoster(),
             {FDivineBeastsFlowNodes::CharacterEntry(),
              FDivineBeastsFlowNodes::Authentication()}),
        Node(FDivineBeastsFlowNodes::CharacterEntry(),
             {FDivineBeastsFlowNodes::CreateCharacter(),
              FDivineBeastsFlowNodes::ValidateSelection(),
              FDivineBeastsFlowNodes::Authentication()}),
        Node(FDivineBeastsFlowNodes::CreateCharacter(),
             {FDivineBeastsFlowNodes::ValidateSelection(),
              FDivineBeastsFlowNodes::CharacterEntry()}),
        Node(FDivineBeastsFlowNodes::ValidateSelection(),
             {FDivineBeastsFlowNodes::RequestWorld(),
              FDivineBeastsFlowNodes::CharacterEntry()}),
        Node(FDivineBeastsFlowNodes::RequestWorld(),
             {FDivineBeastsFlowNodes::TransferWorld(),
              FDivineBeastsFlowNodes::Recovering(),
              FDivineBeastsFlowNodes::CharacterEntry()}),
        Node(FDivineBeastsFlowNodes::TransferWorld(),
             {FDivineBeastsFlowNodes::WorldReady(),
              FDivineBeastsFlowNodes::Recovering(),
              FDivineBeastsFlowNodes::CharacterEntry()}),
        Node(FDivineBeastsFlowNodes::WorldReady(),
             {FDivineBeastsFlowNodes::InWorld(),
              FDivineBeastsFlowNodes::Recovering()}),
        Node(FDivineBeastsFlowNodes::InWorld(),
             {FDivineBeastsFlowNodes::RequestWorld(),
              FDivineBeastsFlowNodes::Recovering(),
              FDivineBeastsFlowNodes::Authentication()}),
        Node(FDivineBeastsFlowNodes::Recovering(),
             {FDivineBeastsFlowNodes::RequestWorld(),
              FDivineBeastsFlowNodes::CharacterEntry(),
              FDivineBeastsFlowNodes::Authentication()})
    };

    bool bAllRegistered = true;
    for (const FGamePlatformFlowNodeDefinition& Definition : Definitions)
    {
        if (!PlatformFlow->HasNode(Definition.NodeId))
        {
            bAllRegistered &=
                PlatformFlow->RegisterNode(Definition);
        }
    }
    return bAllRegistered;
}

bool UDivineBeastsApplicationFlowSubsystem::StartFlow(bool bTryAutoLogin)
{
    if (!PlatformFlow || !Online || !Session || !Loading || !Backend.IsValid())
    {
        SetError(EDivineBeastsFlowError::FlowNotInitialized);
        return false;
    }

    Backend->CancelAll();
    if (ActiveLoadingOperationId.IsValid())
    {
        Loading->CancelOperation(ActiveLoadingOperationId);
    }
    Session->CancelTransfer();
    PlatformFlow->InvalidateRun();
    ResetProjection();
    RecoveryAttempts = 0;

    FGuid RunId;
    if (!PlatformFlow->StartRun(FDivineBeastsFlowNodes::Boot(), RunId) ||
        !TransitionTo(FDivineBeastsFlowNodes::Initialize()) ||
        !TransitionTo(FDivineBeastsFlowNodes::Authentication()))
    {
        SetError(EDivineBeastsFlowError::FlowNotInitialized);
        return false;
    }

    if (bTryAutoLogin)
    {
        SetBusy(true);
        Online->TryAutoLogin();
    }
    else
    {
        SetBusy(false);
    }
    return true;
}

void UDivineBeastsApplicationFlowSubsystem::LoginWithCredentials(
    const FString& LoginName,
    const FString& Password)
{
    if (!Online || !IsCurrentNode(FDivineBeastsFlowNodes::Authentication()))
    {
        SetError(EDivineBeastsFlowError::FlowNotInitialized);
        return;
    }
    SetBusy(true);
    Online->LoginWithCredentials(LoginName, Password);
}

void UDivineBeastsApplicationFlowSubsystem::HandleAuthSnapshot(
    const FGamePlatformAuthSnapshot& Snapshot)
{
    ViewState.ConnectionSummary =
        StaticEnum<EGamePlatformAuthState>()
            ? StaticEnum<EGamePlatformAuthState>()->GetNameStringByValue(
                static_cast<int64>(Snapshot.State))
            : FString();

    if (Snapshot.State == EGamePlatformAuthState::Authenticated &&
        IsCurrentNode(FDivineBeastsFlowNodes::Authentication()))
    {
        SetError(EDivineBeastsFlowError::None);
        if (TransitionTo(FDivineBeastsFlowNodes::LoadProfile()))
        {
            BeginLoadProfile();
        }
        return;
    }

    if (Snapshot.State == EGamePlatformAuthState::Failed)
    {
        SetBusy(false);
        EDivineBeastsFlowError FlowError =
            EDivineBeastsFlowError::AuthenticationFailed;
        switch (Snapshot.Error)
        {
        case EGamePlatformAuthError::InvalidCredentials:
            FlowError = EDivineBeastsFlowError::InvalidCredentials;
            break;
        case EGamePlatformAuthError::AccountLocked:
            FlowError = EDivineBeastsFlowError::AccountLocked;
            break;
        case EGamePlatformAuthError::Maintenance:
            FlowError = EDivineBeastsFlowError::Maintenance;
            break;
        case EGamePlatformAuthError::NetworkUnavailable:
        case EGamePlatformAuthError::ProviderUnavailable:
            FlowError = EDivineBeastsFlowError::NetworkUnavailable;
            break;
        case EGamePlatformAuthError::AuthExpired:
            FlowError = EDivineBeastsFlowError::AuthExpired;
            break;
        case EGamePlatformAuthError::ContractIncompatible:
            FlowError = EDivineBeastsFlowError::ContractIncompatible;
            break;
        case EGamePlatformAuthError::Cancelled:
            FlowError = EDivineBeastsFlowError::Cancelled;
            break;
        case EGamePlatformAuthError::TimedOut:
            FlowError = EDivineBeastsFlowError::TimedOut;
            break;
        default:
            break;
        }
        SetError(FlowError);
        return;
    }

    if (Snapshot.State == EGamePlatformAuthState::LoggedOut &&
        bRestartAfterLogout)
    {
        bRestartAfterLogout = false;
        StartFlow(true);
        return;
    }

    BroadcastView();
}

void UDivineBeastsApplicationFlowSubsystem::BeginLoadProfile()
{
    if (!Backend.IsValid() || !PlatformFlow)
    {
        SetError(EDivineBeastsFlowError::ProfileUnavailable);
        return;
    }

    const FGamePlatformFlowOperationToken Token =
        PlatformFlow->BeginOperation();
    SetBusy(true);

    Backend->LoadProfile(
        [WeakThis = TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem>(this),
         Token](
            bool bSuccess,
            FDivineBeastsPlayerProfile Profile,
            EDivineBeastsFlowError Error)
        {
            if (!WeakThis.IsValid() ||
                !WeakThis->PlatformFlow ||
                !WeakThis->PlatformFlow->IsOperationCurrent(Token))
            {
                return;
            }
            if (!bSuccess)
            {
                WeakThis->SetBusy(false);
                WeakThis->SetError(Error);
                return;
            }

            WeakThis->ViewState.Profile = MoveTemp(Profile);
            if (WeakThis->TransitionTo(
                    FDivineBeastsFlowNodes::LoadRoster()))
            {
                WeakThis->BeginLoadRoster();
            }
        });
}

void UDivineBeastsApplicationFlowSubsystem::BeginLoadRoster()
{
    const FGamePlatformFlowOperationToken Token =
        PlatformFlow->BeginOperation();

    Backend->LoadRoster(
        [WeakThis = TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem>(this),
         Token](
            bool bSuccess,
            TArray<FDivineBeastsCharacterSummary> Roster,
            EDivineBeastsFlowError Error)
        {
            if (!WeakThis.IsValid() ||
                !WeakThis->PlatformFlow ||
                !WeakThis->PlatformFlow->IsOperationCurrent(Token))
            {
                return;
            }
            if (!bSuccess)
            {
                WeakThis->SetBusy(false);
                WeakThis->SetError(Error);
                return;
            }

            WeakThis->ViewState.CharacterRoster = MoveTemp(Roster);
            WeakThis->SetBusy(false);
            WeakThis->TransitionTo(
                FDivineBeastsFlowNodes::CharacterEntry());
        });
}

bool UDivineBeastsApplicationFlowSubsystem::GetCharacterCreationHeroes(
    TArray<FGamePlatformCharacterCreationHeroDescriptor>& OutHeroes) const
{
    OutHeroes.Reset();
    IGamePlatformCharacterCreationProvider* Provider =
        GetCharacterCreationProvider();
    if (!Provider)
    {
        return false;
    }
    Provider->GetCreateableHeroes(OutHeroes);
    return !OutHeroes.IsEmpty();
}

bool UDivineBeastsApplicationFlowSubsystem::SubmitCharacterCreateDraft(
    const FDivineBeastsCharacterCreateDraft& Draft)
{
    if (!Draft.IsLocallyValid() ||
        !IsCurrentNode(FDivineBeastsFlowNodes::CharacterEntry()))
    {
        return false;
    }

    IGamePlatformCharacterCreationProvider* CreationProvider =
        GetCharacterCreationProvider();
    if (!CreationProvider)
    {
        SetError(EDivineBeastsFlowError::HeroCatalogUnavailable);
        return false;
    }

    TArray<FGamePlatformCharacterCreationHeroDescriptor> CreateableHeroes;
    CreationProvider->GetCreateableHeroes(CreateableHeroes);
    const bool bHeroAvailable = CreateableHeroes.ContainsByPredicate(
        [&Draft](const FGamePlatformCharacterCreationHeroDescriptor& Descriptor)
        {
            return Descriptor.HeroDefinitionId == Draft.HeroDefinitionId;
        });
    if (!bHeroAvailable)
    {
        SetError(EDivineBeastsFlowError::HeroCatalogUnavailable);
        return false;
    }

    FString AppearanceError;
    if (!CreationProvider->ValidateCreationDraft(
            Draft.HeroDefinitionId,
            Draft.AppearanceSelection,
            AppearanceError))
    {
        SetError(EDivineBeastsFlowError::InvalidAppearance);
        return false;
    }

    if (!TransitionTo(FDivineBeastsFlowNodes::CreateCharacter()))
    {
        return false;
    }

    const FGamePlatformFlowOperationToken Token =
        PlatformFlow->BeginOperation();
    if (!Token.IsValid())
    {
        SetError(EDivineBeastsFlowError::FlowNotInitialized);
        return false;
    }

    SetBusy(true);
    Backend->CreateCharacter(
        Draft,
        Token.OperationId,
        [WeakThis = TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem>(this),
         Token](
            bool bSuccess,
            FDivineBeastsCharacterSummary Character,
            EDivineBeastsFlowError Error)
        {
            if (!WeakThis.IsValid() ||
                !WeakThis->PlatformFlow ||
                !WeakThis->PlatformFlow->IsOperationCurrent(Token))
            {
                return;
            }
            if (!bSuccess)
            {
                WeakThis->SetBusy(false);
                WeakThis->SetError(Error);
                WeakThis->TransitionTo(
                    FDivineBeastsFlowNodes::CharacterEntry());
                return;
            }

            WeakThis->ViewState.CharacterRoster.Add(Character);
            WeakThis->BeginSelection(Character);
        });
    return true;
}

bool UDivineBeastsApplicationFlowSubsystem::SelectPersistentCharacter(
    const FString& CharacterId)
{
    if (!IsCurrentNode(FDivineBeastsFlowNodes::CharacterEntry()))
    {
        return false;
    }
    const FDivineBeastsCharacterSummary* Character =
        ViewState.CharacterRoster.FindByPredicate(
            [&CharacterId](const FDivineBeastsCharacterSummary& Candidate)
            {
                return Candidate.CharacterId == CharacterId;
            });
    if (!Character)
    {
        SetError(EDivineBeastsFlowError::CharacterNotFound);
        return false;
    }
    BeginSelection(*Character);
    return true;
}

void UDivineBeastsApplicationFlowSubsystem::BeginSelection(
    const FDivineBeastsCharacterSummary& Character)
{
    if (!TransitionTo(FDivineBeastsFlowNodes::ValidateSelection()))
    {
        SetBusy(false);
        SetError(EDivineBeastsFlowError::CharacterSelectionRejected);
        return;
    }

    const FGamePlatformFlowOperationToken Token =
        PlatformFlow->BeginOperation();
    SetBusy(true);
    Backend->SelectPersistentCharacter(
        Character.CharacterId,
        Character.CharacterRevision,
        Token.OperationId,
        [WeakThis = TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem>(this),
         Token](
            bool bSuccess,
            FDivineBeastsValidatedSelection Selection,
            EDivineBeastsFlowError Error)
        {
            if (!WeakThis.IsValid() ||
                !WeakThis->PlatformFlow ||
                !WeakThis->PlatformFlow->IsOperationCurrent(Token))
            {
                return;
            }
            if (!bSuccess)
            {
                WeakThis->SetBusy(false);
                WeakThis->SetError(Error);
                WeakThis->TransitionTo(
                    FDivineBeastsFlowNodes::CharacterEntry());
                return;
            }
            WeakThis->HandleValidatedSelection(Selection);
        });
}

void UDivineBeastsApplicationFlowSubsystem::HandleValidatedSelection(
    const FDivineBeastsValidatedSelection& Selection)
{
    ViewState.SelectedCharacter = Selection.Character;
    ViewState.bHasSelectedCharacter = true;
    ViewState.Profile.ProfileRevision = Selection.ProfileRevision;
    SetError(EDivineBeastsFlowError::None);
    RequestWorldAssignment(DefaultExperienceForOnboarding());
}

FName UDivineBeastsApplicationFlowSubsystem::DefaultExperienceForOnboarding()
    const
{
    return ViewState.Profile.OnboardingState ==
        EDivineBeastsOnboardingState::OnboardingComplete
        ? FName(TEXT("Experience.OpenWorld.Hub"))
        : FName(TEXT("Experience.Village.Tutorial"));
}

bool UDivineBeastsApplicationFlowSubsystem::IsExperienceAllowedForCurrentProfile(
    FName ExperienceId) const
{
    FName ServerRole = NAME_None;
    if (!FDivineBeastsProjectCatalog::TryGetServerRoleForExperience(
            ExperienceId,
            ServerRole))
    {
        return false;
    }

    const bool bComplete =
        ViewState.Profile.OnboardingState ==
        EDivineBeastsOnboardingState::OnboardingComplete;
    if (bComplete)
    {
        return ServerRole ==
            FDivineBeastsProjectCatalog::GetOpenWorldServerRole();
    }
    return ServerRole ==
        FDivineBeastsProjectCatalog::GetVillageServerRole();
}

bool UDivineBeastsApplicationFlowSubsystem::RequestWorldAssignment(
    FName DesiredExperienceId,
    const FString& PreferredRegion)
{
    if (!ViewState.bHasSelectedCharacter ||
        !Backend.IsValid() ||
        !IsExperienceAllowedForCurrentProfile(DesiredExperienceId))
    {
        SetError(EDivineBeastsFlowError::ExperienceNotAllowed);
        return false;
    }

    const FName Current =
        PlatformFlow ? PlatformFlow->GetSnapshot().CurrentNodeId : NAME_None;
    if (Current != FDivineBeastsFlowNodes::ValidateSelection() &&
        Current != FDivineBeastsFlowNodes::InWorld() &&
        Current != FDivineBeastsFlowNodes::Recovering())
    {
        return false;
    }

    if (Current == FDivineBeastsFlowNodes::InWorld())
    {
        NotifyExtensionsLeavingWorld();
    }

    if (!TransitionTo(FDivineBeastsFlowNodes::RequestWorld()))
    {
        return false;
    }

    const FGamePlatformFlowOperationToken Token =
        PlatformFlow->BeginOperation();
    SetBusy(true);
    Backend->RequestWorldAssignment(
        ViewState.SelectedCharacter.CharacterId,
        ViewState.SelectedCharacter.CharacterRevision,
        DesiredExperienceId,
        PreferredRegion,
        Token.OperationId,
        [WeakThis = TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem>(this),
         Token](
            bool bSuccess,
            FDivineBeastsWorldAssignmentPayload Assignment,
            EDivineBeastsFlowError Error)
        {
            if (!WeakThis.IsValid() ||
                !WeakThis->PlatformFlow ||
                !WeakThis->PlatformFlow->IsOperationCurrent(Token))
            {
                return;
            }
            if (!bSuccess)
            {
                WeakThis->SetBusy(false);
                WeakThis->BeginRecovery(Error);
                return;
            }

            FName ExpectedRole = NAME_None;
            const bool bMappingValid =
                FDivineBeastsProjectCatalog::TryGetServerRoleForExperience(
                    Assignment.Summary.ExperienceId,
                    ExpectedRole) &&
                ExpectedRole == Assignment.Summary.ServerRoleId &&
                Assignment.Summary.CharacterId ==
                    WeakThis->ViewState.SelectedCharacter.CharacterId;
            if (!bMappingValid)
            {
                WeakThis->SetBusy(false);
                WeakThis->BeginRecovery(
                    EDivineBeastsFlowError::WorldMismatch);
                return;
            }

            WeakThis->ViewState.Assignment = Assignment.Summary;
            WeakThis->PendingEndpoint = MoveTemp(Assignment.Endpoint);
            WeakThis->PendingTransferTicket =
                MoveTemp(Assignment.TransferTicket);
            WeakThis->TransitionTo(
                FDivineBeastsFlowNodes::TransferWorld());
            WeakThis->BeginLoadingForAssignment(
                WeakThis->PendingEndpoint,
                WeakThis->PendingTransferTicket);
        });
    return true;
}

void UDivineBeastsApplicationFlowSubsystem::BeginLoadingForAssignment(
    const FString& Endpoint,
    const FString& TransferTicket)
{
    const TArray<FName> RequiredTasks =
    {
        TaskSessionAdmission,
        TaskExpectedWorld,
        TaskExpectedExperience,
        TaskCharacterBinding,
        TaskGameplayData,
        TaskProjectReadiness
    };
    ActiveLoadingOperationId =
        Loading->BeginOperation(RequiredTasks);

    ActiveTransferOperationId = FGuid::NewGuid();
    FGamePlatformSessionTransferRequest Request;
    Request.TransferOperationId = ActiveTransferOperationId;
    Request.AssignmentId = ViewState.Assignment.AssignmentId;
    Request.GameServerId = ViewState.Assignment.GameServerId;
    Request.ServerRoleId = ViewState.Assignment.ServerRoleId;
    Request.ExperienceId = ViewState.Assignment.ExperienceId;
    Request.Endpoint = Endpoint;
    Request.TransferTicket = TransferTicket;
    Request.TicketId = ViewState.Assignment.TicketId;
    Request.CharacterId = ViewState.Assignment.CharacterId;
    Request.SessionId = ViewState.Assignment.SessionId;

    // 敏感票据只在这里交给Session；完成调用后立即清除项目层副本。
    const bool bTransferStarted = Session->BeginTransfer(Request);
    PendingTransferTicket.Reset();
    PendingEndpoint.Reset();

    if (!bTransferStarted)
    {
        BeginRecovery(EDivineBeastsFlowError::TravelFailed);
    }
}

void UDivineBeastsApplicationFlowSubsystem::NotifyWorldObserved(
    FName ExperienceId,
    FName WorldId)
{
    if (WorldId.IsNone() ||
        ExperienceId != ViewState.Assignment.ExperienceId)
    {
        SetError(EDivineBeastsFlowError::WorldMismatch);
        return;
    }

    MarkLoadingTaskReady(TaskExpectedWorld);
    MarkLoadingTaskReady(TaskExpectedExperience);
    ViewState.Assignment.WorldId = WorldId;
}

void UDivineBeastsApplicationFlowSubsystem::NotifyCharacterBindingReady()
{
    MarkLoadingTaskReady(TaskCharacterBinding);
}

void UDivineBeastsApplicationFlowSubsystem::NotifyGameplayDataReady()
{
    MarkLoadingTaskReady(TaskGameplayData);
}

void UDivineBeastsApplicationFlowSubsystem::NotifyProjectReadiness()
{
    MarkLoadingTaskReady(TaskProjectReadiness);
}

void UDivineBeastsApplicationFlowSubsystem::MarkLoadingTaskReady(FName TaskId)
{
    if (Loading && ActiveLoadingOperationId.IsValid())
    {
        Loading->SetTaskState(
            ActiveLoadingOperationId,
            TaskId,
            EGamePlatformLoadingTaskState::Ready);
    }
}

void UDivineBeastsApplicationFlowSubsystem::HandleSessionSnapshot(
    const FGamePlatformSessionSnapshot& Snapshot)
{
    if (Snapshot.TransferOperationId != ActiveTransferOperationId)
    {
        return;
    }

    ViewState.ConnectionSummary =
        StaticEnum<EGamePlatformSessionTransferState>()
            ? StaticEnum<EGamePlatformSessionTransferState>()
                ->GetNameStringByValue(static_cast<int64>(Snapshot.State))
            : FString();

    if (Snapshot.State == EGamePlatformSessionTransferState::Admitted)
    {
        MarkLoadingTaskReady(TaskSessionAdmission);
    }
    else if (Snapshot.State == EGamePlatformSessionTransferState::Failed)
    {
        BeginRecovery(EDivineBeastsFlowError::AdmissionFailed);
    }
    BroadcastView();
}

void UDivineBeastsApplicationFlowSubsystem::HandleLoadingSnapshot(
    const FGamePlatformLoadingSnapshot& Snapshot)
{
    if (Snapshot.OperationId != ActiveLoadingOperationId)
    {
        return;
    }

    ViewState.LoadingSummary =
        StaticEnum<EGamePlatformLoadingOperationState>()
            ? StaticEnum<EGamePlatformLoadingOperationState>()
                ->GetNameStringByValue(static_cast<int64>(Snapshot.State))
            : FString();

    if (Snapshot.State == EGamePlatformLoadingOperationState::Ready)
    {
        TryCompleteWorldReady();
    }
    else if (Snapshot.State == EGamePlatformLoadingOperationState::Failed)
    {
        BeginRecovery(
            EDivineBeastsFlowError::WorldReadinessTimedOut);
    }
    BroadcastView();
}

void UDivineBeastsApplicationFlowSubsystem::TryCompleteWorldReady()
{
    if (!Loading ||
        !Session ||
        !Loading->IsReady(ActiveLoadingOperationId) ||
        Session->GetSnapshot().State !=
            EGamePlatformSessionTransferState::Admitted ||
        !IsCurrentNode(FDivineBeastsFlowNodes::TransferWorld()))
    {
        return;
    }

    if (!TransitionTo(FDivineBeastsFlowNodes::WorldReady()) ||
        !TransitionTo(FDivineBeastsFlowNodes::InWorld()))
    {
        BeginRecovery(EDivineBeastsFlowError::WorldMismatch);
        return;
    }

    RecoveryAttempts = 0;
    SetBusy(false);
    SetError(EDivineBeastsFlowError::None);
    NotifyExtensionsEnteredWorld();
}

void UDivineBeastsApplicationFlowSubsystem::BeginRecovery(
    EDivineBeastsFlowError Error)
{
    if (!PlatformFlow || !ViewState.bHasSelectedCharacter)
    {
        SetError(Error);
        return;
    }

    SetError(Error);
    SetBusy(true);
    ++RecoveryAttempts;

    if (RecoveryAttempts > MaxRecoveryAttempts)
    {
        SetBusy(false);
        SetError(EDivineBeastsFlowError::ReconnectExhausted);
        TransitionTo(FDivineBeastsFlowNodes::CharacterEntry());
        return;
    }

    if (ActiveLoadingOperationId.IsValid())
    {
        Loading->CancelOperation(ActiveLoadingOperationId);
    }
    Session->BeginReconnect(MaxRecoveryAttempts);

    if (!IsCurrentNode(FDivineBeastsFlowNodes::Recovering()))
    {
        TransitionTo(FDivineBeastsFlowNodes::Recovering());
    }

    // Ticket不能重用；恢复永远重新请求Assignment和新票据。
    RequestWorldAssignment(
        ViewState.Assignment.ExperienceId.IsNone()
            ? DefaultExperienceForOnboarding()
            : ViewState.Assignment.ExperienceId);
}

bool UDivineBeastsApplicationFlowSubsystem::RequestPostMatchReturnToWorld()
{
    if (!IsCurrentNode(FDivineBeastsFlowNodes::InWorld()) ||
        !ViewState.bHasSelectedCharacter)
    {
        return false;
    }

    // 不缓存旧OpenWorld Endpoint；重新向后端请求安全Assignment。
    return RequestWorldAssignment(
        FName(TEXT("Experience.OpenWorld.Hub")));
}

void UDivineBeastsApplicationFlowSubsystem::LogoutAndRestart()
{
    bRestartAfterLogout = true;

    if (Backend.IsValid())
    {
        Backend->CancelAll();
    }
    if (Loading && ActiveLoadingOperationId.IsValid())
    {
        Loading->CancelOperation(ActiveLoadingOperationId);
    }
    if (Session)
    {
        Session->CancelTransfer();
    }
    if (PlatformFlow)
    {
        PlatformFlow->InvalidateRun();
    }

    ResetProjection();

    if (Online)
    {
        Online->Logout();
    }
    else
    {
        bRestartAfterLogout = false;
    }
}

bool UDivineBeastsApplicationFlowSubsystem::RegisterExtension(
    FName ExtensionId,
    TSharedRef<IDivineBeastsApplicationFlowExtension> Extension)
{
    if (ExtensionId.IsNone() || Extensions.Contains(ExtensionId))
    {
        return false;
    }
    Extensions.Add(ExtensionId, Extension);
    return true;
}

bool UDivineBeastsApplicationFlowSubsystem::UnregisterExtension(FName ExtensionId)
{
    return Extensions.Remove(ExtensionId) > 0;
}

void UDivineBeastsApplicationFlowSubsystem::NotifyExtensionsEnteredWorld()
{
    TArray<FName> Ids;
    Extensions.GetKeys(Ids);
    Ids.Sort([](const FName& A, const FName& B)
    {
        return A.LexicalLess(B);
    });
    for (const FName Id : Ids)
    {
        const TSharedPtr<IDivineBeastsApplicationFlowExtension>* Extension =
            Extensions.Find(Id);
        if (Extension && Extension->IsValid())
        {
            (*Extension)->OnEnteredInWorld(*this);
        }
    }
}

void UDivineBeastsApplicationFlowSubsystem::NotifyExtensionsLeavingWorld()
{
    TArray<FName> Ids;
    Extensions.GetKeys(Ids);
    Ids.Sort([](const FName& A, const FName& B)
    {
        return A.LexicalLess(B);
    });
    for (const FName Id : Ids)
    {
        const TSharedPtr<IDivineBeastsApplicationFlowExtension>* Extension =
            Extensions.Find(Id);
        if (Extension && Extension->IsValid())
        {
            (*Extension)->OnLeavingInWorld(*this);
        }
    }
}

bool UDivineBeastsApplicationFlowSubsystem::TransitionTo(FName NodeId)
{
    return PlatformFlow && PlatformFlow->TransitionTo(NodeId);
}

bool UDivineBeastsApplicationFlowSubsystem::IsCurrentNode(FName NodeId) const
{
    return PlatformFlow &&
        PlatformFlow->GetSnapshot().bActive &&
        PlatformFlow->GetSnapshot().CurrentNodeId == NodeId;
}

void UDivineBeastsApplicationFlowSubsystem::HandleFlowSnapshot(
    const FGamePlatformFlowSnapshot& Snapshot)
{
    ViewState.FlowRunId = Snapshot.FlowRunId;
    ViewState.CurrentStep = Snapshot.CurrentNodeId;
    ViewState.NodeGeneration = Snapshot.NodeGeneration;
    RefreshAllowedActions();
    BroadcastView();
}

void UDivineBeastsApplicationFlowSubsystem::ResetProjection()
{
    ViewState = FDivineBeastsFlowViewState();
    ActiveLoadingOperationId.Invalidate();
    ActiveTransferOperationId.Invalidate();
    PendingEndpoint.Reset();
    PendingTransferTicket.Reset();
    RecoveryAttempts = 0;
    RefreshAllowedActions();
    BroadcastView();
}

void UDivineBeastsApplicationFlowSubsystem::SetError(
    EDivineBeastsFlowError Error)
{
    ViewState.Error = Error;
    RefreshAllowedActions();
    BroadcastView();
}

void UDivineBeastsApplicationFlowSubsystem::SetBusy(bool bBusy)
{
    ViewState.bBusy = bBusy;
    RefreshAllowedActions();
    BroadcastView();
}

void UDivineBeastsApplicationFlowSubsystem::RefreshAllowedActions()
{
    ViewState.AllowedActions.Reset();
    if (ViewState.bBusy)
    {
        ViewState.AllowedActions.Add(EDivineBeastsFlowAction::Logout);
        return;
    }

    if (ViewState.CurrentStep == FDivineBeastsFlowNodes::Authentication())
    {
        ViewState.AllowedActions.Add(EDivineBeastsFlowAction::TryAutoLogin);
        ViewState.AllowedActions.Add(EDivineBeastsFlowAction::Login);
    }
    else if (ViewState.CurrentStep == FDivineBeastsFlowNodes::CharacterEntry())
    {
        ViewState.AllowedActions.Add(
            EDivineBeastsFlowAction::CreateCharacter);
        if (!ViewState.CharacterRoster.IsEmpty())
        {
            ViewState.AllowedActions.Add(
                EDivineBeastsFlowAction::SelectPersistentCharacter);
        }
    }

    if (ViewState.Error != EDivineBeastsFlowError::None)
    {
        ViewState.AllowedActions.Add(EDivineBeastsFlowAction::Retry);
    }
    ViewState.AllowedActions.Add(EDivineBeastsFlowAction::Logout);
}

void UDivineBeastsApplicationFlowSubsystem::BroadcastView()
{
    ViewStateChanged.Broadcast(ViewState);
}
