#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Services/GamePlatformQuestClientSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformQuestClientRevisionTest,
    "GamePlatform.Quest.Client.RevisionProtection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformQuestClientRevisionTest::RunTest(const FString& Parameters)
{
    UGamePlatformQuestClientSubsystem* Client =
        NewObject<UGamePlatformQuestClientSubsystem>();

    FGamePlatformQuestSnapshot Newer;
    Newer.QuestId = TEXT("Quest.Test");
    Newer.Revision = 5;
    Newer.State = EGamePlatformQuestState::Active;

    TestTrue(TEXT("接受新快照"), Client->ApplyAuthoritativeSnapshots({Newer}));

    FGamePlatformQuestSnapshot Older = Newer;
    Older.Revision = 4;
    Older.State = EGamePlatformQuestState::Completed;

    TestFalse(TEXT("旧Revision不能覆盖"), Client->ApplyAuthoritativeSnapshots({Older}));
    TestEqual(
        TEXT("状态保持新版本"),
        Client->FindQuest(TEXT("Quest.Test"))->State,
        EGamePlatformQuestState::Active);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformQuestClientSortedCacheTest,
    "GamePlatform.Quest.Client.SortedCache",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformQuestClientSortedCacheTest::RunTest(const FString& Parameters)
{
    UGamePlatformQuestClientSubsystem* Client =
        NewObject<UGamePlatformQuestClientSubsystem>();

    FGamePlatformQuestSnapshot B;
    B.QuestId = TEXT("Quest.B");
    B.Revision = 1;
    B.State = EGamePlatformQuestState::Active;

    FGamePlatformQuestSnapshot A = B;
    A.QuestId = TEXT("Quest.A");

    TestTrue(TEXT("批量权威快照进入Cache"), Client->ApplyAuthoritativeSnapshots({B, A}));

    const TArray<FGamePlatformQuestSnapshot>& First = Client->GetSortedSnapshotsView();
    TestEqual(TEXT("排序缓存条数"), First.Num(), 2);
    TestEqual(TEXT("同状态按QuestId稳定排序"), First[0].QuestId, FName(TEXT("Quest.A")));
    const FGamePlatformQuestSnapshot* FirstData = First.GetData();

    const TArray<FGamePlatformQuestSnapshot>& Second = Client->GetSortedSnapshotsView();
    TestTrue(TEXT("未变更时复用同一排序缓存"), Second.GetData() == FirstData);

    FGamePlatformQuestSnapshot Updated = B;
    Updated.Revision = 2;
    Updated.State = EGamePlatformQuestState::Completed;
    TestTrue(TEXT("新Revision使排序缓存失效"), Client->ApplyAuthoritativeSnapshots({Updated}));

    const TArray<FGamePlatformQuestSnapshot>& Third = Client->GetSortedSnapshotsView();
    TestEqual(TEXT("重建后仍为两条"), Third.Num(), 2);
    TestEqual(TEXT("更新状态写入排序缓存"), Client->FindQuest(TEXT("Quest.B"))->Revision, int64(2));
    return true;
}
#endif
