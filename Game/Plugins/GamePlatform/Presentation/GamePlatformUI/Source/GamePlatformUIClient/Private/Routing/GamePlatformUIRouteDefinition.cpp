#include "Routing/GamePlatformUIRouteDefinition.h"

#define LOCTEXT_NAMESPACE "GamePlatformUIRouteDefinition"

namespace
{
    bool IsActivatableRouteLayer(EGamePlatformUILayer Layer)
    {
        switch (Layer)
        {
        case EGamePlatformUILayer::Screen:
        case EGamePlatformUILayer::Modal:
        case EGamePlatformUILayer::System:
        case EGamePlatformUILayer::Loading:
        case EGamePlatformUILayer::Debug:
            return true;
        default:
            return false;
        }
    }
}

bool UGamePlatformUIRouteDefinition::ValidateDefinition(
    FText& OutReason) const
{
    if (RouteId.IsNone() || ScreenId.IsNone())
    {
        OutReason = LOCTEXT(
            "MissingRouteIdentity",
            "RouteId和ScreenId不能为空。");
        return false;
    }

    if (!IsActivatableRouteLayer(Layer))
    {
        OutReason = LOCTEXT(
            "InvalidRouteLayer",
            "Route只能指向Screen/Modal/System/Loading/Debug可激活层。");
        return false;
    }

#if UE_BUILD_SHIPPING
    if (Layer == EGamePlatformUILayer::Debug)
    {
        OutReason = LOCTEXT(
            "DebugRouteShipping",
            "Shipping构建禁止注册Debug Route。");
        return false;
    }
#endif

    if (FallbackRouteId == RouteId || BackRouteId == RouteId)
    {
        OutReason = LOCTEXT(
            "SelfRoute",
            "FallbackRouteId或BackRouteId不能指向自身。");
        return false;
    }

    if (RequiredTags.HasAny(BlockedTags))
    {
        OutReason = LOCTEXT(
            "ConflictingRouteTags",
            "Route RequiredTags与BlockedTags不能包含同一标签。");
        return false;
    }

    OutReason = FText::GetEmpty();
    return true;
}

#undef LOCTEXT_NAMESPACE
