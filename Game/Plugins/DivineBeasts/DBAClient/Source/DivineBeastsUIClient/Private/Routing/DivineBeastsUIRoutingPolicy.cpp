// 项目客户端纯路由策略：消费只读流程与本地页面偏好，不生成角色、不登录、不推进权威节点。
#include "Routing/DivineBeastsUIRoutingPolicy.h"

bool FDivineBeastsUIRoutingPolicy::CanOpenSystemMenu(const FDivineBeastsUIViewState& State)
{
    // 已认证并进入世界是唯一入口；传输、错误、注销及前端均不得抢占业务页面焦点。
    return State.bAuthenticated && !State.bBusy && !State.Loading.bIsLoading &&
        State.PageState != EDivineBeastsUIPageState::Error && State.CurrentStep == TEXT("DBA.Flow.InWorld");
}

bool FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(const FDivineBeastsUIViewState& State, FName ScreenId)
{
    if (!State.bAuthenticated || State.bBusy || State.Loading.bIsLoading ||
        State.PageState == EDivineBeastsUIPageState::Error || State.CurrentStep != TEXT("DBA.Flow.CharacterEntry"))
    {
        return false;
    }
    if (ScreenId == TEXT("UI.Screen.CharacterCreate")) { return State.AllowedCommands.Contains(TEXT("CreateCharacter")); }
<<<<<<< HEAD
    // 返回选择页只取消本地创建草稿；空列表或缺少选择提交权限不能把用户困在创建表单。
    // 真实选择与世界准入仍由ViewModel、应用流程和后端核对角色及允许命令。
=======
    // 返回选择页只是本地显示/取消草稿，不是提交角色选择；即使列表为空且无提交权限也必须允许。
>>>>>>> 0963476cd210192af4c0e11459fa707dd95bff99
    if (ScreenId == TEXT("UI.Screen.CharacterSelect")) { return true; }
    return false;
}

FName FDivineBeastsUIRoutingPolicy::ResolvePrimaryScreen(
    const FDivineBeastsUIViewState& State,
    FName CharacterEntryScreenPreference)
{
    // 传输终态失败必须打断残留加载标志，否则连接关闭后界面会继续遮挡错误和返回入口。
    if (State.PageState == EDivineBeastsUIPageState::Error)
    {
        return TEXT("UI.Screen.ErrorReconnect");
    }
    if (State.Loading.bIsLoading) { return TEXT("UI.Screen.LoadingTravel"); }

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
            if (CharacterEntryScreenPreference == TEXT("UI.Screen.CharacterSelect")) { return CharacterEntryScreenPreference; }
        }
<<<<<<< HEAD
        // 登录后的角色入口与档案刷新均先展示选择页；空列表也须显式点击创建，不隐式提交或换页。
=======
        // 无角色不代表已经获得创建授权；默认先显示空角色选择页，创建由用户显式点击。
        // 这条显示路由不调用后端、不伪造SelectPersistentCharacter业务命令。
>>>>>>> 0963476cd210192af4c0e11459fa707dd95bff99
        return TEXT("UI.Screen.CharacterSelect");
    }

    return NAME_None;
}
