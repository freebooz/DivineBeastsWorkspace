#pragma once

#include "CoreMinimal.h"
#include "Contracts/DivineBeastsUIContracts.h"

/** FDivineBeastsUIRoutingPolicy（神兽联盟项目UI主页面路由策略）。 */
class DIVINEBEASTSUICLIENT_API FDivineBeastsUIRoutingPolicy
{
public:
    /** 只根据只读View State（视图状态）给出建议页面，不推进Gameplay/Application流程。 */
    static FName ResolvePrimaryScreen(
        const FDivineBeastsUIViewState& State);
};
