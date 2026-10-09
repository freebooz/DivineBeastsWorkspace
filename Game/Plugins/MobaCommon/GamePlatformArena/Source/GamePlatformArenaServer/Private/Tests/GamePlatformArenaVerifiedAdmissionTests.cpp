// 准入匹配值规则回归：真实运行入口调用同一规则，测试声明不替代Controller握手/验签与联网身份验证。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Server/ArenaVerifiedAdmissionPolicy.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformArenaVerifiedAdmissionTest,
    "GamePlatform.Arena.Server.VerifiedAdmissionRosterBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformArenaVerifiedAdmissionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FGamePlatformArenaAssignment Assignment; Assignment.MatchId = TEXT("Match.Current"); Assignment.GameServerId = TEXT("Server.Current");
    FGamePlatformArenaRosterSlot Slot; Slot.PlayerId = TEXT("Player.Verified"); Slot.CharacterId = TEXT("Character.Roster");
    Slot.TeamId = TEXT("Team.A"); Slot.SlotIndex = 0; Assignment.Roster.Add(Slot);
    FGamePlatformServerVerifiedAdmission Admission;
    Admission.AdmissionId = FGuid::NewGuid(); Admission.ConnectionId = FGuid::NewGuid();
    Admission.PlayerId = Slot.PlayerId; Admission.MatchId = Assignment.MatchId; Admission.SessionId = TEXT("Session.Current");
    Admission.GameSessionId = TEXT("GameSession.Current"); Admission.AssignmentId = TEXT("Assignment.Distinct");
    Admission.ReservationId = TEXT("Reservation.Current"); Admission.ServerInstanceId = Assignment.GameServerId;
    Admission.ServerBootId = TEXT("Boot.Current"); Admission.WorldId = TEXT("World.MainArena");
    Admission.ExperienceId = Assignment.ExperienceId.ToString(); Admission.ProtocolVersion = TEXT("1");
    Admission.ConnectionGeneration = 1; Admission.SessionEpoch = 1; Admission.AuthorityUntil = FDateTime::UtcNow() + FTimespan::FromMinutes(5);
    TestNotNull(TEXT("合法已验证Player从可信Roster选择Character"), GamePlatformArenaVerifiedAdmissionPolicy::ResolveRosterSlot(Admission, Assignment));
    auto Wrong = Admission; Wrong.MatchId = TEXT("Match.Old");
    TestNull(TEXT("同玩家旧比赛准入拒绝"), GamePlatformArenaVerifiedAdmissionPolicy::ResolveRosterSlot(Wrong, Assignment));
    Wrong = Admission; Wrong.ServerInstanceId = TEXT("OtherServer");
    TestNull(TEXT("其他实例拒绝"), GamePlatformArenaVerifiedAdmissionPolicy::ResolveRosterSlot(Wrong, Assignment));
    Wrong = Admission; Wrong.ExperienceId = TEXT("Experience.OpenWorld.Main");
    TestNull(TEXT("其他体验拒绝"), GamePlatformArenaVerifiedAdmissionPolicy::ResolveRosterSlot(Wrong, Assignment));
    Wrong = Admission; Wrong.AuthorityUntil = FDateTime::UtcNow() - FTimespan::FromSeconds(1);
    TestNull(TEXT("过期权威资格拒绝"), GamePlatformArenaVerifiedAdmissionPolicy::ResolveRosterSlot(Wrong, Assignment));
    Assignment.Roster.Add(Slot);
    TestNull(TEXT("重复Roster Player/Character拒绝"), GamePlatformArenaVerifiedAdmissionPolicy::ResolveRosterSlot(Admission, Assignment));
    return true;
}
#endif
