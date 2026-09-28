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

    const FString Step = State.CurrentStep.ToString();
    // NAME_None 转为字符串后得到 "None"，并不是空字符串；必须先按 FName
    // 语义判断“尚无流程步骤”，否则首次事件快照会错误地落到无页面状态。
    if (State.CurrentStep.IsNone() ||
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
