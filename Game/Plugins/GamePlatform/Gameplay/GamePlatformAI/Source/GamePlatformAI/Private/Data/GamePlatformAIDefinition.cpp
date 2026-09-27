#include "Data/GamePlatformAIDefinition.h"

#include "Settings/GamePlatformAISettings.h"

bool UGamePlatformAIDefinition::ValidateDefinition(FText& OutReason) const
{
    if (AIDefinitionId.IsNone())
    {
        OutReason = FText::FromString(TEXT("AIDefinitionId不能为空"));
        return false;
    }

    const UGamePlatformAISettings* Settings =
        GetDefault<UGamePlatformAISettings>();

    if (!FMath::IsFinite(PerceptionProfile.SightRadius) ||
        !FMath::IsFinite(PerceptionProfile.LoseSightRadius) ||
        PerceptionProfile.SightRadius <= 0.0f ||
        PerceptionProfile.LoseSightRadius < PerceptionProfile.SightRadius ||
        PerceptionProfile.SightRadius > Settings->MaxSightRadius ||
        PerceptionProfile.LoseSightRadius > Settings->MaxLoseSightRadius)
    {
        OutReason = FText::FromString(TEXT("Sight配置非法"));
        return false;
    }

    if (!FMath::IsFinite(PerceptionProfile.MaxAge) ||
        PerceptionProfile.MaxAge < 0.0f ||
        PerceptionProfile.MaxAge > Settings->MaxPerceptionAge)
    {
        OutReason = FText::FromString(TEXT("Perception MaxAge非法"));
        return false;
    }

    if (!FMath::IsFinite(
            PerceptionProfile.PeripheralVisionHalfAngleDegrees) ||
        PerceptionProfile.PeripheralVisionHalfAngleDegrees < 0.0f ||
        PerceptionProfile.PeripheralVisionHalfAngleDegrees > 180.0f)
    {
        OutReason = FText::FromString(TEXT("PeripheralVision配置非法"));
        return false;
    }

    if (PerceptionProfile.bEnableHearing &&
        (!FMath::IsFinite(PerceptionProfile.HearingRange) ||
         PerceptionProfile.HearingRange <= 0.0f))
    {
        OutReason = FText::FromString(TEXT("HearingRange配置非法"));
        return false;
    }

    if (TargetSelectionProfile.MaxCandidates <= 0 ||
        TargetSelectionProfile.MaxCandidates > Settings->MaxTargetCandidates ||
        !FMath::IsFinite(TargetSelectionProfile.MemorySeconds) ||
        TargetSelectionProfile.MemorySeconds <= 0.0f)
    {
        OutReason = FText::FromString(TEXT("TargetSelection配置非法"));
        return false;
    }

    if (!FMath::IsFinite(UpdateProfile.DecisionInterval) ||
        UpdateProfile.DecisionInterval < Settings->MinDecisionInterval ||
        !FMath::IsFinite(UpdateProfile.MoveRefreshInterval) ||
        UpdateProfile.MoveRefreshInterval < Settings->MinDecisionInterval ||
        !FMath::IsFinite(UpdateProfile.MoveRefreshDistance) ||
        UpdateProfile.MoveRefreshDistance < 0.0f ||
        !FMath::IsFinite(UpdateProfile.AttackRetryBackoff) ||
        UpdateProfile.AttackRetryBackoff < Settings->MinDecisionInterval ||
        !FMath::IsFinite(HomePolicy.PatrolRadius) ||
        HomePolicy.PatrolRadius < 0.0f ||
        !FMath::IsFinite(HomePolicy.LeashRadius) ||
        HomePolicy.LeashRadius < 0.0f ||
        HomePolicy.LeashRadius > Settings->MaxLeashRadius ||
        !FMath::IsFinite(HomePolicy.ReturnHomeAcceptanceRadius) ||
        HomePolicy.ReturnHomeAcceptanceRadius < 0.0f ||
        !FMath::IsFinite(HomePolicy.PatrolAcceptanceRadius) ||
        HomePolicy.PatrolAcceptanceRadius < 0.0f)
    {
        OutReason = FText::FromString(TEXT("Update/Home配置非法"));
        return false;
    }

    if (!FMath::IsFinite(AttackRange) ||
        AttackRange <= 0.0f ||
        !FMath::IsFinite(PreferredRange) ||
        PreferredRange < 0.0f ||
        PreferredRange > AttackRange)
    {
        OutReason = FText::FromString(TEXT("Combat Range配置非法"));
        return false;
    }

    if (BrainType == EGamePlatformAIBrainType::BehaviorTree &&
        (!BehaviorTreeAsset.IsValid() || !BlackboardAsset.IsValid()))
    {
        OutReason = FText::FromString(TEXT("BehaviorTree模式必须提供BehaviorTree和Blackboard"));
        return false;
    }

    if (BrainType == EGamePlatformAIBrainType::StateTree &&
        !StateTreeAsset.IsValid())
    {
        OutReason = FText::FromString(TEXT("StateTree模式必须提供StateTree资源"));
        return false;
    }

    OutReason = FText::GetEmpty();
    return true;
}

void UGamePlatformAIDefinition::GetReferencedAssetPaths(
    TArray<FSoftObjectPath>& OutPaths) const
{
    OutPaths.Reset();

    if (BehaviorTreeAsset.IsValid())
    {
        OutPaths.AddUnique(BehaviorTreeAsset);
    }

    if (BlackboardAsset.IsValid())
    {
        OutPaths.AddUnique(BlackboardAsset);
    }

    if (StateTreeAsset.IsValid())
    {
        OutPaths.AddUnique(StateTreeAsset);
    }
}
