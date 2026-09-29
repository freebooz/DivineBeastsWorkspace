#include "DivineBeastsApplicationFlowSubsystem.h"

#include "Backend/DivineBeastsApplicationBackend.h"
#include "Async/Async.h"
#include "Context/DivineBeastsApplicationFlowContext.h"
#include "Creation/GamePlatformCharacterCreationProvider.h"
#include "Features/IModularFeatures.h"
#include "Flow/DivineBeastsFlowNodes.h"
#include "Online/DivineBeastsGatewayAuthProvider.h"
#include "Interfaces/GamePlatformFlowNodeFactory.h"
#include "API/GamePlatformApplicationFlowSubsystem.h"
#include "Definitions/GamePlatformFlowDefinition.h"
#include "Interfaces/GamePlatformCallbackFlowNode.h"
#include "Types/GamePlatformFlowTypes.h"
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
#include "Engine/StreamableManager.h"
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

    /** 把项目错误枚举转换为稳定可检索错误码；仅错误路径构造字符串，不进入正常热路径。 */
    FName ProjectErrorCode(EDivineBeastsFlowError Error)
    {
        const UEnum* Enum = StaticEnum<EDivineBeastsFlowError>();
        return Enum
            ? FName(*Enum->GetNameStringByValue(static_cast<int64>(Error)))
            : FName(TEXT("DivineBeastsFlowError"));
    }

    FGamePlatformFlowNodeResult ProjectFailure(
        EDivineBeastsFlowError Error,
        const TCHAR* Message)
    {
        return FGamePlatformFlowNodeResult::Failure(
            ProjectErrorCode(Error),
            Message ? FString(Message) : FString(TEXT("神兽联盟应用流程步骤失败。")));
    }

    FGamePlatformFlowNodeToken MakeNodeToken(const FGamePlatformFlowContext& Context)
    {
        return {Context.Handle, Context.NodeId, Context.NodeGeneration};
    }

    EDivineBeastsFlowError MapAuthError(EGamePlatformAuthError Error)
    {
        switch (Error)
        {
        case EGamePlatformAuthError::InvalidCredentials:
            return EDivineBeastsFlowError::InvalidCredentials;
        case EGamePlatformAuthError::AccountLocked:
            return EDivineBeastsFlowError::AccountLocked;
        case EGamePlatformAuthError::Maintenance:
            return EDivineBeastsFlowError::Maintenance;
        case EGamePlatformAuthError::NetworkUnavailable:
        case EGamePlatformAuthError::ProviderUnavailable:
            return EDivineBeastsFlowError::NetworkUnavailable;
        case EGamePlatformAuthError::AuthExpired:
            return EDivineBeastsFlowError::AuthExpired;
        case EGamePlatformAuthError::ContractIncompatible:
            return EDivineBeastsFlowError::ContractIncompatible;
        case EGamePlatformAuthError::Cancelled:
            return EDivineBeastsFlowError::Cancelled;
        case EGamePlatformAuthError::TimedOut:
            return EDivineBeastsFlowError::TimedOut;
        default:
            return EDivineBeastsFlowError::AuthenticationFailed;
        }
    }

    bool IsSessionFailureState(EGamePlatformSessionTransferState State)
    {
        return State == EGamePlatformSessionTransferState::Failed ||
            State == EGamePlatformSessionTransferState::TimedOut ||
            State == EGamePlatformSessionTransferState::Uncertain;
    }

    bool IsBackendExecutor(FName ExecutorId)
    {
        return ExecutorId == FDivineBeastsFlowExecutors::LoadProfile() ||
            ExecutorId == FDivineBeastsFlowExecutors::LoadRoster() ||
            ExecutorId == FDivineBeastsFlowExecutors::CreateCharacter() ||
            ExecutorId == FDivineBeastsFlowExecutors::ValidateSelection() ||
            ExecutorId == FDivineBeastsFlowExecutors::RequestWorld();
    }

    /** Blueprint只支持有符号64位整数；执行器内部仍保留完整uint64身份，UI投影仅做饱和显示。 */
    int64 ToBlueprintCounter(uint64 Value)
    {
        return Value > static_cast<uint64>(MAX_int64)
            ? MAX_int64
            : static_cast<int64>(Value);
    }
}
void UDivineBeastsApplicationFlowSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 显式初始化顺序避免首帧依赖时序竞争；这些依赖均为同一GameInstance作用域。
    Collection.InitializeDependency<UGamePlatformApplicationFlowSubsystem>();
    Collection.InitializeDependency<UGamePlatformOnlineClientSubsystem>();
    Collection.InitializeDependency<UGamePlatformSessionClientSubsystem>();

    UGameInstance* Instance = GetGameInstance();
    PlatformFlow = Instance
        ? Instance->GetSubsystem<UGamePlatformApplicationFlowSubsystem>()
        : nullptr;
    Online = Instance
        ? Instance->GetSubsystem<UGamePlatformOnlineClientSubsystem>()
        : nullptr;
    Session = Instance
        ? Instance->GetSubsystem<UGamePlatformSessionClientSubsystem>()
        : nullptr;
    Loading = Instance ? IGamePlatformLoadingService::Get(*Instance) : nullptr;
    Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;

    if (Online)
    {
        // 项目组合根只注入一个真实 Provider；UI/Flow 仍只依赖平台 Online 公共状态机。
        AuthProvider = MakeShared<FDivineBeastsGatewayAuthProvider>();
        Online->SetProvider(AuthProvider);
    }

    Backend = MakeShared<FDivineBeastsHttpApplicationBackend>(Online);

    if (PlatformFlow)
    {
        if (!RegisterNodeFactories())
        {
            SetError(EDivineBeastsFlowError::FlowNotInitialized);
        }
        FlowSnapshotHandle = PlatformFlow->OnSnapshotChanged().AddUObject(
            this,
            &UDivineBeastsApplicationFlowSubsystem::HandleFlowSnapshot);
        FlowFinishedHandle = PlatformFlow->OnFinished().AddUObject(
            this,
            &UDivineBeastsApplicationFlowSubsystem::HandleFlowFinished);
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
        if (Online)
        {
            const FGamePlatformAuthSnapshot Auth = Online->GetSnapshot();
            Session->SetAuthenticationContext(
                Auth.State == EGamePlatformAuthState::Authenticated
                    ? Auth.AccountId
                    : FString(),
                Auth.AuthGeneration);
        }
    }

    ResetProjection();
    if (!PlatformFlow || !Online || !Session || !Loading || !Data)
    {
        SetError(EDivineBeastsFlowError::FlowNotInitialized);
    }
}

void UDivineBeastsApplicationFlowSubsystem::Deinitialize()
{
    ++StartRequestGeneration;
    bRestartAfterLogout = false;

    if (CharacterCreationValidationLease.IsValid())
    {
        CharacterCreationValidationLease->CancelHandle();
        CharacterCreationValidationLease.Reset();
    }

    if (Backend.IsValid())
    {
        Backend->CancelAll();
    }

    ReleaseLoadingOperation();

    if (Session)
    {
        FGamePlatformResult Ignored;
        Session->CancelTransfer(Ignored);
    }

    if (PlatformFlow)
    {
        if (ActiveFlow.IsValid())
        {
            PlatformFlow->Cancel(ActiveFlow);
            ActiveFlow = {};
        }

        if (FlowSnapshotHandle.IsValid())
        {
            PlatformFlow->OnSnapshotChanged().Remove(FlowSnapshotHandle);
        }
        if (FlowFinishedHandle.IsValid())
        {
            PlatformFlow->OnFinished().Remove(FlowFinishedHandle);
        }
    }

    ReleasePendingFlowDefinitionLease();
    UnregisterNodeFactories();

    if (Online && AuthHandle.IsValid())
    {
        Online->OnAuthStateChanged().Remove(AuthHandle);
    }
    if (Online)
    {
        // 先让平台子系统推进认证代次并丢弃旧 Provider，再释放项目 Provider。
        Online->SetProvider(nullptr);
    }
    AuthProvider.Reset();
    if (Session && SessionHandle.IsValid())
    {
        Session->OnSessionChanged().Remove(SessionHandle);
    }

    Extensions.Reset();
    ExtensionOrder.Reset();
    FlowContext = nullptr;
    Backend.Reset();

    PlatformFlow = nullptr;
    Online = nullptr;
    Session = nullptr;
    Loading = nullptr;
    Data = nullptr;

    ViewStateChanged.Clear();
    Super::Deinitialize();
}

bool UDivineBeastsApplicationFlowSubsystem::RegisterNodeFactories()
{
    if (!PlatformFlow || !FactoryHandles.IsEmpty())
    {
        return PlatformFlow && !FactoryHandles.IsEmpty();
    }

    const FName ExecutorIds[] =
    {
        FDivineBeastsFlowExecutors::Boot(),
        FDivineBeastsFlowExecutors::Initialize(),
        FDivineBeastsFlowExecutors::Authentication(),
        FDivineBeastsFlowExecutors::LoadProfile(),
        FDivineBeastsFlowExecutors::LoadRoster(),
        FDivineBeastsFlowExecutors::CharacterEntry(),
        FDivineBeastsFlowExecutors::CreateCharacter(),
        FDivineBeastsFlowExecutors::ValidateSelection(),
        FDivineBeastsFlowExecutors::ResolveExperience(),
        FDivineBeastsFlowExecutors::RequestWorld(),
        FDivineBeastsFlowExecutors::TransferWorld(),
        FDivineBeastsFlowExecutors::WorldReady(),
        FDivineBeastsFlowExecutors::InWorld(),
        FDivineBeastsFlowExecutors::Recovering()
    };

    TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem> WeakThis(this);
    for (const FName ExecutorId : ExecutorIds)
    {
        FGamePlatformResult Result;
        const FGamePlatformFlowFactoryHandle Handle =
            PlatformFlow->RegisterNodeFactory(
                ExecutorId,
                [WeakThis, ExecutorId](UGameInstance& Owner) -> UGamePlatformFlowNode*
                {
                    UDivineBeastsApplicationFlowSubsystem* Self = WeakThis.Get();
                    return Self ? Self->CreateProjectNode(ExecutorId, Owner) : nullptr;
                },
                Result);
        if (!Handle.IsValid() || !Result.IsSuccess())
        {
            UnregisterNodeFactories();
            return false;
        }
        FactoryHandles.Add(Handle);
    }
    return true;
}

void UDivineBeastsApplicationFlowSubsystem::UnregisterNodeFactories()
{
    if (!PlatformFlow)
    {
        FactoryHandles.Reset();
        return;
    }

    // 逆序撤销便于未来出现依赖型工厂时仍保持与注册相反的生命周期顺序。
    for (int32 Index = FactoryHandles.Num() - 1; Index >= 0; --Index)
    {
        FGamePlatformResult Ignored;
        PlatformFlow->UnregisterNodeFactory(FactoryHandles[Index], Ignored);
    }
    FactoryHandles.Reset();
}

UGamePlatformFlowNode* UDivineBeastsApplicationFlowSubsystem::CreateProjectNode(
    FName ExecutorId,
    UGameInstance& Owner)
{
    check(IsInGameThread());
    if (&Owner != GetGameInstance())
    {
        return nullptr;
    }

    if (ExecutorId == FDivineBeastsFlowExecutors::Authentication() ||
        ExecutorId == FDivineBeastsFlowExecutors::CharacterEntry() ||
        ExecutorId == FDivineBeastsFlowExecutors::WorldReady() ||
        ExecutorId == FDivineBeastsFlowExecutors::InWorld())
    {
        // 这些步骤只等待外部真实事件，不包含神兽联盟专属节点机制；直接复用平台通用等待节点。
        return GamePlatformApplicationFlowNodes::CreateAwaitEventFlowNode(Owner);
    }

    auto* Node = NewObject<UGamePlatformCallbackFlowNode>(&Owner);
    TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem> WeakThis(this);
    const bool bBound = Node->Bind(
        [WeakThis, ExecutorId](
            const FGamePlatformFlowContext& Context,
            FGamePlatformFlowCompletion Complete)
        {
            if (UDivineBeastsApplicationFlowSubsystem* Self = WeakThis.Get())
            {
                Self->ExecuteProjectNode(
                    ExecutorId,
                    Context,
                    MoveTemp(Complete));
                return;
            }
            Complete(FGamePlatformFlowNodeResult::Failure(
                TEXT("DivineBeastsFlowOwnerExpired"),
                TEXT("神兽联盟应用流程协调子系统已经失效。")));
        },
        [WeakThis, ExecutorId](EGamePlatformFlowFinishReason Reason)
        {
            if (UDivineBeastsApplicationFlowSubsystem* Self = WeakThis.Get())
            {
                Self->FinishProjectNode(ExecutorId, Reason);
            }
        });
    return bBound ? Node : nullptr;
}

void UDivineBeastsApplicationFlowSubsystem::ExecuteProjectNode(
    FName ExecutorId,
    const FGamePlatformFlowContext& Context,
    FGamePlatformFlowCompletion Complete)
{
    check(IsInGameThread());

    UDivineBeastsApplicationFlowContext* ProjectContext =
        Cast<UDivineBeastsApplicationFlowContext>(Context.Payload.Get());
    if (!ProjectContext ||
        ProjectContext != FlowContext ||
        ProjectContext->GetTypedOuter<UGameInstance>() != GetGameInstance())
    {
        Complete(FGamePlatformFlowNodeResult::Failure(
            TEXT("DivineBeastsFlowContextInvalid"),
            TEXT("项目流程节点缺少当前GameInstance所属的流程上下文。")));
        return;
    }

    const FGamePlatformFlowNodeToken Token = MakeNodeToken(Context);
    if (!Token.IsValid())
    {
        Complete(FGamePlatformFlowNodeResult::Failure(
            TEXT("DivineBeastsFlowTokenInvalid"),
            TEXT("项目流程节点没有有效的平台节点代次。")));
        return;
    }

    if (ExecutorId == FDivineBeastsFlowExecutors::Boot() ||
        ExecutorId == FDivineBeastsFlowExecutors::Initialize())
    {
        Complete(FGamePlatformFlowNodeResult::Success());
        return;
    }

    if (ExecutorId == FDivineBeastsFlowExecutors::LoadProfile())
    {
        if (!Backend.IsValid())
        {
            Complete(ProjectFailure(
                EDivineBeastsFlowError::ProfileUnavailable,
                TEXT("玩家资料后端适配器不可用。")));
            return;
        }

        const TSharedRef<FGamePlatformFlowCompletion> SharedComplete =
            MakeShared<FGamePlatformFlowCompletion>(MoveTemp(Complete));
        TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem> WeakThis(this);
        Backend->LoadProfile(
            [WeakThis, Token, SharedComplete](
                bool bSuccess,
                FDivineBeastsPlayerProfile Profile,
                EDivineBeastsFlowError Error) mutable
            {
                AsyncTask(
                    ENamedThreads::GameThread,
                    [WeakThis, Token, SharedComplete, bSuccess,
                     Profile = MoveTemp(Profile), Error]() mutable
                    {
                        UDivineBeastsApplicationFlowSubsystem* Self = WeakThis.Get();
                        if (!Self || !Self->IsCurrentToken(Token))
                        {
                            return;
                        }
                        if (!bSuccess || !Self->FlowContext)
                        {
                            Self->SetError(Error);
                            (*SharedComplete)(ProjectFailure(
                                Error,
                                TEXT("加载玩家资料失败。")));
                            return;
                        }
                        Self->FlowContext->GetProfile() = MoveTemp(Profile);
                        Self->RefreshProjectionFromContext();
                        Self->SetError(EDivineBeastsFlowError::None);
                        (*SharedComplete)(FGamePlatformFlowNodeResult::Success());
                    });
            });
        return;
    }

    if (ExecutorId == FDivineBeastsFlowExecutors::LoadRoster())
    {
        if (!Backend.IsValid())
        {
            Complete(ProjectFailure(
                EDivineBeastsFlowError::CharacterRosterUnavailable,
                TEXT("角色列表后端适配器不可用。")));
            return;
        }

        const TSharedRef<FGamePlatformFlowCompletion> SharedComplete =
            MakeShared<FGamePlatformFlowCompletion>(MoveTemp(Complete));
        TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem> WeakThis(this);
        Backend->LoadRoster(
            [WeakThis, Token, SharedComplete](
                bool bSuccess,
                TArray<FDivineBeastsCharacterSummary> Roster,
                EDivineBeastsFlowError Error) mutable
            {
                AsyncTask(
                    ENamedThreads::GameThread,
                    [WeakThis, Token, SharedComplete, bSuccess,
                     Roster = MoveTemp(Roster), Error]() mutable
                    {
                        UDivineBeastsApplicationFlowSubsystem* Self = WeakThis.Get();
                        if (!Self || !Self->IsCurrentToken(Token))
                        {
                            return;
                        }
                        if (!bSuccess || !Self->FlowContext)
                        {
                            Self->SetError(Error);
                            (*SharedComplete)(ProjectFailure(
                                Error,
                                TEXT("加载持久角色列表失败。")));
                            return;
                        }
                        Self->FlowContext->GetCharacterRoster() = MoveTemp(Roster);
                        Self->RefreshProjectionFromContext();
                        Self->SetBusy(false);
                        Self->SetError(EDivineBeastsFlowError::None);
                        (*SharedComplete)(FGamePlatformFlowNodeResult::Success());
                    });
            });
        return;
    }

    if (ExecutorId == FDivineBeastsFlowExecutors::CreateCharacter())
    {
        FDivineBeastsCharacterCreateDraft Draft;
        if (!Backend.IsValid() ||
            !ProjectContext->ConsumePendingCreateDraft(Draft))
        {
            Complete(ProjectFailure(
                EDivineBeastsFlowError::CharacterCreateRejected,
                TEXT("没有可执行的角色创建草稿。")));
            return;
        }

        const FGuid OperationId = FGuid::NewGuid();
        const TSharedRef<FGamePlatformFlowCompletion> SharedComplete =
            MakeShared<FGamePlatformFlowCompletion>(MoveTemp(Complete));
        TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem> WeakThis(this);
        Backend->CreateCharacter(
            Draft,
            OperationId,
            [WeakThis, Token, SharedComplete](
                bool bSuccess,
                FDivineBeastsCharacterSummary Character,
                EDivineBeastsFlowError Error) mutable
            {
                AsyncTask(
                    ENamedThreads::GameThread,
                    [WeakThis, Token, SharedComplete, bSuccess,
                     Character = MoveTemp(Character), Error]() mutable
                    {
                        UDivineBeastsApplicationFlowSubsystem* Self = WeakThis.Get();
                        if (!Self || !Self->IsCurrentToken(Token))
                        {
                            return;
                        }
                        if (!bSuccess || !Self->FlowContext)
                        {
                            Self->SetBusy(false);
                            Self->SetError(Error);
                            (*SharedComplete)(ProjectFailure(
                                Error,
                                TEXT("创建持久角色失败。")));
                            return;
                        }
                        Self->FlowContext->GetCharacterRoster().Add(Character);
                        Self->FlowContext->SetPendingSelection(Character);
                        Self->RefreshProjectionFromContext();
                        (*SharedComplete)(FGamePlatformFlowNodeResult::Success());
                    });
            });
        return;
    }

    if (ExecutorId == FDivineBeastsFlowExecutors::ValidateSelection())
    {
        FDivineBeastsCharacterSummary Character;
        if (!Backend.IsValid() ||
            !ProjectContext->ConsumePendingSelection(Character))
        {
            Complete(ProjectFailure(
                EDivineBeastsFlowError::CharacterSelectionRejected,
                TEXT("没有待后端验证的角色选择。")));
            return;
        }

        const FGuid RequestId = FGuid::NewGuid();
        const TSharedRef<FGamePlatformFlowCompletion> SharedComplete =
            MakeShared<FGamePlatformFlowCompletion>(MoveTemp(Complete));
        TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem> WeakThis(this);
        Backend->SelectPersistentCharacter(
            Character.CharacterId,
            Character.CharacterRevision,
            RequestId,
            [WeakThis, Token, SharedComplete](
                bool bSuccess,
                FDivineBeastsValidatedSelection Selection,
                EDivineBeastsFlowError Error) mutable
            {
                AsyncTask(
                    ENamedThreads::GameThread,
                    [WeakThis, Token, SharedComplete, bSuccess,
                     Selection = MoveTemp(Selection), Error]() mutable
                    {
                        UDivineBeastsApplicationFlowSubsystem* Self = WeakThis.Get();
                        if (!Self || !Self->IsCurrentToken(Token))
                        {
                            return;
                        }
                        if (!bSuccess || !Self->FlowContext)
                        {
                            Self->SetBusy(false);
                            Self->SetError(Error);
                            (*SharedComplete)(ProjectFailure(
                                Error,
                                TEXT("持久角色选择没有通过后端权威验证。")));
                            return;
                        }
                        Self->FlowContext->SetSelectedCharacter(Selection.Character);
                        Self->FlowContext->GetProfile().ProfileRevision =
                            Selection.ProfileRevision;
                        Self->RefreshProjectionFromContext();
                        Self->SetError(EDivineBeastsFlowError::None);
                        (*SharedComplete)(FGamePlatformFlowNodeResult::Success());
                    });
            });
        return;
    }

    if (ExecutorId == FDivineBeastsFlowExecutors::ResolveExperience())
    {
        const FName ExperienceId = DefaultExperienceForOnboarding();
        if (ExperienceId.IsNone() ||
            !IsExperienceAllowedForCurrentProfile(ExperienceId))
        {
            SetError(EDivineBeastsFlowError::ExperienceNotAllowed);
            Complete(ProjectFailure(
                EDivineBeastsFlowError::ExperienceNotAllowed,
                TEXT("当前玩家状态无法解析到合法的项目体验。")));
            return;
        }
        ProjectContext->SetTargetExperience(ExperienceId, FString());
        Complete(FGamePlatformFlowNodeResult::Success());
        return;
    }

    if (ExecutorId == FDivineBeastsFlowExecutors::RequestWorld())
    {
        if (!Backend.IsValid() ||
            !ProjectContext->HasSelectedCharacter() ||
            ProjectContext->GetTargetExperience().IsNone() ||
            !IsExperienceAllowedForCurrentProfile(
                ProjectContext->GetTargetExperience()))
        {
            SetError(EDivineBeastsFlowError::ExperienceNotAllowed);
            Complete(ProjectFailure(
                EDivineBeastsFlowError::ExperienceNotAllowed,
                TEXT("请求世界分配前缺少有效角色或目标体验。")));
            return;
        }

        const FName ExpectedExperience = ProjectContext->GetTargetExperience();
        const FString PreferredRegion = ProjectContext->GetPreferredRegion();
        const FDivineBeastsCharacterSummary Character =
            ProjectContext->GetSelectedCharacter();
        const FGuid RequestId = FGuid::NewGuid();

        const TSharedRef<FGamePlatformFlowCompletion> SharedComplete =
            MakeShared<FGamePlatformFlowCompletion>(MoveTemp(Complete));
        TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem> WeakThis(this);
        Backend->RequestWorldAssignment(
            Character.CharacterId,
            Character.CharacterRevision,
            ExpectedExperience,
            PreferredRegion,
            RequestId,
            [WeakThis, Token, SharedComplete, ExpectedExperience,
             ExpectedCharacterId = Character.CharacterId](
                bool bSuccess,
                FDivineBeastsWorldAssignmentPayload Assignment,
                EDivineBeastsFlowError Error) mutable
            {
                AsyncTask(
                    ENamedThreads::GameThread,
                    [WeakThis, Token, SharedComplete, ExpectedExperience,
                     ExpectedCharacterId = MoveTemp(ExpectedCharacterId),
                     bSuccess, Assignment = MoveTemp(Assignment), Error]() mutable
                    {
                        UDivineBeastsApplicationFlowSubsystem* Self = WeakThis.Get();
                        if (!Self || !Self->IsCurrentToken(Token))
                        {
                            return;
                        }
                        if (!bSuccess || !Self->FlowContext)
                        {
                            Self->SetBusy(false);
                            Self->SetError(Error);
                            (*SharedComplete)(ProjectFailure(
                                Error,
                                TEXT("后端世界分配失败。")));
                            return;
                        }

                        FName ExpectedRole = NAME_None;
                        const bool bMappingValid =
                            Assignment.Summary.ExperienceId == ExpectedExperience &&
                            FDivineBeastsProjectCatalog::TryGetServerRoleForExperience(
                                Assignment.Summary.ExperienceId,
                                ExpectedRole) &&
                            ExpectedRole == Assignment.Summary.ServerRoleId &&
                            !Assignment.Summary.WorldId.IsNone() &&
                            !Assignment.Summary.MapId.IsNone() &&
                            Assignment.Summary.CharacterId == ExpectedCharacterId;
                        if (!bMappingValid)
                        {
                            Self->SetBusy(false);
                            Self->SetError(EDivineBeastsFlowError::WorldMismatch);
                            (*SharedComplete)(ProjectFailure(
                                EDivineBeastsFlowError::WorldMismatch,
                                TEXT("后端世界分配与请求角色、体验或服务器角色不一致。")));
                            return;
                        }

                        Self->FlowContext->SetAssignment(
                            Assignment.Summary,
                            MoveTemp(Assignment.Endpoint),
                            MoveTemp(Assignment.TransferTicket));
                        Self->RefreshProjectionFromContext();
                        (*SharedComplete)(FGamePlatformFlowNodeResult::Success());
                    });
            });
        return;
    }

    if (ExecutorId == FDivineBeastsFlowExecutors::TransferWorld())
    {
        const FGamePlatformResult Result = BeginLoadingForAssignment();
        if (!Result.IsSuccess())
        {
            SetBusy(false);
            SetError(EDivineBeastsFlowError::TravelFailed);
            Complete(FGamePlatformFlowNodeResult::Failure(
                Result.Code.IsNone()
                    ? FName(TEXT("TravelFailed"))
                    : Result.Code,
                Result.Message.IsEmpty()
                    ? FString(TEXT("世界切换操作没有被真实Loading/Session服务接纳。"))
                    : Result.Message));
            return;
        }
        Complete(FGamePlatformFlowNodeResult::Success());
        return;
    }

    if (ExecutorId == FDivineBeastsFlowExecutors::Recovering())
    {
        ReleaseLoadingOperation();
        if (Session)
        {
            FGamePlatformResult Ignored;
            Session->CancelTransfer(Ignored);
        }

        const int32 Attempt = ProjectContext->IncrementRecoveryAttempts();
        if (Attempt > MaxRecoveryAttempts)
        {
            SetBusy(false);
            SetError(EDivineBeastsFlowError::ReconnectExhausted);
            Complete(ProjectFailure(
                EDivineBeastsFlowError::ReconnectExhausted,
                TEXT("世界连接恢复已经超过最大尝试次数。")));
            return;
        }

        FName ExperienceId = ProjectContext->GetAssignment().ExperienceId;
        if (ExperienceId.IsNone() ||
            !IsExperienceAllowedForCurrentProfile(ExperienceId))
        {
            ExperienceId = DefaultExperienceForOnboarding();
        }
        if (ExperienceId.IsNone())
        {
            Complete(ProjectFailure(
                EDivineBeastsFlowError::ExperienceNotAllowed,
                TEXT("恢复连接时无法解析安全目标体验。")));
            return;
        }

        ProjectContext->SetTargetExperience(ExperienceId, FString());
        Complete(FGamePlatformFlowNodeResult::Success());
        return;
    }

    Complete(FGamePlatformFlowNodeResult::Failure(
        TEXT("UnknownDivineBeastsFlowExecutor"),
        TEXT("流程定义引用了未实现的神兽联盟执行器。")));
}

void UDivineBeastsApplicationFlowSubsystem::FinishProjectNode(
    FName ExecutorId,
    EGamePlatformFlowFinishReason Reason)
{
    check(IsInGameThread());

    // 同一时间平台只运行一个主节点；HTTP节点退出时取消尚未结束的本节点请求，
    // 已完成请求已从ActiveRequests移除，因此成功路径不会产生额外网络取消开销。
    if (IsBackendExecutor(ExecutorId) && Backend.IsValid())
    {
        Backend->CancelAll();
    }
}

void UDivineBeastsApplicationFlowSubsystem::ReleasePendingFlowDefinitionLease()
{
    if (Data && PendingFlowDefinitionLease.IsValid())
    {
        Data->ReleaseDefinition(PendingFlowDefinitionLease);
    }
    PendingFlowDefinitionLease = {};
}

bool UDivineBeastsApplicationFlowSubsystem::StartFlow(bool bTryAutoLogin)
{
    if (!PlatformFlow || !Online || !Session || !Loading || !Data ||
        FactoryHandles.IsEmpty())
    {
        SetError(EDivineBeastsFlowError::FlowNotInitialized);
        return false;
    }

    ++StartRequestGeneration;
    const uint64 RequestGeneration = StartRequestGeneration;

    if (Backend.IsValid())
    {
        Backend->CancelAll();
    }
    ReleaseLoadingOperation();

    if (Session)
    {
        FGamePlatformResult Ignored;
        Session->CancelTransfer(Ignored);
    }

    if (ActiveFlow.IsValid())
    {
        PlatformFlow->Cancel(ActiveFlow);
        ActiveFlow = {};
    }

    ReleasePendingFlowDefinitionLease();

    FlowContext = NewObject<UDivineBeastsApplicationFlowContext>(
        GetGameInstance());
    if (!FlowContext)
    {
        SetError(EDivineBeastsFlowError::FlowNotInitialized);
        return false;
    }
    FlowContext->ResetForNewRun(bTryAutoLogin);

    ResetProjection();
    SetBusy(true);
    SetError(EDivineBeastsFlowError::None);
    LastAutoLoginNodeGeneration = 0;
    LastEnteredInWorldNodeGeneration = 0;

    FGamePlatformId FlowLogicalId;
    if (!FGamePlatformId::TryParse(
            TEXT("divinebeasts.application.main@1"),
            FlowLogicalId))
    {
        SetBusy(false);
        SetError(EDivineBeastsFlowError::FlowNotInitialized);
        return false;
    }

    const FPrimaryAssetId DefinitionId(
        UGamePlatformPrimaryDataAsset::DefinitionAssetType(),
        FName(*FlowLogicalId.ToString()));

    TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem> WeakThis(this);
    FGamePlatformResult Accepted;
    PendingFlowDefinitionLease = Data->AcquireDefinition(
        DefinitionId,
        UGamePlatformFlowDefinition::StaticClass(),
        {},
        EGamePlatformDataLifetime::Instance,
        this,
        [WeakThis, RequestGeneration](
            const FGamePlatformDataLease& Lease,
            const FGamePlatformResult& Result)
        {
            UDivineBeastsApplicationFlowSubsystem* Self = WeakThis.Get();
            if (!Self)
            {
                return;
            }

            if (Self->StartRequestGeneration != RequestGeneration)
            {
                if (Self->Data && Lease.IsValid())
                {
                    Self->Data->ReleaseDefinition(Lease);
                }
                return;
            }

            Self->PendingFlowDefinitionLease = Lease;
            if (!Result.IsSuccess() ||
                !Lease.IsValid() ||
                !Self->PlatformFlow ||
                !Self->FlowContext)
            {
                Self->ReleasePendingFlowDefinitionLease();
                Self->SetBusy(false);
                Self->SetError(EDivineBeastsFlowError::FlowNotInitialized);
                return;
            }

            FGamePlatformResult StartResult;
            Self->ActiveFlow = Self->PlatformFlow->StartFlow(
                Self->PendingFlowDefinitionLease,
                Self->FlowContext,
                StartResult);
            if (!Self->ActiveFlow.IsValid() || !StartResult.IsSuccess())
            {
                Self->ReleasePendingFlowDefinitionLease();
                Self->SetBusy(false);
                Self->SetError(EDivineBeastsFlowError::FlowExecutionFailed);
                return;
            }

            // StartFlow在成功接纳后已经转移并清空输入租约；同步补投影以覆盖启动事件早于ActiveFlow赋值的情况。
            Self->HandleFlowSnapshot(Self->PlatformFlow->GetSnapshot());
        },
        Accepted);

    if (!Accepted.IsSuccess() || !PendingFlowDefinitionLease.IsValid())
    {
        ReleasePendingFlowDefinitionLease();
        SetBusy(false);
        SetError(EDivineBeastsFlowError::FlowNotInitialized);
        return false;
    }
    return true;
}

void UDivineBeastsApplicationFlowSubsystem::TryAutoLogin()
{
    if (!Online ||
        !IsCurrentNode(FDivineBeastsFlowNodes::Authentication()))
    {
        SetError(EDivineBeastsFlowError::FlowNotInitialized);
        return;
    }

    // 自动登录只是向 Online 领域提交认证意图；流程仍等待真实认证事件，
    // 不把“请求已发出”当作认证成功。
    SetBusy(true);
    Online->TryAutoLogin();
}

void UDivineBeastsApplicationFlowSubsystem::LoginWithCredentials(
    const FString& LoginName,
    const FString& Password)
{
    if (!Online ||
        !IsCurrentNode(FDivineBeastsFlowNodes::Authentication()))
    {
        SetError(EDivineBeastsFlowError::FlowNotInitialized);
        return;
    }

    SetBusy(true);
    Online->LoginWithCredentials(LoginName, Password);
}

bool UDivineBeastsApplicationFlowSubsystem::GetCharacterCreationOptions(
    TArray<FDivineBeastsCharacterCreationOption>& OutOptions) const
{
    OutOptions.Reset();
    IGamePlatformCharacterCreationProvider* Provider =
        GetCharacterCreationProvider();
    if (!Provider)
    {
        return false;
    }

    TArray<FGamePlatformCharacterCreationHeroDescriptor> Heroes;
    Provider->GetCreateableHeroes(Heroes);
    OutOptions.Reserve(Heroes.Num());
    for (const FGamePlatformCharacterCreationHeroDescriptor& Hero : Heroes)
    {
        if (Hero.HeroDefinitionId.IsNone())
        {
            continue;
        }
        FDivineBeastsCharacterCreationOption Option;
        Option.HeroDefinitionId = Hero.HeroDefinitionId;
        Option.DisplayNameKey = Hero.DisplayNameKey;
        OutOptions.Add(MoveTemp(Option));
    }
    return !OutOptions.IsEmpty();
}

bool UDivineBeastsApplicationFlowSubsystem::SubmitCharacterCreateDraft(
    const FDivineBeastsCharacterCreateDraft& Draft)
{
    if (!FlowContext ||
        !Draft.IsLocallyValid() ||
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

    // 完整外观Schema校验必须基于真实Definition；不能把“尚未加载”误判为用户草稿非法。
    if (CharacterCreationValidationLease.IsValid())
    {
        CharacterCreationValidationLease->CancelHandle();
        CharacterCreationValidationLease.Reset();
    }

    const FGamePlatformFlowNodeToken Token = GetCurrentNodeToken();
    if (!Token.IsValid())
    {
        return false;
    }

    SetBusy(true);
    TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem> WeakThis(this);
    CharacterCreationValidationLease = CreationProvider->ValidateCreationDraftAsync(
        Draft.HeroDefinitionId,
        Draft.AppearanceSelection,
        [WeakThis, Token, Draft](bool bValid, FString) mutable
        {
            UDivineBeastsApplicationFlowSubsystem* Self = WeakThis.Get();
            if (!Self)
            {
                return;
            }

            Self->CharacterCreationValidationLease.Reset();
            if (!Self->IsCurrentToken(Token) ||
                !Self->IsCurrentNode(FDivineBeastsFlowNodes::CharacterEntry()) ||
                !Self->FlowContext)
            {
                return;
            }

            if (!bValid)
            {
                Self->SetError(EDivineBeastsFlowError::InvalidAppearance);
                Self->SetBusy(false);
                return;
            }

            Self->FlowContext->SetPendingCreateDraft(Draft);
            const bool bAccepted = Self->SubmitCurrentNodeEvent(
                FDivineBeastsFlowNodes::CharacterEntry(),
                FGamePlatformFlowNodeResult::Success(
                    FDivineBeastsFlowOutcomes::CreateCharacter()));
            if (!bAccepted)
            {
                Self->SetBusy(false);
            }
        });

    return true;
}

bool UDivineBeastsApplicationFlowSubsystem::SelectPersistentCharacter(
    const FString& CharacterId)
{
    if (!FlowContext ||
        !IsCurrentNode(FDivineBeastsFlowNodes::CharacterEntry()))
    {
        return false;
    }

    const FDivineBeastsCharacterSummary* Character =
        FlowContext->GetCharacterRoster().FindByPredicate(
            [&CharacterId](const FDivineBeastsCharacterSummary& Candidate)
            {
                return Candidate.CharacterId == CharacterId;
            });
    if (!Character)
    {
        SetError(EDivineBeastsFlowError::CharacterNotFound);
        return false;
    }

    FlowContext->SetPendingSelection(*Character);
    const bool bAccepted = SubmitCurrentNodeEvent(
        FDivineBeastsFlowNodes::CharacterEntry(),
        FGamePlatformFlowNodeResult::Success(
            FDivineBeastsFlowOutcomes::SelectCharacter()));
    if (bAccepted)
    {
        SetBusy(true);
    }
    return bAccepted;
}

bool UDivineBeastsApplicationFlowSubsystem::RequestWorldAssignment(
    FName DesiredExperienceId,
    const FString& PreferredRegion)
{
    if (!FlowContext ||
        !FlowContext->HasSelectedCharacter() ||
        !IsCurrentNode(FDivineBeastsFlowNodes::InWorld()) ||
        !IsExperienceAllowedForCurrentProfile(DesiredExperienceId))
    {
        SetError(EDivineBeastsFlowError::ExperienceNotAllowed);
        return false;
    }

    NotifyExtensionsLeavingWorld();
    FlowContext->SetTargetExperience(
        DesiredExperienceId,
        PreferredRegion);

    const bool bAccepted = SubmitCurrentNodeEvent(
        FDivineBeastsFlowNodes::InWorld(),
        FGamePlatformFlowNodeResult::Success(
            FDivineBeastsFlowOutcomes::RequestWorld()));
    if (bAccepted)
    {
        SetBusy(true);
    }
    return bAccepted;
}

FName UDivineBeastsApplicationFlowSubsystem::DefaultExperienceForOnboarding() const
{
    if (!FlowContext)
    {
        return NAME_None;
    }

    return FlowContext->GetProfile().OnboardingState ==
        EDivineBeastsOnboardingState::OnboardingComplete
        ? FDivineBeastsProjectCatalog::GetOpenWorldHubExperience()
        : FDivineBeastsProjectCatalog::GetVillageTutorialExperience();
}

bool UDivineBeastsApplicationFlowSubsystem::IsExperienceAllowedForCurrentProfile(
    FName ExperienceId) const
{
    if (!FlowContext || ExperienceId.IsNone())
    {
        return false;
    }

    FName ServerRole = NAME_None;
    if (!FDivineBeastsProjectCatalog::TryGetServerRoleForExperience(
            ExperienceId,
            ServerRole))
    {
        return false;
    }

    const bool bComplete =
        FlowContext->GetProfile().OnboardingState ==
        EDivineBeastsOnboardingState::OnboardingComplete;
    if (bComplete)
    {
        // 完成新手后公共应用流程只进入OpenWorld；MainArena由DBAArena先完成匹配后通过授权分配接入。
        return ServerRole ==
            FDivineBeastsProjectCatalog::GetOpenWorldServerRole();
    }

    return ServerRole ==
        FDivineBeastsProjectCatalog::GetVillageServerRole();
}

bool UDivineBeastsApplicationFlowSubsystem::IsCurrentToken(
    const FGamePlatformFlowNodeToken& Token) const
{
    if (!PlatformFlow || !Token.IsValid())
    {
        return false;
    }

    const FGamePlatformFlowSnapshot Snapshot = PlatformFlow->GetSnapshot();
    return Snapshot.Handle.ScopeId == Token.Handle.ScopeId &&
        Snapshot.Handle.RunId == Token.Handle.RunId &&
        Snapshot.NodeId == Token.NodeId &&
        Snapshot.NodeGeneration == Token.NodeGeneration &&
        (Snapshot.State == EGamePlatformFlowState::Running ||
         Snapshot.State == EGamePlatformFlowState::RetryWaiting);
}

bool UDivineBeastsApplicationFlowSubsystem::IsCurrentNode(
    FName NodeId) const
{
    if (!PlatformFlow || !ActiveFlow.IsValid())
    {
        return false;
    }
    const FGamePlatformFlowSnapshot Snapshot = PlatformFlow->GetSnapshot();
    return Snapshot.Handle.ScopeId == ActiveFlow.ScopeId &&
        Snapshot.Handle.RunId == ActiveFlow.RunId &&
        Snapshot.NodeId == NodeId &&
        Snapshot.NodeGeneration != 0 &&
        Snapshot.State == EGamePlatformFlowState::Running;
}

FGamePlatformFlowNodeToken
UDivineBeastsApplicationFlowSubsystem::GetCurrentNodeToken() const
{
    if (!PlatformFlow || !ActiveFlow.IsValid())
    {
        return {};
    }

    const FGamePlatformFlowSnapshot Snapshot = PlatformFlow->GetSnapshot();
    if (Snapshot.Handle.ScopeId != ActiveFlow.ScopeId ||
        Snapshot.Handle.RunId != ActiveFlow.RunId ||
        Snapshot.NodeGeneration == 0)
    {
        return {};
    }
    return {
        Snapshot.Handle,
        Snapshot.NodeId,
        Snapshot.NodeGeneration
    };
}

bool UDivineBeastsApplicationFlowSubsystem::SubmitCurrentNodeEvent(
    FName ExpectedNodeId,
    FGamePlatformFlowNodeResult Event,
    FGamePlatformResult* OutResult)
{
    if (!PlatformFlow)
    {
        if (OutResult)
        {
            *OutResult = FGamePlatformResult::Failure(
                TEXT("FlowUnavailable"),
                TEXT("游戏平台应用流程服务不可用。"));
        }
        return false;
    }

    const FGamePlatformFlowNodeToken Token = GetCurrentNodeToken();
    if (!Token.IsValid() || Token.NodeId != ExpectedNodeId)
    {
        if (OutResult)
        {
            *OutResult = FGamePlatformResult::Failure(
                TEXT("FlowNodeMismatch"),
                TEXT("事件不属于当前已开始的应用流程节点。"));
        }
        return false;
    }

    FGamePlatformResult Result;
    const bool bAccepted = PlatformFlow->SubmitEvent(
        Token,
        MoveTemp(Event),
        Result);
    if (OutResult)
    {
        *OutResult = Result;
    }
    return bAccepted && Result.IsSuccess();
}

FGamePlatformResult
UDivineBeastsApplicationFlowSubsystem::BeginLoadingForAssignment()
{
    if (!FlowContext || !Loading || !Session)
    {
        return FGamePlatformResult::Failure(
            TEXT("WorldEntryServicesUnavailable"),
            TEXT("世界进入所需的Loading、Session或项目流程上下文不可用。"));
    }

    const FDivineBeastsWorldAssignmentSummary& Assignment =
        FlowContext->GetAssignment();
    if (Assignment.AssignmentId.IsEmpty() ||
        Assignment.GameServerId.IsEmpty() ||
        Assignment.ServerRoleId.IsNone() ||
        Assignment.ExperienceId.IsNone() ||
        Assignment.WorldId.IsNone() ||
        Assignment.MapId.IsNone())
    {
        return FGamePlatformResult::Failure(
            TEXT("WorldAssignmentIncomplete"),
            TEXT("世界分配结果缺少必要身份。"));
    }

    FString Endpoint;
    FString TransferTicket;
    if (!FlowContext->ConsumeConnectionMaterial(
            Endpoint,
            TransferTicket))
    {
        return FGamePlatformResult::Failure(
            TEXT("WorldTransferMaterialMissing"),
            TEXT("世界分配缺少一次性连接地址或转移票据。"));
    }

    ActiveTransferOperationId = FGuid::NewGuid();
    FlowContext->SetLoadingObservationId(ActiveTransferOperationId);
    ViewState.LoadingObservationId = ActiveTransferOperationId;

    LoadingContext = MakeShared<FDivineBeastsProjectLoadingContext>(
        *GetGameInstance(),
        ActiveTransferOperationId,
        Assignment.WorldId,
        Assignment.ExperienceId);

    FGamePlatformResult Result;
    const TWeakPtr<FDivineBeastsProjectLoadingContext> WeakContext =
        LoadingContext;

    const FGamePlatformLoadingRegistration Factory =
        Loading->RegisterTaskFactory(
            ReadinessTaskType,
            [WeakContext]() -> TUniquePtr<IGamePlatformLoadingTask>
            {
                return WeakContext.Pin()
                    ? MakeUnique<FDivineBeastsProjectReadinessTask>(WeakContext)
                    : nullptr;
            },
            Result);
    if (!Result.IsSuccess() || !Factory.IsValid())
    {
        ReleaseLoadingOperation();
        return Result.IsSuccess()
            ? FGamePlatformResult::Failure(
                TEXT("ReadinessFactoryRegistrationFailed"),
                TEXT("项目就绪任务工厂注册失败。"))
            : Result;
    }
    LoadingTaskFactories.Add(Factory);

    const FGamePlatformLoadingRegistration WorldDefinitionFactory =
        Loading->RegisterTaskFactory(
            WorldDefinitionTaskType,
            [WeakContext]() -> TUniquePtr<IGamePlatformLoadingTask>
            {
                return WeakContext.Pin()
                    ? MakeUnique<FDivineBeastsWorldDefinitionLoadingTask>(
                        WeakContext)
                    : nullptr;
            },
            Result);
    if (!Result.IsSuccess() || !WorldDefinitionFactory.IsValid())
    {
        ReleaseLoadingOperation();
        return Result.IsSuccess()
            ? FGamePlatformResult::Failure(
                TEXT("WorldDefinitionFactoryRegistrationFailed"),
                TEXT("世界定义加载任务工厂注册失败。"))
            : Result;
    }
    LoadingTaskFactories.Add(WorldDefinitionFactory);

    FGamePlatformLoadingOperationSpec Spec;
    Spec.Purpose = TEXT("DivineBeastsWorldEntry");

    for (const FName TaskId :
        {TaskSessionAdmission, TaskExpectedWorld, TaskExpectedExperience,
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
    if (!FGamePlatformId::TryParse(
            Assignment.WorldId.ToString(),
            ParsedWorldId))
    {
        ReleaseLoadingOperation();
        return FGamePlatformResult::Failure(
            TEXT("AssignedWorldIdInvalid"),
            TEXT("后端世界分配返回的WorldId不是合法项目逻辑身份。"));
    }

    FGamePlatformLoadingTaskSpec WorldDefinitionTask;
    WorldDefinitionTask.TaskId = TEXT("AssignedWorldDefinition");
    WorldDefinitionTask.TaskType = WorldDefinitionTaskType;
    WorldDefinitionTask.Requiredness =
        EGamePlatformLoadingRequirement::Required;
    WorldDefinitionTask.TimeoutSeconds = Spec.TimeoutSeconds;
    WorldDefinitionTask.Data.DefinitionId = FPrimaryAssetId(
        UGamePlatformPrimaryDataAsset::DefinitionAssetType(),
        FName(*ParsedWorldId.ToString()));
    WorldDefinitionTask.Data.ExpectedClass =
        UDivineBeastsWorldDefinition::StaticClass();
    Spec.Tasks.Add(MoveTemp(WorldDefinitionTask));

    ActiveLoadingOperation = Loading->StartLoadingOperation(
        Spec,
        this,
        Result);
    if (!Result.IsSuccess() || !ActiveLoadingOperation.IsValid())
    {
        ReleaseLoadingOperation();
        return Result.IsSuccess()
            ? FGamePlatformResult::Failure(
                TEXT("WorldLoadingOperationRejected"),
                TEXT("Loading服务未接纳世界进入操作。"))
            : Result;
    }

    LoadingSubscription = Loading->SubscribeLoadingState(
        ActiveLoadingOperation,
        this,
        [WeakThis = TWeakObjectPtr<UDivineBeastsApplicationFlowSubsystem>(this),
         Expected = ActiveLoadingOperation](
            const FGamePlatformLoadingSnapshot& Snapshot)
        {
            if (WeakThis.IsValid() &&
                WeakThis->ActiveLoadingOperation == Expected &&
                Snapshot.Handle == Expected)
            {
                WeakThis->HandleLoadingSnapshot(Snapshot);
            }
        });
    if (!LoadingSubscription.IsValid())
    {
        ReleaseLoadingOperation();
        return FGamePlatformResult::Failure(
            TEXT("WorldLoadingSubscriptionFailed"),
            TEXT("无法订阅当前世界进入Loading操作。"));
    }

    FGamePlatformSessionTransferRequest Request;
    Request.TransferOperationId = ActiveTransferOperationId;
    Request.AssignmentId = Assignment.AssignmentId;
    Request.GameServerId = Assignment.GameServerId;
    Request.ServerRoleId = Assignment.ServerRoleId;
    Request.ExperienceId = Assignment.ExperienceId;
    Request.WorldId = Assignment.WorldId;
    Request.Endpoint = MoveTemp(Endpoint);
    Request.TransferTicket = MoveTemp(TransferTicket);
    Request.TicketId = Assignment.TicketId;
    Request.CharacterId = Assignment.CharacterId;
    Request.SessionId = Assignment.SessionId;
    Request.TimeoutSeconds = Spec.TimeoutSeconds;

    if (!Session->BeginTransfer(Request, Result))
    {
        // 一次性Ticket已经从项目上下文取走；失败也绝不重新塞回上下文复用。
        ReleaseLoadingOperation();
        return Result;
    }

    BroadcastView();
    return FGamePlatformResult::Success();
}

bool UDivineBeastsApplicationFlowSubsystem::NotifyWorldObserved(
    FGuid ObservationId,
    FName ExperienceId,
    FName WorldId,
    UObject* WorldContextObject)
{
    UWorld* ObservedWorld = WorldContextObject && GEngine
        ? GEngine->GetWorldFromContextObject(
            WorldContextObject,
            EGetWorldErrorMode::ReturnNull)
        : nullptr;

    if (!Loading ||
        !ActiveLoadingOperation.IsValid() ||
        !LoadingContext.IsValid() ||
        !LoadingContext->IsExpectedWorld(
            ObservationId,
            ExperienceId,
            WorldId,
            ObservedWorld))
    {
        if (FlowContext &&
            ObservationId == FlowContext->GetLoadingObservationId())
        {
            SetError(EDivineBeastsFlowError::WorldMismatch);
        }
        return false;
    }

    if (!LoadingContext->ObserveWorld(
            ObservationId,
            ExperienceId,
            WorldId,
            ObservedWorld))
    {
        SetError(EDivineBeastsFlowError::WorldMismatch);
        return false;
    }

    if (!ObservedWorld ||
        !Loading->ReportWorldOperable(
            ActiveLoadingOperation,
            *ObservedWorld).IsSuccess())
    {
        SetError(EDivineBeastsFlowError::WorldMismatch);
        return false;
    }

    TryReportLocalFactsToSession();
    TryCompleteWorldReady();
    return true;
}

bool UDivineBeastsApplicationFlowSubsystem::NotifyCharacterBindingReady(
    FGuid ObservationId)
{
    const bool bAccepted =
        MarkLoadingFactReady(ObservationId, TaskCharacterBinding);
    if (bAccepted)
    {
        TryReportLocalFactsToSession();
    }
    return bAccepted;
}

bool UDivineBeastsApplicationFlowSubsystem::NotifyGameplayDataReady(
    FGuid ObservationId)
{
    return MarkLoadingFactReady(ObservationId, TaskGameplayData);
}

bool UDivineBeastsApplicationFlowSubsystem::NotifyProjectReadiness(
    FGuid ObservationId)
{
    return MarkLoadingFactReady(ObservationId, TaskProjectReadiness);
}

bool UDivineBeastsApplicationFlowSubsystem::MarkLoadingFactReady(
    FGuid ObservationId,
    FName TaskId)
{
    DivineBeastsLoading::Fact Fact;
    if (!Loading ||
        !ActiveLoadingOperation.IsValid() ||
        !LoadingContext.IsValid() ||
        ObservationId != ActiveTransferOperationId ||
        !TryGetReadinessFact(TaskId, Fact) ||
        Fact == DivineBeastsLoading::Fact::SessionAdmission ||
        Fact == DivineBeastsLoading::Fact::ExpectedWorld ||
        Fact == DivineBeastsLoading::Fact::ExpectedExperience ||
        !LoadingContext->Observe(ObservationId, Fact))
    {
        return false;
    }

    TryCompleteWorldReady();
    return true;
}

void UDivineBeastsApplicationFlowSubsystem::TryReportLocalFactsToSession()
{
    if (bSynchronizingSessionFacts ||
        !Session ||
        !LoadingContext.IsValid() ||
        !ActiveTransferOperationId.IsValid())
    {
        return;
    }

    const FGamePlatformSessionSnapshot SessionSnapshot =
        Session->GetSnapshot();
    if (SessionSnapshot.TransferOperationId != ActiveTransferOperationId ||
        !SessionSnapshot.Binding.IsValid())
    {
        return;
    }

    TGuardValue<bool> Guard(bSynchronizingSessionFacts, true);
    FGamePlatformResult Ignored;

    if (LoadingContext->Has(DivineBeastsLoading::Fact::ExpectedWorld))
    {
        Session->ReportLocalFact(
            ActiveTransferOperationId,
            EGamePlatformSessionTransferFact::TargetWorldLoaded,
            SessionSnapshot.Binding,
            Ignored);
    }

    if (LoadingContext->Has(DivineBeastsLoading::Fact::CharacterBinding))
    {
        Session->ReportLocalFact(
            ActiveTransferOperationId,
            EGamePlatformSessionTransferFact::ControllerReady,
            SessionSnapshot.Binding,
            Ignored);
    }
}

void UDivineBeastsApplicationFlowSubsystem::TryCompleteWorldReady()
{
    if (!Loading ||
        !Session ||
        !ActiveLoadingOperation.IsValid() ||
        !LoadingContext.IsValid() ||
        !LoadingContext->AllReady() ||
        !LoadingContext->IsWorldOperable() ||
        !Loading->IsReadyToPlay(ActiveLoadingOperation) ||
        !IsCurrentNode(FDivineBeastsFlowNodes::WorldReady()))
    {
        return;
    }

    const FGamePlatformSessionSnapshot SessionSnapshot =
        Session->GetSnapshot();
    if (SessionSnapshot.TransferOperationId != ActiveTransferOperationId ||
        SessionSnapshot.State != EGamePlatformSessionTransferState::Ready ||
        !SessionSnapshot.bAdmissionConfirmed)
    {
        return;
    }

    SubmitCurrentNodeEvent(
        FDivineBeastsFlowNodes::WorldReady(),
        FGamePlatformFlowNodeResult::Success());
}

void UDivineBeastsApplicationFlowSubsystem::FailWorldReady(
    EDivineBeastsFlowError Error)
{
    SetBusy(false);
    SetError(Error);
    SubmitCurrentNodeEvent(
        FDivineBeastsFlowNodes::WorldReady(),
        ProjectFailure(
            Error,
            TEXT("世界进入就绪屏障失败。")));
}

bool UDivineBeastsApplicationFlowSubsystem::ReleaseLoadingOperation()
{
    if (LoadingContext.IsValid())
    {
        LoadingContext->Invalidate();
    }
    if (FlowContext)
    {
        FlowContext->SetLoadingObservationId(FGuid());
        FlowContext->ClearConnectionMaterial();
    }

    ViewState.LoadingObservationId.Invalidate();
    ActiveTransferOperationId.Invalidate();

    bool bSuccess = true;

    if (LoadingSubscription.IsValid())
    {
        if (Loading)
        {
            bSuccess &= Loading->UnsubscribeLoadingState(
                LoadingSubscription);
        }
        else
        {
            bSuccess = false;
        }
        LoadingSubscription = {};
    }

    if (ActiveLoadingOperation.IsValid())
    {
        if (Loading)
        {
            const FGamePlatformResult Released =
                Loading->ReleaseLoadingOperation(
                    ActiveLoadingOperation);
            bSuccess &= Released.IsSuccess();
        }
        else
        {
            bSuccess = false;
        }
        ActiveLoadingOperation = {};
    }

    if (!LoadingTaskFactories.IsEmpty())
    {
        if (!Loading)
        {
            bSuccess = false;
        }
        else
        {
            for (int32 Index = LoadingTaskFactories.Num() - 1;
                 Index >= 0;
                 --Index)
            {
                const FGamePlatformResult Unregistered =
                    Loading->UnregisterTaskFactory(
                        LoadingTaskFactories[Index]);
                if (Unregistered.IsSuccess())
                {
                    LoadingTaskFactories.RemoveAt(Index);
                }
                else
                {
                    bSuccess = false;
                }
            }
        }
    }

    if (LoadingTaskFactories.IsEmpty())
    {
        LoadingContext.Reset();
    }

    return bSuccess;
}

void UDivineBeastsApplicationFlowSubsystem::HandleFlowSnapshot(
    const FGamePlatformFlowSnapshot& Snapshot)
{
    if (!ActiveFlow.IsValid() ||
        Snapshot.Handle.ScopeId != ActiveFlow.ScopeId ||
        Snapshot.Handle.RunId != ActiveFlow.RunId)
    {
        return;
    }

    ViewState.FlowRunId = ToBlueprintCounter(Snapshot.Handle.RunId);
    ViewState.CurrentStep = Snapshot.NodeId;
    ViewState.NodeGeneration = ToBlueprintCounter(Snapshot.NodeGeneration);

    if (Snapshot.NodeId == FDivineBeastsFlowNodes::Authentication() &&
        Snapshot.NodeGeneration != 0 &&
        Snapshot.NodeGeneration != LastAutoLoginNodeGeneration)
    {
        LastAutoLoginNodeGeneration = Snapshot.NodeGeneration;
        if (FlowContext &&
            FlowContext->ShouldTryAutoLogin() &&
            Online &&
            Online->GetSnapshot().State !=
                EGamePlatformAuthState::Authenticated)
        {
            SetBusy(true);
            Online->TryAutoLogin();
        }
        else
        {
            SetBusy(false);
        }
    }
    else if (Snapshot.NodeId ==
                 FDivineBeastsFlowNodes::CharacterEntry() &&
             Snapshot.NodeGeneration != 0)
    {
        SetBusy(false);
    }
    else if (Snapshot.NodeId ==
                 FDivineBeastsFlowNodes::WorldReady() &&
             Snapshot.NodeGeneration != 0)
    {
        SetBusy(true);
        TryCompleteWorldReady();
    }
    else if (Snapshot.NodeId ==
                 FDivineBeastsFlowNodes::InWorld() &&
             Snapshot.NodeGeneration != 0 &&
             Snapshot.NodeGeneration != LastEnteredInWorldNodeGeneration)
    {
        LastEnteredInWorldNodeGeneration = Snapshot.NodeGeneration;
        if (FlowContext)
        {
            FlowContext->ResetRecoveryAttempts();
        }
        ReleaseLoadingOperation();
        SetBusy(false);
        SetError(EDivineBeastsFlowError::None);
        RefreshProjectionFromContext();
        NotifyExtensionsEnteredWorld();
    }
    else if (Snapshot.NodeId ==
                 FDivineBeastsFlowNodes::Recovering() &&
             Snapshot.NodeGeneration != 0)
    {
        SetBusy(true);
    }

    RefreshAllowedActions();
    BroadcastView();
}

void UDivineBeastsApplicationFlowSubsystem::HandleFlowFinished(
    const FGamePlatformFlowSnapshot& Snapshot)
{
    if (!ActiveFlow.IsValid() ||
        Snapshot.Handle.ScopeId != ActiveFlow.ScopeId ||
        Snapshot.Handle.RunId != ActiveFlow.RunId)
    {
        return;
    }

    ActiveFlow = {};
    SetBusy(false);
    ReleaseLoadingOperation();

    if (Snapshot.State == EGamePlatformFlowState::Failed &&
        ViewState.Error == EDivineBeastsFlowError::None)
    {
        SetError(EDivineBeastsFlowError::FlowExecutionFailed);
    }

    RefreshAllowedActions();
    BroadcastView();
}

void UDivineBeastsApplicationFlowSubsystem::HandleAuthSnapshot(
    const FGamePlatformAuthSnapshot& Snapshot)
{
    ViewState.ConnectionSummary =
        StaticEnum<EGamePlatformAuthState>()
            ? StaticEnum<EGamePlatformAuthState>()
                ->GetNameStringByValue(
                    static_cast<int64>(Snapshot.State))
            : FString();

    // 只投影无秘密认证事实，Password/AccessToken/RefreshToken 永不进入流程 ViewState。
    ViewState.bAuthenticated =
        Snapshot.State == EGamePlatformAuthState::Authenticated;

    if (Session)
    {
        Session->SetAuthenticationContext(
            Snapshot.State == EGamePlatformAuthState::Authenticated
                ? Snapshot.AccountId
                : FString(),
            Snapshot.AuthGeneration);
    }

    if (Snapshot.State == EGamePlatformAuthState::Authenticated &&
        IsCurrentNode(FDivineBeastsFlowNodes::Authentication()))
    {
        SetBusy(false);
        SetError(EDivineBeastsFlowError::None);
        SubmitCurrentNodeEvent(
            FDivineBeastsFlowNodes::Authentication(),
            FGamePlatformFlowNodeResult::Success());
        return;
    }

    if (Snapshot.State == EGamePlatformAuthState::Failed &&
        IsCurrentNode(FDivineBeastsFlowNodes::Authentication()))
    {
        SetBusy(false);
        SetError(MapAuthError(Snapshot.Error));
        // 认证节点继续等待人工重试，不把一次错误密码升级成整个ApplicationFlow终态。
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
                ->GetNameStringByValue(
                    static_cast<int64>(Snapshot.State))
            : FString();

    if (Snapshot.bAdmissionConfirmed &&
        LoadingContext.IsValid())
    {
        DivineBeastsLoading::Fact AdmissionFact;
        if (TryGetReadinessFact(
                TaskSessionAdmission,
                AdmissionFact))
        {
            LoadingContext->Observe(
                ActiveTransferOperationId,
                AdmissionFact);
        }
    }

    TryReportLocalFactsToSession();

    if (IsSessionFailureState(Snapshot.State))
    {
        if (IsCurrentNode(FDivineBeastsFlowNodes::WorldReady()))
        {
            FailWorldReady(EDivineBeastsFlowError::AdmissionFailed);
            return;
        }

        if (IsCurrentNode(FDivineBeastsFlowNodes::InWorld()))
        {
            NotifyExtensionsLeavingWorld();
            SetError(EDivineBeastsFlowError::SessionDisconnected);
            SubmitCurrentNodeEvent(
                FDivineBeastsFlowNodes::InWorld(),
                FGamePlatformFlowNodeResult::Success(
                    FDivineBeastsFlowOutcomes::Recover()));
            return;
        }
    }

    TryCompleteWorldReady();
    BroadcastView();
}

void UDivineBeastsApplicationFlowSubsystem::HandleLoadingSnapshot(
    const FGamePlatformLoadingSnapshot& Snapshot)
{
    if (!(Snapshot.Handle == ActiveLoadingOperation))
    {
        return;
    }

    // EGamePlatformLoadingState（游戏平台加载状态）是轻量普通枚举而非UENUM（反射枚举），
    // 因此不能使用StaticEnum。这里使用无反射、无查表的switch，仅在加载状态事件到达时构造一次短字符串，
    // 避免为了UI投影把平台热路径枚举改造成反射类型，也不会产生逐帧字符串分配。
    switch (Snapshot.State)
    {
    case EGamePlatformLoadingState::Idle:
        ViewState.LoadingSummary = TEXT("Idle");
        break;
    case EGamePlatformLoadingState::Running:
        ViewState.LoadingSummary = TEXT("Running");
        break;
    case EGamePlatformLoadingState::Ready:
        ViewState.LoadingSummary = TEXT("Ready");
        break;
    case EGamePlatformLoadingState::DegradedReady:
        ViewState.LoadingSummary = TEXT("DegradedReady");
        break;
    case EGamePlatformLoadingState::Failed:
        ViewState.LoadingSummary = TEXT("Failed");
        break;
    case EGamePlatformLoadingState::Cancelled:
        ViewState.LoadingSummary = TEXT("Cancelled");
        break;
    case EGamePlatformLoadingState::TimedOut:
        ViewState.LoadingSummary = TEXT("TimedOut");
        break;
    default:
        ViewState.LoadingSummary.Reset();
        break;
    }

    if (Snapshot.State == EGamePlatformLoadingState::Ready ||
        Snapshot.State == EGamePlatformLoadingState::DegradedReady)
    {
        TryCompleteWorldReady();
    }
    else if (Snapshot.State == EGamePlatformLoadingState::Failed ||
             Snapshot.State == EGamePlatformLoadingState::TimedOut ||
             Snapshot.State == EGamePlatformLoadingState::Cancelled)
    {
        FailWorldReady(
            EDivineBeastsFlowError::WorldReadinessTimedOut);
        return;
    }

    BroadcastView();
}

void UDivineBeastsApplicationFlowSubsystem::RefreshProjectionFromContext()
{
    if (!FlowContext)
    {
        return;
    }

    // 这些复制只发生在领域事件/节点终态，不发生在Tick热路径。
    ViewState.Profile = FlowContext->GetProfile();
    ViewState.CharacterRoster = FlowContext->GetCharacterRoster();
    ViewState.SelectedCharacter = FlowContext->GetSelectedCharacter();
    ViewState.bHasSelectedCharacter =
        FlowContext->HasSelectedCharacter();
    ViewState.Assignment = FlowContext->GetAssignment();
    ViewState.LoadingObservationId =
        FlowContext->GetLoadingObservationId();
    RefreshAllowedActions();
    BroadcastView();
}

void UDivineBeastsApplicationFlowSubsystem::ResetProjection()
{
    ViewState = FDivineBeastsFlowViewState();
    RefreshAllowedActions();
    BroadcastView();
}

void UDivineBeastsApplicationFlowSubsystem::SetError(
    EDivineBeastsFlowError Error)
{
    if (ViewState.Error == Error)
    {
        return;
    }
    ViewState.Error = Error;
    RefreshAllowedActions();
    BroadcastView();
}

void UDivineBeastsApplicationFlowSubsystem::SetBusy(bool bBusy)
{
    if (ViewState.bBusy == bBusy)
    {
        return;
    }
    ViewState.bBusy = bBusy;
    RefreshAllowedActions();
    BroadcastView();
}

void UDivineBeastsApplicationFlowSubsystem::RefreshAllowedActions()
{
    TArray<EDivineBeastsFlowAction> Next;

    if (IsCurrentNode(FDivineBeastsFlowNodes::Authentication()))
    {
        Next = {
            EDivineBeastsFlowAction::TryAutoLogin,
            EDivineBeastsFlowAction::Login
        };
    }
    else if (IsCurrentNode(FDivineBeastsFlowNodes::CharacterEntry()))
    {
        Next = {
            EDivineBeastsFlowAction::CreateCharacter,
            EDivineBeastsFlowAction::SelectPersistentCharacter,
            EDivineBeastsFlowAction::Logout
        };
    }
    else if (IsCurrentNode(FDivineBeastsFlowNodes::InWorld()))
    {
        Next = {
            EDivineBeastsFlowAction::RequestExperience,
            EDivineBeastsFlowAction::Logout
        };
    }
    else if (!ActiveFlow.IsValid() &&
             ViewState.Error != EDivineBeastsFlowError::None)
    {
        Next = {
            EDivineBeastsFlowAction::Retry,
            EDivineBeastsFlowAction::Logout
        };
    }

    ViewState.AllowedActions = MoveTemp(Next);
}

void UDivineBeastsApplicationFlowSubsystem::BroadcastView()
{
    ViewStateChanged.Broadcast(ViewState);
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
    ExtensionOrder.Add(ExtensionId);
    ExtensionOrder.Sort(
        [](const FName& Left, const FName& Right)
        {
            return Left.LexicalLess(Right);
        });
    return true;
}

bool UDivineBeastsApplicationFlowSubsystem::UnregisterExtension(
    FName ExtensionId)
{
    if (Extensions.Remove(ExtensionId) == 0)
    {
        return false;
    }
    ExtensionOrder.RemoveSingle(ExtensionId);
    return true;
}

void UDivineBeastsApplicationFlowSubsystem::NotifyExtensionsEnteredWorld()
{
    // 注册时已经排序，世界切换热路径不再分配临时Key数组或重复排序。
    for (const FName Id : ExtensionOrder)
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
    for (const FName Id : ExtensionOrder)
    {
        const TSharedPtr<IDivineBeastsApplicationFlowExtension>* Extension =
            Extensions.Find(Id);
        if (Extension && Extension->IsValid())
        {
            (*Extension)->OnLeavingInWorld(*this);
        }
    }
}

bool UDivineBeastsApplicationFlowSubsystem::RequestPostMatchReturnToWorld()
{
    return RequestWorldAssignment(
        FDivineBeastsProjectCatalog::GetOpenWorldHubExperience());
}

void UDivineBeastsApplicationFlowSubsystem::LogoutAndRestart()
{
    bRestartAfterLogout = true;
    ++StartRequestGeneration;

    if (Backend.IsValid())
    {
        Backend->CancelAll();
    }
    ReleaseLoadingOperation();

    if (Session)
    {
        FGamePlatformResult Ignored;
        Session->CancelTransfer(Ignored);
    }

    if (PlatformFlow && ActiveFlow.IsValid())
    {
        PlatformFlow->Cancel(ActiveFlow);
        ActiveFlow = {};
    }

    ReleasePendingFlowDefinitionLease();
    FlowContext = nullptr;
    ResetProjection();

    if (Online)
    {
        Online->Logout();
    }
    else
    {
        bRestartAfterLogout = false;
        SetError(EDivineBeastsFlowError::FlowNotInitialized);
    }
}
