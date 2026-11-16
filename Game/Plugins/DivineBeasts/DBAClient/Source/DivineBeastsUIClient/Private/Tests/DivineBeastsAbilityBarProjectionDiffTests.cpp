#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ViewModels/Combat/DivineBeastsAbilityBarProjectionDiff.h"

/** 新旧快照增量比较：不构造Widget，不发起GAS或资源加载。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsAbilityBarProjectionDiffTest,
    "DivineBeasts.UI.Combat.AbilityProjectionDiff",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsAbilityBarProjectionDiffTest::RunTest(const FString&)
{
    using FDiff = FDivineBeastsAbilityBarProjectionDiff;
    TArray<FGamePlatformUISlotState> OldSlots;
    TArray<FGamePlatformUISlotState> NewSlots;
    TestFalse(TEXT("两份空槽位不广播变化"), FDiff::HasSlotsChanged(OldSlots, NewSlots));
    FGamePlatformUISlotState One;
    One.SlotId = TEXT("Input.Slot1");
    One.ContentId = TEXT("Ability.Rat.Basic");
    One.OverlayProgress = 0.25f;
    OldSlots.Add(One);
    TestTrue(TEXT("增减槽位必须发布变化"), FDiff::HasSlotsChanged(OldSlots, NewSlots));
    NewSlots.Add(One);
    TestFalse(TEXT("完全一致槽位不广播变化"), FDiff::HasSlotsChanged(OldSlots, NewSlots));
    NewSlots[0].OverlayProgress = 0.25005f;
    TestFalse(TEXT("容许细小渲染浮动"), FDiff::HasSlotsChanged(OldSlots, NewSlots));
    NewSlots[0].OverlayProgress = 0.4f;
    TestTrue(TEXT("真实冷却遮罩变化必须广播"), FDiff::HasSlotsChanged(OldSlots, NewSlots));
    NewSlots[0] = One;
    NewSlots[0].bPending = true;
    TestTrue(TEXT("复制待确认状态变化必须广播"), FDiff::HasSlotsChanged(OldSlots, NewSlots));
    NewSlots[0] = One;
    NewSlots[0].ContentId = TEXT("Ability.Rat.Other");
    TestTrue(TEXT("技能身份改变必须广播"), FDiff::HasSlotsChanged(OldSlots, NewSlots));

    TArray<FDivineBeastsAbilitySlotDetails> OldDetails;
    TArray<FDivineBeastsAbilitySlotDetails> NewDetails;
    TestFalse(TEXT("两份空详细信息一致"), FDiff::HasDetailsChanged(OldDetails, NewDetails));
    FDivineBeastsAbilitySlotDetails Detail;
    Detail.SlotId = One.SlotId;
    Detail.AbilityId = One.ContentId;
    Detail.DisplayName = FText::FromString(TEXT("利爪"));
    Detail.CooldownRemainingSeconds = 2.0f;
    Detail.CooldownTotalSeconds = 5.0f;
    Detail.CooldownEndsAtAppSeconds = 10.0f;
    OldDetails.Add(Detail);
    NewDetails.Add(Detail);
    TestFalse(TEXT("详情完全一致无需广播"), FDiff::HasDetailsChanged(OldDetails, NewDetails));
    NewDetails[0].CooldownEndsAtAppSeconds = 10.2f;
    TestTrue(TEXT("新GAS冷却截止时间必须广播"), FDiff::HasDetailsChanged(OldDetails, NewDetails));
    NewDetails[0] = Detail;
    NewDetails[0].DisabledReason = FText::FromString(TEXT("眩晕中无法使用技能"));
    TestTrue(TEXT("封禁文案变化必须广播"), FDiff::HasDetailsChanged(OldDetails, NewDetails));
    NewDetails[0] = Detail;
    NewDetails[0].CooldownRemainingSeconds = 2.005f;
    TestFalse(TEXT("小于既有时间阈值时不重复广播"), FDiff::HasDetailsChanged(OldDetails, NewDetails));
    return true;
}

#endif
