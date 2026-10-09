#include "Components/GamePlatformPartyRosterWidget.h"

bool UGamePlatformPartyRosterWidget::BindPartySource(FGuid InSourceScopeId)
{
    if (!InSourceScopeId.IsValid())
    {
        return false;
    }
    if (CurrentScopeId == InSourceScopeId)
    {
        return true;
    }
    CurrentScopeId = InSourceScopeId;
    State = FGamePlatformUIPartyRosterState();
    BP_OnPartyRosterChanged(State);
    return true;
}

void UGamePlatformPartyRosterWidget::ClearPartySource()
{
    if (!CurrentScopeId.IsValid())
    {
        return;
    }
    CurrentScopeId.Invalidate();
    State = FGamePlatformUIPartyRosterState();
    BP_OnPartyRosterChanged(State);
}

bool UGamePlatformPartyRosterWidget::ApplyPartyRoster(
    const FGamePlatformUIPartyRosterState& InState)
{
    constexpr int32 MaxPartyRows = 32;
    if (!CurrentScopeId.IsValid() ||
        InState.SourceScopeId != CurrentScopeId ||
        InState.Revision < 0 ||
        InState.Members.Num() > MaxPartyRows ||
        (State.SourceScopeId == InState.SourceScopeId &&
         InState.Revision <= State.Revision))
    {
        return false;
    }

    TSet<FName> SeenIds;
    SeenIds.Reserve(InState.Members.Num());
    for (const FGamePlatformUIPartyMember& Member : InState.Members)
    {
        if (Member.MemberId.IsNone() ||
            SeenIds.Contains(Member.MemberId) ||
            !FMath::IsFinite(Member.HealthRatio) ||
            !FMath::IsFinite(Member.ShieldRatio))
        {
            return false;
        }
        SeenIds.Add(Member.MemberId);
    }

    State = InState;
    for (FGamePlatformUIPartyMember& Member : State.Members)
    {
        Member.HealthRatio = FMath::Clamp(Member.HealthRatio, 0.0f, 1.0f);
        Member.ShieldRatio = FMath::Clamp(Member.ShieldRatio, 0.0f, 1.0f);
    }
    BP_OnPartyRosterChanged(State);
    return true;
}
