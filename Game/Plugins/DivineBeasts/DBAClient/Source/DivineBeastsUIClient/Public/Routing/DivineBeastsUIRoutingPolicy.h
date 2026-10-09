#pragma once

#include "CoreMinimal.h"
#include "Contracts/DivineBeastsUIContracts.h"

/** FDivineBeastsUIRoutingPolicy（神兽联盟项目UI主页面路由策略）。 */
class DIVINEBEASTSUICLIENT_API FDivineBeastsUIRoutingPolicy
{
public:
    /** 只投影当前流程与本地角色入口页面偏好；偏好不能越过认证、加载、错误或权威选择节点。 */
    static FName ResolvePrimaryScreen(
        const FDivineBeastsUIViewState& State,
        FName CharacterEntryScreenPreference = NAME_None);
    /** 仅认证后的空闲CharacterEntry允许切换创建/选择页；无角色时不能返回选择页。 */
    static bool CanNavigateCharacterEntry(const FDivineBeastsUIViewState& State, FName ScreenId);
};
