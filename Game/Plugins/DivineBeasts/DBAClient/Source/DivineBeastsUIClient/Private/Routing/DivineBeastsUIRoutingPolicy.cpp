// 项目客户端纯路由策略：消费只读流程与本地页面偏好，不生成角色、不登录、不推进权威节点。
#include "Routing/DivineBeastsUIRoutingPolicy.h"

bool FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(const FDivineBeastsUIViewState& State, FName ScreenId)
{
    if (!State.bAuthenticated || State.bBusy || State.Loading.bIsLoading ||
        State.PageState == EDivineBeastsUIPageState::Error || State.CurrentStep != TEXT("DBA.Flow.CharacterEntry"))
    {
        return false;
    }
    if (ScreenId == TEXT("UI.Screen.CharacterCreate")) { return State.AllowedCommands.Contains(TEXT("CreateCharacter")); }
    if (ScreenId == TEXT("UI.Screen.CharacterSelect")) { return !State.Characters.IsEmpty() && State.AllowedCommands.Contains(TEXT("SelectPersistentCharacter")); }
    return false;
}

FName FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(
    const FDivineBeastsUIViewState& State,
    FName CharacterEntryScreenPreference)
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
        // 偏好只在已认证的角色入口中保持，包括提交命令前短暂的忙碌快照；不改写业务角色列表。
        if (State.CurrentStep == TEXT("DBA.Flow.CharacterEntry") && State.bAuthenticated)
        {
            if (CharacterEntryScreenPreference == TEXT("UI.Screen.CharacterCreate")) { return CharacterEntryScreenPreference; }
            if (CharacterEntryScreenPreference == TEXT("UI.Screen.CharacterSelect") && !State.Characters.IsEmpty()) { return CharacterEntryScreenPreference; }
        }
        return State.Characters.IsEmpty()
            ? FName(TEXT("UI.Screen.CharacterCreate"))
            : FName(TEXT("UI.Screen.CharacterSelect"));
    }

    return NAME_None;
}
