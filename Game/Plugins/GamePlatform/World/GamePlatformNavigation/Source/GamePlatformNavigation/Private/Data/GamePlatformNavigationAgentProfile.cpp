#include "Data/GamePlatformNavigationAgentProfile.h"

bool UGamePlatformNavigationAgentProfile::ValidateProfile(FText& OutReason) const
{
    if (ProfileId.IsNone() ||
        !FMath::IsFinite(AgentRadius) || AgentRadius <= 0.0f ||
        !FMath::IsFinite(AgentHeight) || AgentHeight <= AgentRadius ||
        !FMath::IsFinite(StepHeight) || StepHeight < 0.0f ||
        !FMath::IsFinite(MaxSlope) || MaxSlope < 0.0f || MaxSlope > 90.0f ||
        Version <= 0)
    {
        OutReason = FText::FromString(TEXT("Navigation Agent Profile配置非法"));
        return false;
    }

    if (InvokerPolicy == EGamePlatformNavigationInvokerPolicy::RegisterWhenActive &&
        (!FMath::IsFinite(TileGenerationRadius) ||
         !FMath::IsFinite(TileRemovalRadius) ||
         TileGenerationRadius <= 0.0f ||
         TileRemovalRadius < TileGenerationRadius))
    {
        OutReason = FText::FromString(TEXT("Navigation Invoker配置非法"));
        return false;
    }

    OutReason = FText::GetEmpty();
    return true;
}
