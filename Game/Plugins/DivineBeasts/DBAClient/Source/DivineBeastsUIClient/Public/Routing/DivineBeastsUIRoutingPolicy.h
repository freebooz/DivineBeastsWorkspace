// DivineBeasts项目客户端纯页面策略：供本地玩家UI协调器和页面读取，不持有角色或后端权威状态。
// 调用方按状态事件投影页面；本策略无异步资源、无持久化状态，不推进应用流程节点。
#pragma once

#include "CoreMinimal.h"
#include "Contracts/DivineBeastsUIContracts.h"

/** FDivineBeastsUIRoutingPolicy（神兽联盟项目UI主页面路由策略）。 */
class DIVINEBEASTSUICLIENT_API FDivineBeastsUIRoutingPolicy
{
public:
    /** 系统菜单仅已认证且空闲的InWorld可打开；不推导服务器角色或暂停世界。 */
    static bool CanOpenSystemMenu(const FDivineBeastsUIViewState& State);
    /**
     * 读取State只读流程快照与CharacterEntryScreenPreference本地页面偏好，返回应显示的页面ID。
     * CharacterEntry及LoadRoster默认显示选择页，空角色不会隐式打开创建页；未知节点返回NAME_None。
     * 偏好只在已认证角色入口保持，包括忙碌快照；错误、加载和权威节点优先，不授予业务命令资格。
     */
    static FName ResolvePrimaryScreen(
        const FDivineBeastsUIViewState& State,
        FName CharacterEntryScreenPreference = NAME_None);
    /**
     * State须为已认证、空闲、无加载或错误的CharacterEntry快照；ScreenId只能为创建或选择页。
     * 创建页仍须允许CreateCharacter；返回选择页属于本地取消，允许空角色且不要求选择提交命令。
     * 返回false时调用方保持原页面；返回true不表示角色选择或世界准入已获授权。
     */
    static bool CanNavigateCharacterEntry(const FDivineBeastsUIViewState& State, FName ScreenId);
};
