// 竞技场把本人退出命令接入公共 ESC 菜单。场内弃权和赛后取票分别由既有权威入口确认。
#include "Client/DivineBeastsArenaUIClientSubsystem.h"

#include "Client/DivineBeastsArenaClientSubsystem.h"
#include "DivineBeastsUIClientSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/GamePlatformArenaGameState.h"
#include "Framework/GamePlatformArenaPlayerController.h"
#include "Framework/GamePlatformArenaPlayerState.h"

namespace
{
bool CanRequestForfeit(const AGamePlatformArenaGameState& GameState,
    const AGamePlatformArenaPlayerController& Controller)
{
    const auto* Player = Controller.GetPlayerState<AGamePlatformArenaPlayerState>();
    return GameState.MatchPhase == EGamePlatformArenaMatchPhase::InProgress &&
        Player && Player->ConnectionState == EGamePlatformArenaConnectionState::Connected &&
        Player->ForfeitState != EGamePlatformArenaForfeitState::Accepted;
}
}

AGamePlatformArenaPlayerController* UDivineBeastsArenaUIClientSubsystem::GetArenaExitController() const
{
    const auto* Player = GetLocalPlayer();
    const auto* GameState = BoundGameState.Get();
    UWorld* World = BoundWorld.Get();
    return !bDeinitializing && Player && World && World == GetWorld() && !World->bIsTearingDown && GameState &&
        !GameState->MatchIdPublic.IsEmpty() && !GetAuthenticatedMatchmakingPlayerId().IsEmpty()
        ? Cast<AGamePlatformArenaPlayerController>(Player->GetPlayerController(World)) : nullptr;
}

void UDivineBeastsArenaUIClientSubsystem::ClearArenaExitAction()
{
    const auto Controller = ArenaExitController;
    ArenaExitController.Reset();
    bReturnAfterForfeit = false;
    bArenaForfeitRejected = false;
    if (auto* Owner = Controller.Get())
    {
        Owner->OnArenaForfeitRejected.RemoveAll(this);
        if (auto* Player = GetLocalPlayer())
            if (auto* UI = Player->GetSubsystem<UDivineBeastsUIClientSubsystem>()) UI->ClearWorldMenuAction(Owner);
    }
}

void UDivineBeastsArenaUIClientSubsystem::RefreshArenaExitAction()
{
    auto* Controller = GetArenaExitController();
    if (!Controller)
    {
        ClearArenaExitAction();
        return;
    }
    if (ArenaExitController.Get() != Controller)
    {
        ClearArenaExitAction();
        if (GetArenaExitController() != Controller) return;
        ArenaExitController = Controller;
        Controller->OnArenaForfeitRejected.AddUObject(this, &ThisClass::HandleArenaMenuExitRejected);
    }
    auto* UI = GetLocalPlayer()->GetSubsystem<UDivineBeastsUIClientSubsystem>();
    auto* Client = MatchmakingSource.Get();
    if (!UI || !Client) return;

    const bool bCanReturn = Client->CanReturnObservedMatch();
    if (bReturnAfterForfeit && bCanReturn)
    {
        bReturnAfterForfeit = false;
        if (Client->RequestPostMatchReturnToWorld()) return;
        if (GetArenaExitController() != Controller) return;
    }

    const auto& GameState = *BoundGameState.Get();
    const bool bCanForfeit = Client->IsApplicationFlowInWorld() && !bReturnAfterForfeit &&
        CanRequestForfeit(GameState, *Controller);
    const bool bCanRetryReturn = GameState.MatchPhase == EGamePlatformArenaMatchPhase::Completed &&
        Client->CanRetryPostMatchReturn();
    FDivineBeastsWorldMenuAction Action;
    Action.Label = INVTEXT("退出竞技场");
    Action.bEnabled = bCanReturn || bCanForfeit || bCanRetryReturn;
    Action.Execute = FSimpleDelegate::CreateUObject(this, &ThisClass::RequestArenaMenuExit);
    if (bArenaForfeitRejected) Action.Status = INVTEXT("退出请求未受理，请重试");
    else if (bCanRetryReturn) Action.Status = INVTEXT("点击重新同步返回信息");
    else if (bReturnAfterForfeit) Action.Status = INVTEXT("已提交弃权，结算后返回");
    else if (bCanForfeit) Action.Status = INVTEXT("比赛中退出将记为个人弃权");
    else if (!bCanReturn) Action.Status = INVTEXT("等待服务器确认");
    UI->SetWorldMenuAction(*Controller, MoveTemp(Action));
}

void UDivineBeastsArenaUIClientSubsystem::RequestArenaMenuExit()
{
    auto* Controller = GetArenaExitController();
    auto* Client = MatchmakingSource.Get();
    if (!Controller || !Client || !Client->IsApplicationFlowInWorld() || ArenaExitController.Get() != Controller) return;
    if (Client->CanReturnObservedMatch())
    {
        bArenaForfeitRejected = false;
        Client->RequestPostMatchReturnToWorld();
        return;
    }
    if (BoundGameState->MatchPhase == EGamePlatformArenaMatchPhase::Completed && Client->CanRetryPostMatchReturn())
    {
        Client->RetryObservedMigration();
        return;
    }
    if (bReturnAfterForfeit || !CanRequestForfeit(*BoundGameState.Get(), *Controller)) return;
    bReturnAfterForfeit = true;
    bArenaForfeitRejected = false;
    Controller->ServerRequestForfeit();
    RefreshArenaExitAction();
}

void UDivineBeastsArenaUIClientSubsystem::HandleArenaMenuExitRejected()
{
    if (!bReturnAfterForfeit || ArenaExitController.Get() != GetArenaExitController()) return;
    bReturnAfterForfeit = false;
    bArenaForfeitRejected = true;
    RefreshArenaExitAction();
}
