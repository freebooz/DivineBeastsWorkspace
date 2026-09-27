#include "DivineBeastsApplicationFlowSubsystem.h"

#include "Backend/DivineBeastsApplicationBackend.h"
#include "Context/DivineBeastsProjectContext.h"
#include "Creation/GamePlatformCharacterCreationProvider.h"
#include "Features/IModularFeatures.h"
#include "Flow/DivineBeastsFlowNodes.h"
#include "GamePlatformApplicationFlowSubsystem.h"
#include "GamePlatformApplicationFlowTypes.h"
#include "Interfaces/IGamePlatformLoadingService.h"
#include "Interfaces/IGamePlatformLoadingTask.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "GamePlatformSessionClientSubsystem.h"
#include "Identity/DivineBeastsProjectCatalog.h"
#include "Definitions/DivineBeastsWorldDefinition.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Loading/DivineBeastsReadinessFacts.h"
#include "UObject/Package.h"
#include <string>

namespace
{
    const FName TaskSessionAdmission(TEXT("SessionAdmission"));
    const FName TaskExpectedWorld(TEXT("ExpectedWorld"));
    const FName TaskExpectedExperience(TEXT("ExpectedExperience"));
    const FName TaskCharacterBinding(TEXT("CharacterBinding"));
    const FName TaskGameplayData(TEXT("GameplayData"));
    const FName TaskProjectReadiness(TEXT("ProjectReadiness"));
    const FName ReadinessTaskType(TEXT("DivineBeastsReadinessFact"));
    const FName WorldDefinitionTaskType(TEXT("DivineBeastsWorldDefinition"));

    bool TryGetReadinessFact(FName TaskId, DivineBeastsLoading::Fact& OutFact)
    {
        if (TaskId == TaskSessionAdmission) { OutFact = DivineBeastsLoading::Fact::SessionAdmission; return true; }
        if (TaskId == TaskExpectedWorld) { OutFact = DivineBeastsLoading::Fact::ExpectedWorld; return true; }
        if (TaskId == TaskExpectedExperience) { OutFact = DivineBeastsLoading::Fact::ExpectedExperience; return true; }
        if (TaskId == TaskCharacterBinding) { OutFact = DivineBeastsLoading::Fact::CharacterBinding; return true; }
        if (TaskId == TaskGameplayData) { OutFact = DivineBeastsLoading::Fact::GameplayData; return true; }
        if (TaskId == TaskProjectReadiness) { OutFact = DivineBeastsLoading::Fact::ProjectReadiness; return true; }
        return false;
    }

    std::string ToUtf8(const FString& Value)
    {
        FTCHARToUTF8 Converted(*Value);
        return std::string(Converted.Get(), static_cast<size_t>(Converted.Length()));
    }

    FGamePlatformLoadingTaskUpdate FailedLoadingUpdate(FName Error)
    {
        return {EGamePlatformLoadingTaskUpdate::Failed, 0.0, Error};
    }

    FGamePlatformResult LoadingFailure(FName Code, const TCHAR* Message)
    {
        return FGamePlatformResult::Failure(Code, Message);
    }
}

/** 本次世界切换的事实和弱对象；不持有地图或会话对象，也不充当流程执行器。 */
class FDivineBeastsProjectLoadingContext final
{
    public:
        FDivineBeastsProjectLoadingContext(UGameInstance& InInstance, const FGuid& InObservationId,
            FName InExpectedWorldId, FName InExpectedExperienceId)
            : Instance(&InInstance), ObservationId(InObservationId), ExpectedWorldId(InExpectedWorldId),
              ExpectedExperienceId(InExpectedExperienceId),
              Facts(MakeShared<DivineBeastsLoading::ReadinessFacts>(
                  ToUtf8(InObservationId.ToString()), ToUtf8(InExpectedWorldId.ToString()), ToUtf8(InExpectedExperienceId.ToString()))) {}

        bool IsActiveFor(const FGuid& Id) const { return Facts->Accepts(ToUtf8(Id.ToString())); }
        bool Observe(FGuid Id, DivineBeastsLoading::Fact Fact) { return Facts->Observe(ToUtf8(Id.ToString()), Fact); }
        bool Has(DivineBeastsLoading::Fact Fact) const { return Facts->Has(Fact); }
        bool AllReady() const { return Facts->AllReady(); }
        bool HasObservedWorld() const { return Has(DivineBeastsLoading::Fact::ExpectedWorld); }
        bool HasWorldDefinition() const { return !TargetWorldPackage.IsEmpty(); }
        void Invalidate() { Facts->Invalidate(); ObservedWorld.Reset(); }

        bool SetWorldDefinition(const UDivineBeastsWorldDefinition& Definition)
        {
            FGamePlatformId ExpectedWorld;
            if (!FGamePlatformId::TryParse(ExpectedWorldId.ToString(), ExpectedWorld) ||
                Definition.LogicalId != ExpectedWorld || !Definition.ValidateDefinition().IsSuccess() ||
                Definition.DefaultExperienceId.LogicalVersion != 1 ||
                FName(*(Definition.DefaultExperienceId.Namespace + TEXT(".") + Definition.DefaultExperienceId.Name)) != ExpectedExperienceId)
            {
                return false;
            }
            const FString Package = Definition.MapIdentity.ToSoftObjectPath().GetLongPackageName();
            if (Package.IsEmpty() || !FPackageName::IsValidLongPackageName(Package)) { return false; }
            if (ObservedWorld.IsValid() &&
                UWorld::RemovePIEPrefix(ObservedWorld->GetOutermost()->GetName()) != Package) { return false; }
            TargetWorldPackage = Package;
            return true;
        }

        bool IsExpectedWorld(FGuid Id, FName ExperienceId, FName WorldId, UWorld* World) const
        {
            UGameInstance* Owner = Instance.Get();
            if (!IsActiveFor(Id) || !Owner || !World || ExperienceId != ExpectedExperienceId ||
                WorldId != ExpectedWorldId || World->GetGameInstance() != Owner || Owner->GetWorld() != World ||
                !World->HasBegunPlay() || World->bIsTearingDown || !World->GetPackage())
            {
                return false;
            }
            if (HasWorldDefinition() && UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) != TargetWorldPackage) { return false; }
            if (ObservedWorld.IsValid() && ObservedWorld.Get() != World) { return false; }
            return true;
        }

        bool ObserveWorld(FGuid Id, FName ExperienceId, FName WorldId, UWorld* World)
        {
            if (!IsExpectedWorld(Id, ExperienceId, WorldId, World) ||
                !Facts->ObserveWorld(ToUtf8(Id.ToString()), ToUtf8(WorldId.ToString()), ToUtf8(ExperienceId.ToString()))) { return false; }
            ObservedWorld = World;
            return true;
        }

        bool IsWorldOperable() const
        {
            UGameInstance* Owner = Instance.Get();
            UWorld* World = ObservedWorld.Get();
            return Facts->Has(DivineBeastsLoading::Fact::ExpectedWorld) && Facts->Has(DivineBeastsLoading::Fact::ExpectedExperience) &&
                Owner && World && World->GetGameInstance() == Owner && Owner->GetWorld() == World &&
                World->HasBegunPlay() && !World->bIsTearingDown && World->GetPackage() &&
                UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == TargetWorldPackage;
        }

        TWeakObjectPtr<UGameInstance> Instance;
        const FGuid ObservationId;
        const FName ExpectedWorldId;
        const FName ExpectedExperienceId;
        FString TargetWorldPackage;
    private:
        TSharedRef<DivineBeastsLoading::ReadinessFacts> Facts;
        TWeakObjectPtr<UWorld> ObservedWorld;
};

namespace
{
    /** Loading任务只采样已发生的项目事实；不等待、不创建网络连接，也不伪造成功。 */
    class FDivineBeastsProjectReadinessTask final : public IGamePlatformLoadingTask
    {
    public:
        explicit FDivineBeastsProjectReadinessTask(TWeakPtr<FDivineBeastsProjectLoadingContext> InContext)
            : Context(MoveTemp(InContext)) {}

        FGamePlatformResult Start(UGameInstance& Instance, const FGamePlatformLoadingTaskSpec& Spec) override
        {
            const auto Shared = Context.Pin();
            DivineBeastsLoading::Fact SpecFact;
            if (!Shared || Shared->Instance.Get() != &Instance || !TryGetReadinessFact(Spec.TaskId, SpecFact))
            {
                return LoadingFailure(TEXT("ProjectReadinessContextMismatch"), TEXT("项目加载事实与任务上下文不匹配"));
            }
            Fact = SpecFact;
            bStarted = true;
            return FGamePlatformResult::Success();
        }

        FGamePlatformLoadingTaskUpdate Poll() override
        {
            const auto Shared = Context.Pin();
            if (!bStarted || bReleased || !Shared || !Shared->IsActiveFor(Shared->ObservationId))
            {
                return FailedLoadingUpdate(TEXT("ProjectReadinessCancelled"));
            }
            if (Shared->HasObservedWorld() && Shared->HasWorldDefinition() && !Shared->IsWorldOperable())
            {
                return FailedLoadingUpdate(TEXT("ProjectWorldNoLongerOperable"));
            }
            return Shared->Has(Fact) && Shared->HasWorldDefinition() && Shared->IsWorldOperable()
                ? FGamePlatformLoadingTaskUpdate{EGamePlatformLoadingTaskUpdate::Succeeded, 1.0, NAME_None}
                : FGamePlatformLoadingTaskUpdate{EGamePlatformLoadingTaskUpdate::Pending, 0.0, NAME_None};
        }

        bool IsReadyToUse() const override
        {
            const auto Shared = Context.Pin();
            return bStarted && !bReleased && Shared && Shared->Has(Fact) && Shared->HasWorldDefinition() && Shared->IsWorldOperable();
        }

        void Release() override { bReleased = true; }
    private:
        TWeakPtr<FDivineBeastsProjectLoadingContext> Context;
        DivineBeastsLoading::Fact Fact = DivineBeastsLoading::Fact::Count;
        bool bStarted = false;
        bool bReleased = false;
    };

    /** 通过世界逻辑身份取得真实世界定义租约，再核对体验与MapIdentity；不把MapId当作磁盘包路径。 */
    class FDivineBeastsWorldDefinitionLoadingTask final : public IGamePlatformLoadingTask
    {
    public:
        explicit FDivineBeastsWorldDefinitionLoadingTask(TWeakPtr<FDivineBeastsProjectLoadingContext> InContext)
            : Context(MoveTemp(InContext)) {}

        FGamePlatformResult Start(UGameInstance& InInstance, const FGamePlatformLoadingTaskSpec& Spec) override
        {
            const auto Shared = Context.Pin();
            if (!Shared || Shared->Instance.Get() != &InInstance ||
                Spec.Data.DefinitionId.PrimaryAssetType != UGamePlatformPrimaryDataAsset::DefinitionAssetType() ||
                Spec.Data.DefinitionId.PrimaryAssetName != FName(*Shared->ExpectedWorldId.ToString()) ||
                Spec.Data.ExpectedClass != UDivineBeastsWorldDefinition::StaticClass())
            {
                return LoadingFailure(TEXT("WorldDefinitionRequestInvalid"), TEXT("世界定义数据请求与分配身份不匹配"));
            }
            IGamePlatformDataService* Data = IGamePlatformDataService::Get(InInstance);
            if (!Data) { return LoadingFailure(TEXT("WorldDataServiceUnavailable"), TEXT("世界定义所需的数据服务不可用")); }
            Instance = &InInstance;
            Completion = MakeShared<FGamePlatformResult>();
            FGamePlatformResult Accepted;
            Lease = Data->AcquireDefinition(Spec.Data.DefinitionId, UDivineBeastsWorldDefinition::StaticClass(), Spec.Data.Bundles,
                EGamePlatformDataLifetime::Instance, &InInstance,
                [Result = Completion](const FGamePlatformDataLease&, const FGamePlatformResult& Value) { *Result = Value; }, Accepted);
            if (!Accepted.IsSuccess() || !Lease.IsValid())
            {
                return Accepted.IsSuccess()
                    ? LoadingFailure(TEXT("WorldDefinitionLeaseMissing"), TEXT("数据服务未签发有效世界定义租约"))
                    : Accepted;
            }
            bStarted = true;
            return FGamePlatformResult::Success();
        }

        FGamePlatformLoadingTaskUpdate Poll() override
        {
            const auto Shared = Context.Pin();
            UGameInstance* Owner = Instance.Get();
            IGamePlatformDataService* Data = Owner ? IGamePlatformDataService::Get(*Owner) : nullptr;
            if (!bStarted || bReleased || !Shared || !Shared->IsActiveFor(Shared->ObservationId) || !Data)
            {
                return FailedLoadingUpdate(TEXT("WorldDefinitionTaskExpired"));
            }
            const EGamePlatformDataRequestState State = Data->GetLeaseState(Lease);
            if (State == EGamePlatformDataRequestState::Loading) { return {}; }
            if (State != EGamePlatformDataRequestState::Succeeded)
            {
                const FName Error = Completion.IsValid() && !Completion->Code.IsNone()
                    ? Completion->Code : FName(TEXT("WorldDefinitionLoadFailed"));
                return FailedLoadingUpdate(Error);
            }
            const UGamePlatformDefinitionBase* Base = Data->GetLoadedDefinition(Lease);
            if (!Base || !Base->IsA<UDivineBeastsWorldDefinition>()) { return FailedLoadingUpdate(TEXT("WorldDefinitionClassMismatch")); }
            const auto* Definition = static_cast<const UDivineBeastsWorldDefinition*>(Base);
            const FGamePlatformResult Validation = Definition->ValidateDefinition();
            if (!Validation.IsSuccess()) { return FailedLoadingUpdate(Validation.Code); }
            if (!Shared->SetWorldDefinition(*Definition)) { return FailedLoadingUpdate(TEXT("AssignedWorldDefinitionMismatch")); }
            DefinitionObject = const_cast<UDivineBeastsWorldDefinition*>(Definition);
            bValidated = true;
            return {EGamePlatformLoadingTaskUpdate::Succeeded, 1.0, NAME_None};
        }

        bool IsReadyToUse() const override
        {
            const auto Shared = Context.Pin();
            UGameInstance* Owner = Instance.Get();
            IGamePlatformDataService* Data = Owner ? IGamePlatformDataService::Get(*Owner) : nullptr;
            const UDivineBeastsWorldDefinition* Definition = DefinitionObject.Get();
            return bValidated && !bReleased && Shared && Shared->IsWorldOperable() && Data &&
                Data->GetLeaseState(Lease) == EGamePlatformDataRequestState::Succeeded && Definition &&
                Definition->ValidateDefinition().IsSuccess();
        }

        void Release() override
        {
            if (bReleased) { return; }
            bReleased = true;
            if (Lease.IsValid())
            {
                UGameInstance* Owner = Instance.Get();
                IGamePlatformDataService* Data = Owner ? IGamePlatformDataService::Get(*Owner) : nullptr;
                if (Data)
                {
                    const FGamePlatformResult Released = Data->ReleaseDefinition(Lease);
                    if (!Released.IsSuccess()) { UE_LOG(LogTemp, Error, TEXT("Could not release assigned world definition lease: %s"), *Released.Code.ToString()); }
                }
                else { UE_LOG(LogTemp, Error, TEXT("World definition lease owner ended before explicit release; instance teardown must reclaim it")); }
            }
            Lease = {};
            DefinitionObject.Reset();
            Completion.Reset();
            Instance.Reset();
        }
    private:
        TWeakPtr<FDivineBeastsProjectLoadingContext> Context;
        TWeakObjectPtr<UGameInstance> Instance;
        TWeakObjectPtr<UDivineBeastsWorldDefinition> DefinitionObject;
        FGamePlatformDataLease Lease;
        TSharedPtr<FGamePlatformResult> Completion;
        bool bStarted = false;
        bool bValidated = false;
        bool bReleased = false;
    };

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
    Loading = IGamePlatformLoadingService::Get(*GetGameInstance());

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
    if (!ReleaseLoadingOperation())
    {
        UE_LOG(LogTemp, Error, TEXT("DivineBeasts flow could not release its owned Loading operation during shutdown"));
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
    if (!ReleaseLoadingOperation())
    {
        SetError(EDivineBeastsFlowError::StaleOperation);
        return false;
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

    // 新的Assignment不能覆盖上一操作的任务、订阅或数据租约。
    if ((ActiveLoadingOperation.IsValid() || LoadingContext.IsValid() || !LoadingTaskFactories.IsEmpty()) &&
        !ReleaseLoadingOperation())
    {
        SetError(EDivineBeastsFlowError::StaleOperation);
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
                !Assignment.Summary.WorldId.IsNone() && !Assignment.Summary.MapId.IsNone() &&
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
    if (!Loading || ViewState.Assignment.MapId.IsNone() || ViewState.Assignment.WorldId.IsNone() ||
        ViewState.Assignment.ExperienceId.IsNone())
    {
        BeginRecovery(EDivineBeastsFlowError::WorldAssignmentUnavailable);
        return;
    }

    ActiveTransferOperationId = FGuid::NewGuid();
    ViewState.LoadingObservationId = ActiveTransferOperationId;
    LoadingContext = MakeShared<FDivineBeastsProjectLoadingContext>(
        *GetGameInstance(), ActiveTransferOperationId, ViewState.Assignment.WorldId,
        ViewState.Assignment.ExperienceId);

    FGamePlatformResult Result;
    const TWeakPtr<FDivineBeastsProjectLoadingContext> WeakContext = LoadingContext;
    const FGamePlatformLoadingRegistration Factory = Loading->RegisterTaskFactory(ReadinessTaskType,
        [WeakContext]() -> TUniquePtr<IGamePlatformLoadingTask>
        {
            if (!WeakContext.Pin()) { return nullptr; }
            return MakeUnique<FDivineBeastsProjectReadinessTask>(WeakContext);
        }, Result);
    if (!Result.IsSuccess() || !Factory.IsValid())
    {
        ReleaseLoadingOperation();
        BeginRecovery(EDivineBeastsFlowError::WorldReadinessTimedOut);
        return;
    }
    LoadingTaskFactories.Add(Factory);

    const FGamePlatformLoadingRegistration WorldDefinitionFactory = Loading->RegisterTaskFactory(WorldDefinitionTaskType,
        [WeakContext]() -> TUniquePtr<IGamePlatformLoadingTask>
        {
            if (!WeakContext.Pin()) { return nullptr; }
            return MakeUnique<FDivineBeastsWorldDefinitionLoadingTask>(WeakContext);
        }, Result);
    if (!Result.IsSuccess() || !WorldDefinitionFactory.IsValid())
    {
        ReleaseLoadingOperation();
        BeginRecovery(EDivineBeastsFlowError::WorldReadinessTimedOut);
        return;
    }
    LoadingTaskFactories.Add(WorldDefinitionFactory);

    FGamePlatformLoadingOperationSpec Spec;
    Spec.Purpose = TEXT("DivineBeastsWorldEntry");
    // 全屏障任务使用同一操作期限，避免单个事实在World/Session仍加载时先行超时。
    for (const FName TaskId : {TaskSessionAdmission, TaskExpectedWorld, TaskExpectedExperience,
                                TaskCharacterBinding, TaskGameplayData, TaskProjectReadiness})
    {
        FGamePlatformLoadingTaskSpec Task;
        Task.TaskId = TaskId;
        Task.TaskType = ReadinessTaskType;
        Task.Requiredness = EGamePlatformLoadingRequirement::Required;
        Task.TimeoutSeconds = Spec.TimeoutSeconds;
        Spec.Tasks.Add(MoveTemp(Task));
    }
    FGamePlatformId ParsedWorldId;
    if (!FGamePlatformId::TryParse(ViewState.Assignment.WorldId.ToString(), ParsedWorldId))
    {
        ReleaseLoadingOperation();
        BeginRecovery(EDivineBeastsFlowError::WorldAssignmentUnavailable);
        return;
    }
    FGamePlatformLoadingTaskSpec WorldDefinitionTask;
    WorldDefinitionTask.TaskId = TEXT("AssignedWorldDefinition");
    WorldDefinitionTask.TaskType = WorldDefinitionTaskType;
    WorldDefinitionTask.Requiredness = EGamePlatformLoadingRequirement::Required;
    WorldDefinitionTask.TimeoutSeconds = Spec.TimeoutSeconds;
    WorldDefinitionTask.Data.DefinitionId = FPrimaryAssetId(
        UGamePlatformPrimaryDataAsset::DefinitionAssetType(), FName(*ParsedWorldId.ToString()));
    WorldDefinitionTask.Data.ExpectedClass = UDivineBeastsWorldDefinition::StaticClass();
    Spec.Tasks.Add(MoveTemp(WorldDefinitionTask));

    ActiveLoadingOperation = Loading->StartLoadingOperation(Spec, this, Result);
    if (!Result.IsSuccess() || !ActiveLoadingOperation.IsValid())
    {
        ReleaseLoadingOperation();
        BeginRecovery(EDivineBeastsFlowError::WorldReadinessTimedOut);
        return;
    }
    LoadingSubscription = Loading->SubscribeLoadingState(ActiveLoadingOperation, this,
        [WeakThis = TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem>(this), Expected = ActiveLoadingOperation]
        (const FGamePlatformLoadingSnapshot& Snapshot)
        {
            if (WeakThis.IsValid() && Snapshot.Handle == Expected && WeakThis->ActiveLoadingOperation == Expected)
            {
                WeakThis->HandleLoadingSnapshot(Snapshot);
            }
        });
    if (!LoadingSubscription.IsValid())
    {
        ReleaseLoadingOperation();
        BeginRecovery(EDivineBeastsFlowError::WorldReadinessTimedOut);
        return;
    }
    BroadcastView();

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

bool UDivineBeastsApplicationFlowSubsystem::NotifyWorldObserved(
    FGuid ObservationId,
    FName ExperienceId,
    FName WorldId,
    UObject* WorldContextObject)
{
    UWorld* ObservedWorld = WorldContextObject && GEngine
        ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
        : nullptr;
    if (!Loading || !ActiveLoadingOperation.IsValid() || !LoadingContext.IsValid() ||
        !LoadingContext->IsExpectedWorld(ObservationId, ExperienceId, WorldId, ObservedWorld))
    {
        if (ObservationId == ViewState.LoadingObservationId) { SetError(EDivineBeastsFlowError::WorldMismatch); }
        return false;
    }
    if (!LoadingContext->ObserveWorld(ObservationId, ExperienceId, WorldId, ObservedWorld))
    {
        SetError(EDivineBeastsFlowError::WorldMismatch);
        return false;
    }
    TryCompleteWorldReady();
    return true;
}

bool UDivineBeastsApplicationFlowSubsystem::NotifyCharacterBindingReady(FGuid ObservationId)
{
    return MarkLoadingFactReady(ObservationId, TaskCharacterBinding);
}

bool UDivineBeastsApplicationFlowSubsystem::NotifyGameplayDataReady(FGuid ObservationId)
{
    return MarkLoadingFactReady(ObservationId, TaskGameplayData);
}

bool UDivineBeastsApplicationFlowSubsystem::NotifyProjectReadiness(FGuid ObservationId)
{
    return MarkLoadingFactReady(ObservationId, TaskProjectReadiness);
}

bool UDivineBeastsApplicationFlowSubsystem::MarkLoadingFactReady(FGuid ObservationId, FName TaskId)
{
    DivineBeastsLoading::Fact Fact;
    if (!Loading || !ActiveLoadingOperation.IsValid() || !LoadingContext.IsValid() ||
        ObservationId != ActiveTransferOperationId || !TryGetReadinessFact(TaskId, Fact) ||
        Fact == DivineBeastsLoading::Fact::SessionAdmission || Fact == DivineBeastsLoading::Fact::ExpectedWorld ||
        Fact == DivineBeastsLoading::Fact::ExpectedExperience || !LoadingContext->Observe(ObservationId, Fact))
    {
        return false;
    }
    TryCompleteWorldReady();
    return true;
}

bool UDivineBeastsApplicationFlowSubsystem::ReleaseLoadingOperation()
{
    if (LoadingContext.IsValid()) { LoadingContext->Invalidate(); }
    ViewState.LoadingObservationId.Invalidate();
    ActiveTransferOperationId.Invalidate();
    PendingEndpoint.Reset();
    PendingTransferTicket.Reset();
    BroadcastView();
    if (LoadingSubscription.IsValid())
    {
        if (!Loading) { return false; }
        Loading->UnsubscribeLoadingState(LoadingSubscription);
        LoadingSubscription = {};
    }
    if (ActiveLoadingOperation.IsValid())
    {
        if (!Loading) { return false; }
        const FGamePlatformResult Released = Loading->ReleaseLoadingOperation(ActiveLoadingOperation);
        if (!Released.IsSuccess()) { return false; }
        ActiveLoadingOperation = {};
    }
    if (!LoadingTaskFactories.IsEmpty())
    {
        if (!Loading) { return false; }
        for (int32 Index = LoadingTaskFactories.Num() - 1; Index >= 0; --Index)
        {
            const FGamePlatformResult Unregistered = Loading->UnregisterTaskFactory(LoadingTaskFactories[Index]);
            if (Unregistered.IsSuccess()) { LoadingTaskFactories.RemoveAt(Index); }
        }
        if (!LoadingTaskFactories.IsEmpty()) { return false; }
    }
    LoadingContext.Reset();
    return true;
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
        DivineBeastsLoading::Fact AdmissionFact;
        if (LoadingContext.IsValid() && Snapshot.TransferOperationId == ActiveTransferOperationId &&
            TryGetReadinessFact(TaskSessionAdmission, AdmissionFact) &&
            LoadingContext->Observe(ActiveTransferOperationId, AdmissionFact))
        {
            TryCompleteWorldReady();
        }
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
    if (!(Snapshot.Handle == ActiveLoadingOperation))
    {
        return;
    }

    ViewState.LoadingSummary =
        StaticEnum<EGamePlatformLoadingState>()
            ? StaticEnum<EGamePlatformLoadingState>()
                ->GetNameStringByValue(static_cast<int64>(Snapshot.State))
            : FString();

    if (Snapshot.State == EGamePlatformLoadingState::Ready || Snapshot.State == EGamePlatformLoadingState::DegradedReady)
    {
        if (!Loading || !LoadingContext.IsValid() || !LoadingContext->AllReady() || !LoadingContext->IsWorldOperable() ||
            !Loading->IsReadyToPlay(ActiveLoadingOperation) || !Session)
        {
            BeginRecovery(EDivineBeastsFlowError::WorldReadinessTimedOut);
        }
        else
        {
            const FGamePlatformSessionSnapshot SessionSnapshot = Session->GetSnapshot();
            if (SessionSnapshot.TransferOperationId != ActiveTransferOperationId ||
                SessionSnapshot.State != EGamePlatformSessionTransferState::Admitted)
            {
                BeginRecovery(EDivineBeastsFlowError::AdmissionFailed);
            }
            else { TryCompleteWorldReady(); }
        }
    }
    else if (Snapshot.State == EGamePlatformLoadingState::Failed || Snapshot.State == EGamePlatformLoadingState::TimedOut ||
             Snapshot.State == EGamePlatformLoadingState::Cancelled)
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
        !ActiveLoadingOperation.IsValid() || !LoadingContext.IsValid() ||
        !LoadingContext->AllReady() || !LoadingContext->IsWorldOperable() ||
        !Loading->IsReadyToPlay(ActiveLoadingOperation) ||
        !IsCurrentNode(FDivineBeastsFlowNodes::TransferWorld()))
    {
        return;
    }
    const FGamePlatformSessionSnapshot SessionSnapshot = Session->GetSnapshot();
    if (SessionSnapshot.TransferOperationId != ActiveTransferOperationId ||
        SessionSnapshot.State != EGamePlatformSessionTransferState::Admitted) { return; }

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
    if (!ReleaseLoadingOperation())
    {
        SetBusy(false);
        SetError(EDivineBeastsFlowError::StaleOperation);
        return;
    }
    if (!PlatformFlow || !Session || !ViewState.bHasSelectedCharacter)
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
    if (!ReleaseLoadingOperation())
    {
        UE_LOG(LogTemp, Error, TEXT("DivineBeasts flow logout could not release its owned Loading operation"));
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
