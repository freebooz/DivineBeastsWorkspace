#include "DivineBeastsUIClientSubsystem.h"

#include "Adapters/Application/DivineBeastsApplicationUIAdapter.h"
#include "Definitions/GamePlatformUIScreenDefinition.h"
#include "Dialogs/GamePlatformToastWidget.h"
#include "Engine/LocalPlayer.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "Loading/GamePlatformLoadingScreenService.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Screens/DivineBeastsUIScreenCatalog.h"
#include "Screens/GamePlatformHUDWidget.h"
#include "Screens/GamePlatformUIScreen.h"
#include "Routing/DivineBeastsUIRoutingPolicy.h"
#include "ViewModels/Boot/DivineBeastsBootViewModel.h"
#include "ViewModels/Characters/DivineBeastsCharacterCreateViewModel.h"
#include "ViewModels/Characters/DivineBeastsCharacterSelectViewModel.h"
#include "ViewModels/DivineBeastsUIViewModel.h"
#include "ViewModels/Loading/DivineBeastsLoadingViewModel.h"
#include "ViewModels/Login/DivineBeastsLoginViewModel.h"

void UDivineBeastsUIClientSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UGamePlatformUIManagerSubsystem>();

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        PlatformUI = LocalPlayer->GetSubsystem<UGamePlatformUIManagerSubsystem>();
    }

    if (PlatformUI)
    {
        PlatformUI->OnScreenOpened.AddDynamic(
            this,
            &UDivineBeastsUIClientSubsystem::HandlePrimaryScreenOpened);
        PlatformUI->OnScreenOpenFailed.AddDynamic(
            this,
            &UDivineBeastsUIClientSubsystem::HandlePrimaryScreenOpenFailed);
        PlatformUI->OnScreenClosed.AddDynamic(
            this,
            &UDivineBeastsUIClientSubsystem::HandlePrimaryScreenClosed);
    }

    RegisterDefaultScreenDefinitions();

    // DBAClient 内部由项目 UI 层单向依赖项目 ApplicationFlow，
    // 不要求流程模块反向认识任何 Widget/ViewModel 类型。
    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        ApplicationAdapter =
            NewObject<UDivineBeastsApplicationUIAdapter>(this);
        if (!ApplicationAdapter ||
            !ApplicationAdapter->Initialize(*LocalPlayer) ||
            !RegisterApplicationContract(
                TEXT("DivineBeasts.ApplicationFlow"),
                ApplicationAdapter,
                ApplicationAdapter).IsValid())
        {
            if (ApplicationAdapter)
            {
                ApplicationAdapter->Shutdown();
            }
            ApplicationAdapter = nullptr;
        }
    }
}

void UDivineBeastsUIClientSubsystem::Deinitialize()
{
    if (PlatformUI && LoadingToken.IsValid())
    {
        if (UGamePlatformLoadingScreenService* LoadingService =
            PlatformUI->GetLoadingScreenService())
        {
            LoadingService->ReleaseToken(LoadingToken);
        }
        LoadingToken = {};
    }
    DetachContract();
    if (ApplicationAdapter)
    {
        ApplicationAdapter->Shutdown();
        ApplicationAdapter = nullptr;
    }
    if (PlatformUI)
    {
        PlatformUI->OnScreenOpened.RemoveDynamic(
            this,
            &UDivineBeastsUIClientSubsystem::HandlePrimaryScreenOpened);
        PlatformUI->OnScreenOpenFailed.RemoveDynamic(
            this,
            &UDivineBeastsUIClientSubsystem::HandlePrimaryScreenOpenFailed);
        PlatformUI->OnScreenClosed.RemoveDynamic(
            this,
            &UDivineBeastsUIClientSubsystem::HandlePrimaryScreenClosed);

        if (OpeningPrimaryRequestId.IsValid())
        {
            PlatformUI->CancelOpen(OpeningPrimaryRequestId);
        }
        if (UGamePlatformUIScreen* Active = ActivePrimaryScreen.Get())
        {
            PlatformUI->CloseScreen(Active);
        }
    }
    OpeningPrimaryRequestId.Invalidate();
    OpeningPrimaryScreenId = NAME_None;
    ActivePrimaryScreen.Reset();
    ActivePrimaryScreenId = NAME_None;

    UnregisterDefaultScreenDefinitions();
    PlatformUI = nullptr;
    StateChanged.Clear();
    Super::Deinitialize();
}

FDivineBeastsUIContractHandle
UDivineBeastsUIClientSubsystem::RegisterApplicationContract(
    FName AdapterId,
    UObject* InQuerySourceObject,
    UObject* InCommandPortObject)
{
    FDivineBeastsUIContractHandle Result;

    if (AdapterId.IsNone() ||
        !IsValid(InQuerySourceObject) ||
        !IsValid(InCommandPortObject) ||
        ContractHandle.IsValid() ||
        !InQuerySourceObject->GetClass()->ImplementsInterface(
            UDivineBeastsUIQuerySource::StaticClass()) ||
        !InCommandPortObject->GetClass()->ImplementsInterface(
            UDivineBeastsUICommandPort::StaticClass()))
    {
        return Result;
    }

    IDivineBeastsUIQuerySource* Query =
        Cast<IDivineBeastsUIQuerySource>(InQuerySourceObject);
    if (!Query)
    {
        return Result;
    }

    ContractAdapterId = AdapterId;
    QuerySourceObject = InQuerySourceObject;
    CommandPortObject = InCommandPortObject;
    ContractHandle.Id = FGuid::NewGuid();
    Result = ContractHandle;

    QueryStateHandle = Query->OnUIViewStateChanged().AddUObject(
        this,
        &UDivineBeastsUIClientSubsystem::HandleViewStateChanged);

    PullInitialState();
    return Result;
}

bool UDivineBeastsUIClientSubsystem::UnregisterApplicationContract(
    const FDivineBeastsUIContractHandle& Handle)
{
    if (!Handle.IsValid() ||
        !ContractHandle.IsValid() ||
        Handle.Id != ContractHandle.Id)
    {
        return false;
    }

    DetachContract();
    return true;
}

TArray<FName> UDivineBeastsUIClientSubsystem::GetRegisteredScreenIds() const
{
    return FDivineBeastsUIScreenCatalog::GetScreenIds();
}

FName UDivineBeastsUIClientSubsystem::GetRecommendedPrimaryScreenId() const
{
    return FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(ViewState);
}

FGamePlatformUIAsyncRequest
UDivineBeastsUIClientSubsystem::OpenScreen(
    FName ScreenId,
    UDivineBeastsUIViewModel* ViewModel)
{
    return PlatformUI
        ? PlatformUI->OpenScreenAsync(ScreenId, ViewModel)
        : FGamePlatformUIAsyncRequest();
}

bool UDivineBeastsUIClientSubsystem::CancelScreenOpen(FGuid RequestId)
{
    return PlatformUI && PlatformUI->CancelOpen(RequestId);
}

bool UDivineBeastsUIClientSubsystem::CloseScreen(
    UGamePlatformUIScreen* Screen)
{
    return PlatformUI && PlatformUI->CloseScreen(Screen);
}

bool UDivineBeastsUIClientSubsystem::InstallRootLayoutClass(
    TSubclassOf<UGamePlatformUILayerStack> RootLayoutClass)
{
    const bool bInstalled =
        PlatformUI &&
        PlatformUI->InstallRootLayoutClass(RootLayoutClass);
    if (bInstalled)
    {
        // RootLayout 是页面真正可打开的前置条件。安装成功后立即消费此前已到达的流程状态，
        // 不要求业务层再发送一次伪状态事件。
        SyncPrimaryScreen();
    }
    return bInstalled;
}

UGamePlatformLoadingScreenService*
UDivineBeastsUIClientSubsystem::GetLoadingScreenService() const
{
    return PlatformUI ? PlatformUI->GetLoadingScreenService() : nullptr;
}

bool UDivineBeastsUIClientSubsystem::AttachHUDWidget(
    UGamePlatformHUDWidget* Widget)
{
    return PlatformUI && PlatformUI->AttachHUDWidget(Widget);
}

bool UDivineBeastsUIClientSubsystem::AttachToastWidget(
    UGamePlatformToastWidget* Widget)
{
    return PlatformUI && PlatformUI->AttachToastWidget(Widget);
}

UDivineBeastsUIViewModel*
UDivineBeastsUIClientSubsystem::CreateViewModel(FName ScreenId)
{
    UDivineBeastsUIViewModel* ViewModel = nullptr;

    // P0 页面使用类型明确的 ViewModel；其他现有页面继续复用兼容 ViewModel，
    // 保持增量迁移，不在本阶段一次性重写全部业务页面。
    if (ScreenId == TEXT("UI.Screen.Login"))
    {
        ViewModel = NewObject<UDivineBeastsLoginViewModel>(this);
    }
    else if (ScreenId == TEXT("UI.Screen.CharacterSelect"))
    {
        ViewModel = NewObject<UDivineBeastsCharacterSelectViewModel>(this);
    }
    else if (ScreenId == TEXT("UI.Screen.CharacterCreate"))
    {
        ViewModel = NewObject<UDivineBeastsCharacterCreateViewModel>(this);
    }
    else if (ScreenId == TEXT("UI.Screen.Boot"))
    {
        UDivineBeastsBootViewModel* BootViewModel =
            NewObject<UDivineBeastsBootViewModel>(this);
        if (BootViewModel)
        {
            // Boot 与世界切换都复用同一个平台 Loading Service。
            // 若当前阶段没有可量化进度，快照会保持 Progress < 0，
            // 蓝图只能显示不确定进度表现，禁止伪造百分比。
            BootViewModel->InitializeLoadingService(
                GetLoadingScreenService());
        }
        ViewModel = BootViewModel;
    }
    else if (ScreenId == TEXT("UI.Screen.LoadingTravel"))
    {
        UDivineBeastsLoadingViewModel* LoadingViewModel =
            NewObject<UDivineBeastsLoadingViewModel>(this);
        if (LoadingViewModel)
        {
            // Loading Service 属于当前 LocalPlayer 的平台 UI Manager，
            // ViewModel 只持有瞬态引用并在页面激活期间订阅事件。
            LoadingViewModel->InitializeLoadingService(
                GetLoadingScreenService());
        }
        ViewModel = LoadingViewModel;
    }
    else
    {
        ViewModel = NewObject<UDivineBeastsUIViewModel>(this);
    }

    if (ViewModel)
    {
        ViewModel->InitializeForScreen(this, ScreenId);
    }

    return ViewModel;
}

void UDivineBeastsUIClientSubsystem::SubmitCommand(
    FDivineBeastsUICommand Command,
    TFunction<void(const FDivineBeastsUICommandResult&)> Completion)
{
    FDivineBeastsUICommandResult Rejected;
    Rejected.RequestId = Command.RequestId;

    FString Error;
    if (!Command.IsValid(Error) || !CommandPortObject.IsValid())
    {
        Rejected.ErrorCode = TEXT("UI.Command.Invalid");
        if (Completion)
        {
            Completion(Rejected);
        }
        return;
    }

    IDivineBeastsUICommandPort* Port =
        Cast<IDivineBeastsUICommandPort>(CommandPortObject.Get());
    if (!Port)
    {
        Rejected.ErrorCode = TEXT("UI.Command.PortUnavailable");
        if (Completion)
        {
            Completion(Rejected);
        }
        return;
    }

    Port->SubmitUICommand(Command, MoveTemp(Completion));
}

bool UDivineBeastsUIClientSubsystem::CancelCommand(const FGuid& RequestId)
{
    if (!RequestId.IsValid() || !CommandPortObject.IsValid())
    {
        return false;
    }

    IDivineBeastsUICommandPort* Port =
        Cast<IDivineBeastsUICommandPort>(CommandPortObject.Get());
    return Port && Port->CancelUICommand(RequestId);
}

void UDivineBeastsUIClientSubsystem::RegisterDefaultScreenDefinitions()
{
    if (!PlatformUI)
    {
        return;
    }

    for (const FDivineBeastsUISurfaceDescriptor& Surface :
         FDivineBeastsUIScreenCatalog::GetSurfaces())
    {
        if (Surface.Kind != EDivineBeastsUISurfaceKind::Screen ||
            Surface.SurfaceId.IsNone() ||
            Surface.WidgetClassPath.IsEmpty())
        {
            continue;
        }

        UGamePlatformUIScreenDefinition* Definition =
            NewObject<UGamePlatformUIScreenDefinition>(this);
        Definition->ScreenId = Surface.SurfaceId;
        Definition->WidgetClass =
            TSoftClassPtr<UGamePlatformUIScreen>(
                FSoftObjectPath(Surface.WidgetClassPath));
        Definition->Layer = Surface.Layer;
        Definition->InputMode = Surface.InputMode;
        Definition->PausePolicy = Surface.PausePolicy;
        Definition->Transition = Surface.Transition;
        Definition->DefaultFocusWidgetName = Surface.DefaultFocusWidgetName;
        Definition->bSurvivesTravel = Surface.bSurvivesTravel;

        if (!Surface.AndroidWidgetClassPath.IsEmpty())
        {
            Definition->PlatformWidgetVariants.Add(
                TEXT("Android"),
                TSoftClassPtr<UGamePlatformUIScreen>(
                    FSoftObjectPath(Surface.AndroidWidgetClassPath)));
        }

        if (PlatformUI->RegisterScreenDefinition(Definition))
        {
            RegisteredDefinitions.Add(Definition);
        }
    }
}

void UDivineBeastsUIClientSubsystem::UnregisterDefaultScreenDefinitions()
{
    if (PlatformUI)
    {
        for (const UGamePlatformUIScreenDefinition* Definition :
             RegisteredDefinitions)
        {
            if (IsValid(Definition))
            {
                PlatformUI->UnregisterScreenDefinition(
                    Definition->ScreenId);
            }
        }
    }
    RegisteredDefinitions.Reset();
}

void UDivineBeastsUIClientSubsystem::HandleViewStateChanged(
    const FDivineBeastsUIViewState& NewState)
{
    const bool bNewRun =
        NewState.StateRunId.IsValid() &&
        NewState.StateRunId != ViewState.StateRunId;

    if (!bNewRun &&
        NewState.StateRunId == ViewState.StateRunId &&
        NewState.Revision < ViewState.Revision)
    {
        return;
    }

    ViewState = NewState;
    StateChanged.Broadcast(ViewState);
    SyncLoadingService();
    SyncPrimaryScreen();
}

void UDivineBeastsUIClientSubsystem::PullInitialState()
{
    if (!QuerySourceObject.IsValid())
    {
        return;
    }

    if (IDivineBeastsUIQuerySource* Query =
        Cast<IDivineBeastsUIQuerySource>(QuerySourceObject.Get()))
    {
        HandleViewStateChanged(Query->GetUIViewState());
    }
}

void UDivineBeastsUIClientSubsystem::SyncLoadingService()
{
    if (!PlatformUI)
    {
        return;
    }

    UGamePlatformLoadingScreenService* LoadingService =
        PlatformUI->GetLoadingScreenService();
    if (!LoadingService)
    {
        return;
    }

    if (ViewState.Loading.bIsLoading)
    {
        if (!LoadingToken.IsValid())
        {
            LoadingToken = LoadingService->AcquireToken(
                ViewState.Loading.Stage,
                ViewState.Loading.Progress);
        }
        else
        {
            LoadingService->UpdateToken(
                LoadingToken,
                ViewState.Loading.Stage,
                ViewState.Loading.Progress);
        }
    }
    else if (LoadingToken.IsValid())
    {
        LoadingService->ReleaseToken(LoadingToken);
        LoadingToken = {};
    }
}

void UDivineBeastsUIClientSubsystem::SyncPrimaryScreen()
{
    if (bSynchronizingPrimaryScreen ||
        !PlatformUI ||
        !PlatformUI->GetRootLayout())
    {
        return;
    }

    TGuardValue<bool> Guard(bSynchronizingPrimaryScreen, true);

    const FName DesiredScreenId =
        FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(ViewState);

    if (DesiredScreenId.IsNone())
    {
        if (OpeningPrimaryRequestId.IsValid())
        {
            PlatformUI->CancelOpen(OpeningPrimaryRequestId);
        }
        OpeningPrimaryRequestId.Invalidate();
        OpeningPrimaryScreenId = NAME_None;

        if (UGamePlatformUIScreen* Active = ActivePrimaryScreen.Get())
        {
            ActivePrimaryScreen.Reset();
            ActivePrimaryScreenId = NAME_None;
            PlatformUI->CloseScreen(Active);
        }
        return;
    }

    if (ActivePrimaryScreen.IsValid() &&
        ActivePrimaryScreenId == DesiredScreenId)
    {
        return;
    }

    if (OpeningPrimaryScreenId == DesiredScreenId)
    {
        return;
    }

    if (OpeningPrimaryRequestId.IsValid())
    {
        PlatformUI->CancelOpen(OpeningPrimaryRequestId);
        OpeningPrimaryRequestId.Invalidate();
        OpeningPrimaryScreenId = NAME_None;
    }

    if (UGamePlatformUIScreen* Active = ActivePrimaryScreen.Get())
    {
        ActivePrimaryScreen.Reset();
        ActivePrimaryScreenId = NAME_None;
        PlatformUI->CloseScreen(Active);
    }

    if (!PlatformUI->HasScreenDefinition(DesiredScreenId))
    {
        return;
    }

    UDivineBeastsUIViewModel* ViewModel =
        CreateViewModel(DesiredScreenId);
    if (!ViewModel)
    {
        return;
    }

    // 先记录 ScreenId，再调用平台异步打开。平台可能在参数/Root/Layer无效时同步广播失败；
    // 失败处理会立即清空 OpeningPrimaryScreenId，因此调用返回后不能盲目记录一个已终结请求。
    OpeningPrimaryScreenId = DesiredScreenId;
    const FGamePlatformUIAsyncRequest Request =
        PlatformUI->OpenScreenAsync(
            DesiredScreenId,
            ViewModel);

    if (OpeningPrimaryScreenId == DesiredScreenId)
    {
        OpeningPrimaryRequestId = Request.RequestId;
    }
}

void UDivineBeastsUIClientSubsystem::HandlePrimaryScreenOpened(
    FGuid RequestId,
    FName ScreenId,
    UGamePlatformUIScreen* Screen)
{
    if (ScreenId != OpeningPrimaryScreenId ||
        (OpeningPrimaryRequestId.IsValid() &&
         RequestId != OpeningPrimaryRequestId))
    {
        return;
    }

    OpeningPrimaryRequestId.Invalidate();
    OpeningPrimaryScreenId = NAME_None;
    ActivePrimaryScreen = Screen;
    ActivePrimaryScreenId = IsValid(Screen)
        ? ScreenId
        : NAME_None;
}

void UDivineBeastsUIClientSubsystem::HandlePrimaryScreenOpenFailed(
    FGuid RequestId,
    FName ScreenId,
    FText Reason)
{
    // Reason 只由平台用于界面/诊断展示。本层不把资源路径或加载内部错误写入业务状态。
    if (ScreenId != OpeningPrimaryScreenId ||
        (OpeningPrimaryRequestId.IsValid() &&
         RequestId != OpeningPrimaryRequestId))
    {
        return;
    }

    OpeningPrimaryRequestId.Invalidate();
    OpeningPrimaryScreenId = NAME_None;
}

void UDivineBeastsUIClientSubsystem::HandlePrimaryScreenClosed(
    FName ScreenId)
{
    if (ScreenId != ActivePrimaryScreenId)
    {
        return;
    }

    ActivePrimaryScreen.Reset();
    ActivePrimaryScreenId = NAME_None;
}

void UDivineBeastsUIClientSubsystem::DetachContract()
{
    if (QuerySourceObject.IsValid() && QueryStateHandle.IsValid())
    {
        if (IDivineBeastsUIQuerySource* Query =
            Cast<IDivineBeastsUIQuerySource>(QuerySourceObject.Get()))
        {
            Query->OnUIViewStateChanged().Remove(QueryStateHandle);
        }
    }

    QueryStateHandle.Reset();
    QuerySourceObject.Reset();
    CommandPortObject.Reset();
    ContractAdapterId = NAME_None;
    ContractHandle = {};
    if (PlatformUI && LoadingToken.IsValid())
    {
        if (UGamePlatformLoadingScreenService* LoadingService =
            PlatformUI->GetLoadingScreenService())
        {
            LoadingService->ReleaseToken(LoadingToken);
        }
        LoadingToken = {};
    }
    ViewState = FDivineBeastsUIViewState();
}
