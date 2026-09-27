#include "DivineBeastsUIClientSubsystem.h"

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
#include "ViewModels/DivineBeastsUIViewModel.h"

void UDivineBeastsUIClientSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UGamePlatformUIManagerSubsystem>();

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        PlatformUI = LocalPlayer->GetSubsystem<UGamePlatformUIManagerSubsystem>();
    }

    RegisterDefaultScreenDefinitions();
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
    return PlatformUI &&
        PlatformUI->InstallRootLayoutClass(RootLayoutClass);
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
    UDivineBeastsUIViewModel* ViewModel =
        NewObject<UDivineBeastsUIViewModel>(this);
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
