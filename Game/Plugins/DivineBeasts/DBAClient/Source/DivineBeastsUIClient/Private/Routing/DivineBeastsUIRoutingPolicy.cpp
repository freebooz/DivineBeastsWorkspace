#include "Routing/DivineBeastsUIRoutingPolicy.h"

FName FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(
    const FDivineBeastsUIViewState& State)
{
    if (State.Loading.bIsLoading)
    {
        return TEXT("UI.Screen.LoadingTravel");
    }
    if (State.PageState == EDivineBeastsUIPageState::Error)
    {
        return TEXT("UI.Screen.ErrorReconnect");
    }

    if (State.Arena.ResultCommitState ==
        EDivineBeastsUIResultCommitState::Committed)
    {
        return TEXT("UI.Screen.PostMatchResult");
    }
    if (State.Arena.MatchPhaseId == TEXT("HeroSelection"))
    {
        return TEXT("UI.Screen.ArenaHeroSelection");
    }
    if (State.Arena.MatchPhaseId == TEXT("ReadyCheck") ||
        State.Arena.MatchPhaseId == TEXT("Countdown"))
    {
        return TEXT("UI.Screen.MatchFoundReady");
    }

    const FString Step = State.CurrentStep.ToString();
    if (Step.IsEmpty() ||
        Step.Contains(TEXT("Boot")) ||
        Step.Contains(TEXT("Initialize")))
    {
        return TEXT("UI.Screen.Boot");
    }
    if (Step.Contains(TEXT("Authentication")))
    {
        return TEXT("UI.Screen.Login");
    }
    if (Step.Contains(TEXT("CreateCharacter")))
    {
        return TEXT("UI.Screen.CharacterCreate");
    }
    if (Step.Contains(TEXT("ValidateSelection")))
    {
        return TEXT("UI.Screen.CharacterSelect");
    }
    if (Step.Contains(TEXT("CharacterEntry")) ||
        Step.Contains(TEXT("LoadRoster")))
    {
        return State.Characters.IsEmpty()
            ? FName(TEXT("UI.Screen.CharacterCreate"))
            : FName(TEXT("UI.Screen.CharacterRoster"));
    }

    return NAME_None;
}
