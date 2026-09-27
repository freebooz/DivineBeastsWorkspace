#include "Components/GamePlatformNavigationModifierComponent.h"

#include "Areas/GamePlatformNavigationAreas.h"

void UGamePlatformNavigationModifierComponent::SetPlatformArea(
    EGamePlatformNavigationAreaKind AreaKind)
{
    switch (AreaKind)
    {
    case EGamePlatformNavigationAreaKind::HighCost:
        SetAreaClass(UGamePlatformNavArea_HighCost::StaticClass());
        break;
    case EGamePlatformNavigationAreaKind::Blocked:
        SetAreaClass(UGamePlatformNavArea_Blocked::StaticClass());
        break;
    case EGamePlatformNavigationAreaKind::Default:
    default:
        SetAreaClass(UGamePlatformNavArea_Default::StaticClass());
        break;
    }
}
