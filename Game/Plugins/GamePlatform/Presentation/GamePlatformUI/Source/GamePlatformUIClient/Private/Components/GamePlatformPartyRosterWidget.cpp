#include "Components/GamePlatformPartyRosterWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

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
    SetVisibility(ESlateVisibility::Collapsed);
    if (UVerticalBox* Rows = Cast<UVerticalBox>(GetWidgetFromName(TEXT("PartyMembers"))))
    {
        Rows->ClearChildren();
    }
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
    SetVisibility(ESlateVisibility::Collapsed);
    if (UVerticalBox* Rows = Cast<UVerticalBox>(GetWidgetFromName(TEXT("PartyMembers"))))
    {
        Rows->ClearChildren();
    }
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
        // ShieldRatio属于历史快照合同，不再在当前项目HUD显示其数值。
        Member.ShieldRatio = FMath::Clamp(Member.ShieldRatio, 0.0f, 1.0f);
    }
    // 仅用上游批准的队伍快照绘制名称和生命比例；不显示已取消的护盾数值。
    // 单次最多32成员，且只在快照更新时重建子控件，不在Tick轮询。
    if (UVerticalBox* Rows = Cast<UVerticalBox>(GetWidgetFromName(TEXT("PartyMembers"))))
    {
        Rows->ClearChildren();
        for (const FGamePlatformUIPartyMember& Member : State.Members)
        {
            UTextBlock* Text = NewObject<UTextBlock>(this);
            const FText SafeName = Member.Portrait.DisplayName.IsEmpty()
                ? FText::FromString(TEXT("队伍成员"))
                : Member.Portrait.DisplayName;
            const int32 HealthPercent = FMath::RoundToInt(Member.HealthRatio * 100.0f);
            Text->SetText(FText::Format(
                NSLOCTEXT("GamePlatformUI", "PartyMemberHealthRow", "{0}  生命{1}%"),
                SafeName, FText::AsNumber(HealthPercent)));
            Rows->AddChildToVerticalBox(Text);
        }
    }

    SetVisibility(State.Members.IsEmpty()
        ? ESlateVisibility::Collapsed
        : ESlateVisibility::SelfHitTestInvisible);
    BP_OnPartyRosterChanged(State);
    return true;
}
