// 项目客户端真实匹配响应回归：只构造传输JSON夹具，不运行HTTP或伪造服务器成功；必须解码Gateway实际信封并核对逻辑命令身份。
#include "Misc/AutomationTest.h"
#include "Client/DivineBeastsArenaOnlineMatchmakingAdapter.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaBackendCodecTest,"DivineBeasts.Arena.Backend.CommandEnvelope",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaBackendCodecTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<UGamePlatformOnlineClientSubsystem> Online(NewObject<UGamePlatformOnlineClientSubsystem>(Instance.Get()));
    FDivineBeastsArenaOnlineMatchmakingAdapter Adapter(*Online,*Instance);
    const FString Snapshot=TEXT(R"({"state":"idle","ticketId":"","partyId":"","candidateId":"","arenaModeId":"","regionId":"test","reasonCode":"","revision":"0","candidateRevision":"0","serverTimeUtc":"2026-10-15T00:00:00Z","deadlineUtc":null,"serviceReady":true,"development":true,"canCancel":false,"canReady":false,"canReply":false,"participants":[]})");
    FDivineBeastsArenaQueueSnapshot Out;
    const FString Envelope=TEXT("{\"clientRequestId\":\"command-1\",\"snapshot\":")+Snapshot+TEXT("}");
    TestTrue(TEXT("真实Gateway信封可读"),Adapter.DecodeCommandResponse(Envelope,TEXT("command-1"),Out));
    TestTrue(TEXT("服务能力与初始状态保持"),Out.bKnown&&Out.bServiceReady&&Out.bDevelopment&&Out.Phase==TEXT("idle"));
    TestFalse(TEXT("其它命令的迟到响应不能确认本人写入"),Adapter.DecodeCommandResponse(Envelope,TEXT("command-2"),Out));
    TestFalse(TEXT("裸快照不能冒充已确认命令"),Adapter.DecodeCommandResponse(Snapshot,TEXT("command-1"),Out));
    TestFalse(TEXT("信封缺少快照必须失败"),Adapter.DecodeCommandResponse(TEXT("{\"clientRequestId\":\"command-1\"}"),TEXT("command-1"),Out));

    // 回归保护：真实约战邀请必须能经同一个观察/命令解码器投影，不能因未知state丢失邀请。
    Adapter.PlayerId=TEXT("player-b");
    const FString Invitation=TEXT(R"({"state":"challengeInvitation","ticketId":"","partyId":"","candidateId":"","arenaModeId":"Arena.Mode.Duel1v1","regionId":"test","reasonCode":"","revision":"0","candidateRevision":"0","serverTimeUtc":"2026-10-31T00:00:00Z","deadlineUtc":"2026-10-31T00:01:00Z","serviceReady":true,"development":true,"canCancel":false,"canReady":false,"canReply":false,"participants":[{"playerId":"player-a","teamId":"A","accepted":false,"ready":false},{"playerId":"player-b","teamId":"B","accepted":false,"ready":false}],"challenge":{"challengeId":"challenge-1","revision":"1","ownerPlayerId":"player-a","targetPlayerId":"player-b","arenaModeId":"Arena.Mode.Duel1v1","regionId":"test","deadlineUtc":"2026-10-31T00:01:00Z","canReply":true,"canCancel":false,"teamAPlayerIds":["player-a"],"teamBPlayerIds":["player-b"]}})");
    const auto InvitationEnvelope=[&](const FString& View){return TEXT("{\"clientRequestId\":\"challenge-command\",\"snapshot\":")+View+TEXT("}");};
    TestTrue(TEXT("指定双方邀请通过唯一解码器"),Adapter.DecodeCommandResponse(InvitationEnvelope(Invitation),TEXT("challenge-command"),Out));
    TestTrue(TEXT("本人目标保留真实回复资格及双方名单"),Out.Challenge.bPresent&&Out.Challenge.bCanReply&&Out.Challenge.TeamAPlayerIds==TArray<FString>{TEXT("player-a") }&&Out.Challenge.TeamBPlayerIds==TArray<FString>{TEXT("player-b") });
    Adapter.ApplySnapshot(Out);
    TestEqual(TEXT("约战邀请保持面板阶段而非随机空闲资格"),Adapter.GetObservedState(),EGamePlatformArenaClientFlowState::Matchmaking);
    TestFalse(TEXT("双方名单不能重叠"),Adapter.DecodeCommandResponse(InvitationEnvelope(Invitation.Replace(TEXT("\"teamBPlayerIds\":[\"player-b\"]"),TEXT("\"teamBPlayerIds\":[\"player-a\"]"))),TEXT("challenge-command"),Out));
    TestFalse(TEXT("缺失邀请不能伪装活动邀请"),Adapter.DecodeCommandResponse(InvitationEnvelope(Invitation.Left(Invitation.Find(TEXT(",\"challenge\":")))+TEXT("}")),TEXT("challenge-command"),Out));
    TestFalse(TEXT("过期邀请不能保留可回复权限"),Adapter.DecodeCommandResponse(InvitationEnvelope(Invitation.Replace(TEXT("2026-10-31T00:01:00Z"),TEXT("2026-10-30T23:59:59Z"))),TEXT("challenge-command"),Out));
    const FString CancelledInvitation=Snapshot.Replace(TEXT("\"state\":\"idle\""),TEXT("\"state\":\"cancelled\""));
    TestTrue(TEXT("邀请取消后无票据无名单的终态可收敛"),Adapter.DecodeCommandResponse(InvitationEnvelope(CancelledInvitation),TEXT("challenge-command"),Out));
    Adapter.PlayerId=TEXT("unrelated-player");
    TestFalse(TEXT("其它主体邀请不可作为本人快照"),Adapter.DecodeCommandResponse(InvitationEnvelope(Invitation),TEXT("challenge-command"),Out));
    const uint64 OldGeneration=Adapter.Generation;
    FGamePlatformAuthSnapshot NewAuthentication;NewAuthentication.AuthGeneration=FGuid::NewGuid();
    Adapter.AuthenticationChanged(NewAuthentication);
    TestTrue(TEXT("认证换代立即撤销旧邀请及迟到回包代次"),!Adapter.GetSnapshot().bKnown&&!Adapter.GetSnapshot().Challenge.bPresent&&Adapter.Generation>OldGeneration);
    return true;
}
#endif
