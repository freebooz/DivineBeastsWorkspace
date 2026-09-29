#include "Adapters/Application/DivineBeastsApplicationUIAdapter.h"

#include "DivineBeastsApplicationFlowSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Flow/DivineBeastsFlowNodes.h"
#include "Flow/DivineBeastsFlowTypes.h"
#include "Localization/DivineBeastsUILocalization.h"

namespace
{
FName FlowErrorToName(EDivineBeastsFlowError Error)
{
    if (Error == EDivineBeastsFlowError::None)
    {
        return NAME_None;
    }

    const UEnum* Enum = StaticEnum<EDivineBeastsFlowError>();
    return Enum
        ? FName(*Enum->GetNameStringByValue(static_cast<int64>(Error)))
        : FName(TEXT("Unknown"));
}

FName UICommandName(EDivineBeastsUICommandType Type)
{
    switch (Type)
    {
    case EDivineBeastsUICommandType::TryAutoLogin: return TEXT("TryAutoLogin");
    case EDivineBeastsUICommandType::Login: return TEXT("Login");
    case EDivineBeastsUICommandType::Refresh: return TEXT("Refresh");
    case EDivineBeastsUICommandType::Retry: return TEXT("Retry");
    case EDivineBeastsUICommandType::CreateCharacter: return TEXT("CreateCharacter");
    case EDivineBeastsUICommandType::SelectPersistentCharacter: return TEXT("SelectPersistentCharacter");
    case EDivineBeastsUICommandType::RequestWorld: return TEXT("RequestWorld");
    case EDivineBeastsUICommandType::Logout: return TEXT("Logout");
    case EDivineBeastsUICommandType::TrainingReset: return TEXT("TrainingReset");
    case EDivineBeastsUICommandType::StartMatchmaking: return TEXT("StartMatchmaking");
    case EDivineBeastsUICommandType::CancelMatchmaking: return TEXT("CancelMatchmaking");
    case EDivineBeastsUICommandType::ArenaSelectHero: return TEXT("ArenaSelectHero");
    case EDivineBeastsUICommandType::ArenaReady: return TEXT("ArenaReady");
    case EDivineBeastsUICommandType::PostMatchReturnToWorld: return TEXT("PostMatchReturnToWorld");
    default: return NAME_None;
    }
}

FName FlowActionToCommandName(EDivineBeastsFlowAction Action)
{
    switch (Action)
    {
    case EDivineBeastsFlowAction::TryAutoLogin: return TEXT("TryAutoLogin");
    case EDivineBeastsFlowAction::Login: return TEXT("Login");
    case EDivineBeastsFlowAction::Refresh: return TEXT("Refresh");
    case EDivineBeastsFlowAction::Retry: return TEXT("Retry");
    case EDivineBeastsFlowAction::CreateCharacter: return TEXT("CreateCharacter");
    case EDivineBeastsFlowAction::SelectPersistentCharacter: return TEXT("SelectPersistentCharacter");
    case EDivineBeastsFlowAction::RequestExperience: return TEXT("RequestWorld");
    case EDivineBeastsFlowAction::Logout: return TEXT("Logout");
    default: return NAME_None;
    }
}

bool IsAuthenticationRecoverableError(EDivineBeastsFlowError Error)
{
    return Error == EDivineBeastsFlowError::InvalidCredentials ||
        Error == EDivineBeastsFlowError::AccountLocked ||
        Error == EDivineBeastsFlowError::AuthExpired ||
        Error == EDivineBeastsFlowError::NetworkUnavailable ||
        Error == EDivineBeastsFlowError::Maintenance;
}
}

bool UDivineBeastsApplicationUIAdapter::Initialize(ULocalPlayer& LocalPlayer)
{
    Shutdown();

    UGameInstance* GameInstance = LocalPlayer.GetGameInstance();
    if (!GameInstance)
    {
        return false;
    }

    Flow = GameInstance->GetSubsystem<UDivineBeastsApplicationFlowSubsystem>();
    if (!Flow)
    {
        return false;
    }

    FlowStateHandle = Flow->OnViewStateChanged().AddUObject(
        this,
        &UDivineBeastsApplicationUIAdapter::HandleFlowStateChanged);

    HandleFlowStateChanged(Flow->GetViewState());

    // 主流程属于 GameInstance，而不是 LocalPlayer。
    // 多本地玩家场景只允许第一个 LocalPlayer 触发首次启动，避免并发 StartFlow 互相失效。
    if (GameInstance->GetFirstGamePlayer() == &LocalPlayer &&
        Flow->GetViewState().FlowRunId == 0 &&
        Flow->GetViewState().CurrentStep.IsNone())
    {
        Flow->StartFlow(true);
    }

    return true;
}

void UDivineBeastsApplicationUIAdapter::Shutdown()
{
    if (Flow && FlowStateHandle.IsValid())
    {
        Flow->OnViewStateChanged().Remove(FlowStateHandle);
    }

    FlowStateHandle.Reset();
    Flow = nullptr;
    ViewState = FDivineBeastsUIViewState();
    LastFlowRunId = 0;
    StateChanged.Clear();
}

void UDivineBeastsApplicationUIAdapter::HandleFlowStateChanged(
    const FDivineBeastsFlowViewState& FlowState)
{
    ViewState = BuildViewState(FlowState);
    StateChanged.Broadcast(ViewState);
}

FDivineBeastsUIViewState UDivineBeastsApplicationUIAdapter::BuildViewState(
    const FDivineBeastsFlowViewState& FlowState)
{
    FDivineBeastsUIViewState Result;

    if (!ViewState.StateRunId.IsValid() ||
        LastFlowRunId != FlowState.FlowRunId)
    {
        LastFlowRunId = FlowState.FlowRunId;
        Result.StateRunId = FGuid::NewGuid();
        Result.Revision = 1;
    }
    else
    {
        Result.StateRunId = ViewState.StateRunId;
        Result.Revision = ViewState.Revision + 1;
    }

    Result.CurrentStep = FlowState.CurrentStep;
    Result.bBusy = FlowState.bBusy;
    Result.bAuthenticated = FlowState.bAuthenticated;

    Result.ErrorCode = FlowErrorToName(FlowState.Error);
    Result.ErrorText =
        FDivineBeastsUILocalization::ErrorCodeToText(Result.ErrorCode);
    Result.bMaintenance =
        FlowState.Error == EDivineBeastsFlowError::Maintenance;

    const bool bAuthenticationStep =
        FlowState.CurrentStep == FDivineBeastsFlowNodes::Authentication();
    const bool bBootStep =
        FlowState.CurrentStep.IsNone() ||
        FlowState.CurrentStep == FDivineBeastsFlowNodes::Boot() ||
        FlowState.CurrentStep == FDivineBeastsFlowNodes::Initialize();

    if (FlowState.Error != EDivineBeastsFlowError::None &&
        !(bAuthenticationStep &&
          IsAuthenticationRecoverableError(FlowState.Error)))
    {
        Result.PageState = EDivineBeastsUIPageState::Error;
    }
    else if (bBootStep)
    {
        Result.PageState = EDivineBeastsUIPageState::Loading;
    }
    else if (FlowState.bBusy)
    {
        Result.PageState = EDivineBeastsUIPageState::Submitting;
    }
    else
    {
        Result.PageState = EDivineBeastsUIPageState::Ready;
    }

    Result.Characters.Reserve(FlowState.CharacterRoster.Num());
    for (const FDivineBeastsCharacterSummary& Character :
         FlowState.CharacterRoster)
    {
        FDivineBeastsUICharacterItem Item;
        Item.CharacterId = Character.CharacterId;
        Item.HeroDefinitionId = Character.HeroDefinitionId;
        Item.DisplayName = FText::FromString(Character.CharacterName);
        Item.AppearanceProfileId = Character.AppearanceProfileId;
        Item.Status = Character.Status;
        Item.bEnabled = Character.Status == TEXT("Active");
        Item.bSelected =
            FlowState.bHasSelectedCharacter &&
            FlowState.SelectedCharacter.CharacterId ==
                Character.CharacterId;
        Result.Characters.Add(MoveTemp(Item));
    }

    // 角色创建候选通过 ApplicationFlow（应用流程）的项目 DTO 获取；
    // 本 UI 模块不直接依赖 GamePlatformCharacter（平台角色插件）的 Provider 实现类型。
    if (Flow)
    {
        TArray<FDivineBeastsCharacterCreationOption> CreateOptions;
        if (Flow->GetCharacterCreationOptions(CreateOptions))
        {
            Result.CreateHeroOptions.Reserve(CreateOptions.Num());
            for (const FDivineBeastsCharacterCreationOption& Option : CreateOptions)
            {
                FDivineBeastsUICreateHeroItem Item;
                Item.HeroDefinitionId = Option.HeroDefinitionId;
                Item.DisplayNameKey = Option.DisplayNameKey;
                Result.CreateHeroOptions.Add(MoveTemp(Item));
            }
        }
    }

    if (FlowState.bHasSelectedCharacter)
    {
        Result.SelectedCharacterId =
            FlowState.SelectedCharacter.CharacterId;
        Result.SelectedHeroDefinitionId =
            FlowState.SelectedCharacter.HeroDefinitionId;
    }

    // 世界切换才进入项目 LoadingTravel（切服加载）路由。
    // Boot / Initialize 使用独立 Boot 页面，避免被通用 LoadingTravel 抢占。
    const bool bWorldLoading =
        FlowState.CurrentStep == FDivineBeastsFlowNodes::TransferWorld() ||
        FlowState.CurrentStep == FDivineBeastsFlowNodes::WorldReady();
    Result.Loading.bIsLoading = bWorldLoading;
    Result.Loading.ActiveTokenCount = bWorldLoading ? 1 : 0;
    Result.Loading.Stage = FlowState.LoadingSummary.IsEmpty()
        ? FText::GetEmpty()
        : FText::FromString(FlowState.LoadingSummary);
    Result.Loading.Progress = -1.0f;

    Result.World.ExperienceId = FlowState.Assignment.ExperienceId;
    Result.World.WorldId = FlowState.Assignment.WorldId;
    Result.World.RegionId = FlowState.Assignment.RegionId;

    Result.AllowedCommands.Reserve(FlowState.AllowedActions.Num());
    for (const EDivineBeastsFlowAction Action :
         FlowState.AllowedActions)
    {
        const FName CommandName = FlowActionToCommandName(Action);
        if (!CommandName.IsNone())
        {
            Result.AllowedCommands.AddUnique(CommandName);
        }
    }

    return Result;
}

void UDivineBeastsApplicationUIAdapter::CompleteCommand(
    const FDivineBeastsUICommand& Command,
    bool bAccepted,
    FName ErrorCode,
    TFunction<void(const FDivineBeastsUICommandResult&)> Completion)
{
    if (!Completion)
    {
        return;
    }

    FDivineBeastsUICommandResult Result;
    Result.RequestId = Command.RequestId;
    Result.bAccepted = bAccepted;
    Result.ErrorCode = ErrorCode;
    Completion(Result);
}

void UDivineBeastsApplicationUIAdapter::SubmitUICommand(
    const FDivineBeastsUICommand& Command,
    TFunction<void(const FDivineBeastsUICommandResult&)> Completion)
{
    if (!Flow || Command.ExpectedRevision != ViewState.Revision)
    {
        CompleteCommand(
            Command,
            false,
            TEXT("UI.Command.Stale"),
            MoveTemp(Completion));
        return;
    }

    const FName CommandName = UICommandName(Command.Type);
    if (CommandName.IsNone() ||
        !ViewState.AllowedCommands.Contains(CommandName))
    {
        CompleteCommand(
            Command,
            false,
            TEXT("UI.Command.NotAllowed"),
            MoveTemp(Completion));
        return;
    }

    bool bAccepted = true;
    switch (Command.Type)
    {
    case EDivineBeastsUICommandType::TryAutoLogin:
        Flow->TryAutoLogin();
        break;

    case EDivineBeastsUICommandType::Login:
        // Password 只在当前调用栈瞬时传入项目流程，Adapter 不保存、不日志、不投影。
        Flow->LoginWithCredentials(Command.LoginName, Command.Password);
        break;

    case EDivineBeastsUICommandType::CreateCharacter:
    {
        FDivineBeastsCharacterCreateDraft Draft;
        Draft.HeroDefinitionId = Command.HeroDefinitionId;
        Draft.CharacterName = Command.CharacterName;
        Draft.AppearanceSelection = Command.AppearanceSelection;
        bAccepted = Flow->SubmitCharacterCreateDraft(Draft);
        break;
    }

    case EDivineBeastsUICommandType::SelectPersistentCharacter:
        bAccepted = Flow->SelectPersistentCharacter(Command.CharacterId);
        break;

    case EDivineBeastsUICommandType::RequestWorld:
        bAccepted = Flow->RequestWorldAssignment(
            Command.DesiredExperienceId,
            Command.PreferredRegion);
        break;

    case EDivineBeastsUICommandType::Logout:
        Flow->LogoutAndRestart();
        break;

    default:
        // Arena / Training 等领域命令由其独立组合适配器负责。
        // 公共 DBAClient 不反向依赖 DBAArena，也不假装接受尚未接线的命令。
        bAccepted = false;
        break;
    }

    CompleteCommand(
        Command,
        bAccepted,
        bAccepted ? NAME_None : FName(TEXT("UI.Command.Rejected")),
        MoveTemp(Completion));
}

bool UDivineBeastsApplicationUIAdapter::CancelUICommand(
    const FGuid& RequestId)
{
    // 当前端口只报告“命令已接纳”，异步操作的所有权已经转移给 ApplicationFlow / Online。
    // 在没有精确领域请求句柄的情况下绝不伪造取消成功。
    return false;
}
