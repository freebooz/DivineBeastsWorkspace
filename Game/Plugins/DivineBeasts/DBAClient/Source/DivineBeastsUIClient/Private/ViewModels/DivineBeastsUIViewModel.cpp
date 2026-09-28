#include "ViewModels/DivineBeastsUIViewModel.h"

#include "DivineBeastsUIClientSubsystem.h"

void UDivineBeastsUIViewModel::InitializeForScreen(
    UDivineBeastsUIClientSubsystem* InOwner,
    FName InScreenId)
{
    Owner = InOwner;
    ScreenId = InScreenId;
    State = Owner ? Owner->GetViewState() : FDivineBeastsUIViewState();
}

void UDivineBeastsUIViewModel::OnScreenActivated()
{
    if (!Owner || StateHandle.IsValid())
    {
        return;
    }

    State = Owner->GetViewState();
    StateHandle = Owner->OnStateChanged().AddUObject(
        this,
        &UDivineBeastsUIViewModel::HandleStateChanged);
    MarkStateChanged();
}

void UDivineBeastsUIViewModel::OnScreenDeactivated()
{
    if (Owner && StateHandle.IsValid())
    {
        Owner->OnStateChanged().Remove(StateHandle);
        StateHandle.Reset();
    }

    if (Owner)
    {
        const TArray<FGuid> Requests = PendingCommands.Array();
        for (const FGuid& RequestId : Requests)
        {
            Owner->CancelCommand(RequestId);
        }
    }
    PendingCommands.Reset();
}

FGuid UDivineBeastsUIViewModel::TryAutoLogin()
{
    FDivineBeastsUICommand Command;
    Command.Type = EDivineBeastsUICommandType::TryAutoLogin;
    return Submit(MoveTemp(Command));
}

FGuid UDivineBeastsUIViewModel::Login(
    const FString& LoginName,
    const FString& Password)
{
    FDivineBeastsUICommand Command;
    Command.Type = EDivineBeastsUICommandType::Login;
    Command.LoginName = LoginName;
    Command.Password = Password;
    return Submit(MoveTemp(Command));
}

FGuid UDivineBeastsUIViewModel::Refresh()
{
    FDivineBeastsUICommand Command;
    Command.Type = EDivineBeastsUICommandType::Refresh;
    return Submit(MoveTemp(Command));
}

FGuid UDivineBeastsUIViewModel::Retry()
{
    FDivineBeastsUICommand Command;
    Command.Type = EDivineBeastsUICommandType::Retry;
    return Submit(MoveTemp(Command));
}

FGuid UDivineBeastsUIViewModel::CreateCharacter(
    FName HeroDefinitionId,
    const FString& CharacterName,
    const TMap<FString, FString>& AppearanceSelection)
{
    FDivineBeastsUICommand Command;
    Command.Type = EDivineBeastsUICommandType::CreateCharacter;
    Command.HeroDefinitionId = HeroDefinitionId;
    Command.CharacterName = CharacterName;
    Command.AppearanceSelection = AppearanceSelection;
    return Submit(MoveTemp(Command));
}

FGuid UDivineBeastsUIViewModel::SelectPersistentCharacter(
    const FString& CharacterId)
{
    FDivineBeastsUICommand Command;
    Command.Type = EDivineBeastsUICommandType::SelectPersistentCharacter;
    Command.CharacterId = CharacterId;
    return Submit(MoveTemp(Command));
}

FGuid UDivineBeastsUIViewModel::RequestWorld(
    FName DesiredExperienceId,
    const FString& PreferredRegion)
{
    FDivineBeastsUICommand Command;
    Command.Type = EDivineBeastsUICommandType::RequestWorld;
    Command.DesiredExperienceId = DesiredExperienceId;
    Command.PreferredRegion = PreferredRegion;
    return Submit(MoveTemp(Command));
}

FGuid UDivineBeastsUIViewModel::Logout()
{
    FDivineBeastsUICommand Command;
    Command.Type = EDivineBeastsUICommandType::Logout;
    return Submit(MoveTemp(Command));
}

FGuid UDivineBeastsUIViewModel::RequestTrainingReset()
{
    FDivineBeastsUICommand Command;
    Command.Type = EDivineBeastsUICommandType::TrainingReset;
    return Submit(MoveTemp(Command));
}

FGuid UDivineBeastsUIViewModel::StartMatchmaking(
    FName ArenaModeId,
    const FString& PartyId,
    const FString& PreferredRegion)
{
    FDivineBeastsUICommand Command;
    Command.Type = EDivineBeastsUICommandType::StartMatchmaking;
    Command.ArenaModeId = ArenaModeId;
    Command.PartyId = PartyId;
    Command.PreferredRegion = PreferredRegion;
    return Submit(MoveTemp(Command));
}

FGuid UDivineBeastsUIViewModel::CancelMatchmaking()
{
    FDivineBeastsUICommand Command;
    Command.Type = EDivineBeastsUICommandType::CancelMatchmaking;
    return Submit(MoveTemp(Command));
}

FGuid UDivineBeastsUIViewModel::SelectArenaHero(FName HeroDefinitionId)
{
    FDivineBeastsUICommand Command;
    Command.Type = EDivineBeastsUICommandType::ArenaSelectHero;
    Command.HeroDefinitionId = HeroDefinitionId;
    return Submit(MoveTemp(Command));
}

FGuid UDivineBeastsUIViewModel::SetArenaReady()
{
    FDivineBeastsUICommand Command;
    Command.Type = EDivineBeastsUICommandType::ArenaReady;
    return Submit(MoveTemp(Command));
}

FGuid UDivineBeastsUIViewModel::ReturnToWorldAfterMatch()
{
    FDivineBeastsUICommand Command;
    Command.Type = EDivineBeastsUICommandType::PostMatchReturnToWorld;
    return Submit(MoveTemp(Command));
}

bool UDivineBeastsUIViewModel::CancelCommand(FGuid RequestId)
{
    if (!Owner || !PendingCommands.Contains(RequestId))
    {
        return false;
    }

    const bool bCancelled = Owner->CancelCommand(RequestId);
    if (bCancelled)
    {
        PendingCommands.Remove(RequestId);
    }
    return bCancelled;
}

FGuid UDivineBeastsUIViewModel::Submit(FDivineBeastsUICommand Command)
{
    if (!Owner || !IsPageActive())
    {
        return FGuid();
    }

    Command.RequestId = FGuid::NewGuid();
    Command.PageGeneration = GetPageGeneration();
    Command.ExpectedRevision = State.Revision;

    FString Error;
    if (!Command.IsValid(Error))
    {
        LastCommandErrorCode = TEXT("UI.Command.Invalid");
        MarkStateChanged();
        return FGuid();
    }

    const FGuid RequestId = Command.RequestId;
    const int32 ExpectedPageGeneration = GetPageGeneration();
    PendingCommands.Add(RequestId);

    const TWeakObjectPtr<UDivineBeastsUIViewModel> WeakThis(this);
    Owner->SubmitCommand(
        MoveTemp(Command),
        [WeakThis, ExpectedPageGeneration](
            const FDivineBeastsUICommandResult& Result)
        {
            if (WeakThis.IsValid())
            {
                WeakThis->HandleCommandResult(
                    ExpectedPageGeneration,
                    Result);
            }
        });

    return RequestId;
}

void UDivineBeastsUIViewModel::HandleStateChanged(
    const FDivineBeastsUIViewState& NewState)
{
    State = NewState;
    LastCommandErrorCode = NAME_None;
    MarkStateChanged();
}

void UDivineBeastsUIViewModel::HandleCommandResult(
    int32 ExpectedPageGeneration,
    const FDivineBeastsUICommandResult& Result)
{
    const bool bWasPending = PendingCommands.Remove(Result.RequestId) > 0;
    // 命令终态以“请求身份＋页面代次”为所有权边界。业务状态可以先于命令终态到达并
    // 增加ViewModel Revision；若继续要求Revision不变，会吞掉合法终态并让页面永久忙碌。
    if (!bWasPending ||
        !IsPageActive() ||
        GetPageGeneration() != ExpectedPageGeneration)
    {
        return;
    }

    LastCommandErrorCode =
        Result.bAccepted ? NAME_None : Result.ErrorCode;
    MarkStateChanged();
    OnCommandCompleted.Broadcast(Result.RequestId, LastCommandErrorCode);
}
