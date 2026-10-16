// 平台客户端回归：捕获修订精度丢失、失败覆盖旧值、非法命令及主体越界；不联网、不模拟生产成功。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Party/GamePlatformPartyCodec.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformPartyCodecTest,
    "GamePlatform.Online.Party.StrictContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformPartyCodecTest::RunTest(const FString&)
{
    // 手工夹具的最大int64不得经过double；同一响应内的名单和队长身份必须一致。
    const FString Valid = TEXT(R"({"hasParty":true,"partyId":"party-1","leaderPlayerId":"p1","revision":"9223372036854775807","rosterLocked":false,"members":[{"playerId":"p1","displayName":"队长","ready":false,"online":true}],"requests":[],"serverTimeUtc":"2026-10-14T00:00:00Z"})");
    FGamePlatformPartySnapshot Value;
    TestTrue(TEXT("真实合同可解析"), GamePlatformPartyCodec::DecodeSnapshot(Valid, Value));
    TestEqual(TEXT("完整保留int64修订"), Value.Revision, MAX_int64);
    TestEqual(TEXT("真实名单保留"), Value.Members.Num(), 1);
    Value.PartyId = TEXT("sentinel");
    TestFalse(TEXT("拒绝数字修订"), GamePlatformPartyCodec::DecodeSnapshot(Valid.Replace(TEXT("\"9223372036854775807\""), TEXT("9007199254740993")), Value));
    TestEqual(TEXT("失败不覆盖旧数据"), Value.PartyId, FString(TEXT("sentinel")));
    TestFalse(TEXT("拒绝溢出"), GamePlatformPartyCodec::DecodeSnapshot(Valid.Replace(TEXT("9223372036854775807"), TEXT("9223372036854775808")), Value));
    TestFalse(TEXT("拒绝前导零"), GamePlatformPartyCodec::DecodeSnapshot(Valid.Replace(TEXT("9223372036854775807"), TEXT("01")), Value));
    TestFalse(TEXT("拒绝名单外队长"), GamePlatformPartyCodec::DecodeSnapshot(Valid.Replace(TEXT("\"leaderPlayerId\":\"p1\""), TEXT("\"leaderPlayerId\":\"p2\"")), Value));
    TestFalse(TEXT("拒绝字符串布尔而非容错转换"),GamePlatformPartyCodec::DecodeSnapshot(Valid.Replace(TEXT("\"hasParty\":true"),TEXT("\"hasParty\":\"true\"")),Value));
    TestFalse(TEXT("玩家身份不能由数值自动转成字符串"),GamePlatformPartyCodec::DecodeSnapshot(Valid.Replace(TEXT("\"partyId\":\"party-1\""),TEXT("\"partyId\":123")),Value));
    FGamePlatformPartyCommand Command;
    Command.PartyId=TEXT("party-1"); Command.Action=TEXT("invite"); Command.ClientRequestId=TEXT("request-1");
    Command.ExpectedRevision=MAX_int64; Command.TargetPlayerId=TEXT("p2"); Command.MembershipRequestId=TEXT("invite-1");
    FString Body;
    TestTrue(TEXT("合法邀请可编码"), GamePlatformPartyCodec::EncodeCommand(Command, Body));
    TestTrue(TEXT("修订以字符串编码"), Body.Contains(TEXT("\"9223372036854775807\"")));
    TestFalse(TEXT("不发送自报主体"), Body.Contains(TEXT("actorPlayerId")));
    Command.Action=TEXT("acceptArenaOffer");
    TestFalse(TEXT("普通队伍不接受竞技动作"), GamePlatformPartyCodec::EncodeCommand(Command, Body));
    Command.Action=TEXT("invite"); Command.TargetPlayerId.Empty();
    TestFalse(TEXT("邀请目标缺失拒绝"), GamePlatformPartyCodec::EncodeCommand(Command, Body));
    // 收件箱和命令回执是独立解码入口：覆盖作用域去重、严格数字类型和逻辑回执关联。
    const FString Inbox=TEXT(R"([{"partyId":"party-1","requestId":"invite-1","inviterPlayerId":"p2","revision":"7","expiresAtUtc":"2026-10-14T00:01:00Z","memberCount":2}])");
    TArray<FGamePlatformPartyRequest> Invitations;
    if(!TestTrue(TEXT("合法收件箱解码"),GamePlatformPartyCodec::DecodeInvitations(Inbox,Invitations)))return false;
    TestEqual(TEXT("收件箱人数保持"),Invitations[0].MemberCount,2);
    TestFalse(TEXT("人数不允许字符串容错"),GamePlatformPartyCodec::DecodeInvitations(Inbox.Replace(TEXT("\"memberCount\":2"),TEXT("\"memberCount\":\"2\"")),Invitations));
    TestEqual(TEXT("失败保留原收件箱"),Invitations.Num(),1);
    const FString Duplicate=Inbox.LeftChop(1)+TEXT(",")+Inbox.Mid(1);
    TestFalse(TEXT("同队伍重复邀请身份拒绝"),GamePlatformPartyCodec::DecodeInvitations(Duplicate,Invitations));
    const FString OtherParty=Inbox.LeftChop(1)+TEXT(",")+Inbox.Replace(TEXT("party-1"),TEXT("party-2")).Mid(1);
    TestTrue(TEXT("不同队伍可以有相同局部请求身份"),GamePlatformPartyCodec::DecodeInvitations(OtherParty,Invitations));
    TestEqual(TEXT("跨队伍请求均保留"),Invitations.Num(),2);
    Command.Action=TEXT("invite");Command.TargetPlayerId=TEXT("p2");
    const FString Receipt=TEXT("{\"clientRequestId\":\"request-1\",\"affectedPartyId\":\"party-1\",\"affectedRevision\":\"9223372036854775807\",\"snapshot\":")+Valid+TEXT("}");
    TestTrue(TEXT("回执与本次逻辑命令关联"),GamePlatformPartyCodec::DecodeCommandResult(Receipt,Command,Value));
    Value.PartyId=TEXT("sentinel");
    TestFalse(TEXT("别的逻辑请求回执不能完成当前命令"),GamePlatformPartyCodec::DecodeCommandResult(Receipt.Replace(TEXT("request-1"),TEXT("request-2")),Command,Value));
    TestEqual(TEXT("错误回执不污染已有快照"),Value.PartyId,FString(TEXT("sentinel")));
    TestFalse(TEXT("错误受影响队伍拒绝"),GamePlatformPartyCodec::DecodeCommandResult(Receipt.Replace(TEXT("\"affectedPartyId\":\"party-1\""),TEXT("\"affectedPartyId\":\"party-2\"")),Command,Value));
    // 观察快照必须绑定本人作用域和严格游标；其中任一嵌套字段失败都不能发布半成功。
    const FString Observation=TEXT("{\"scopeId\":\"party/p1\",\"cursor\":\"123-0\",\"changed\":true,\"resyncRequired\":false,\"serverTimeUtc\":\"2026-10-14T00:00:00Z\",\"snapshot\":")+Valid+TEXT(",\"invitations\":")+Inbox+TEXT("}");
    FGamePlatformPartyResult Observed;
    TestTrue(TEXT("完整事件观察可以解析"),GamePlatformPartyCodec::DecodeObservation(Observation,Observed));
    TestEqual(TEXT("持久游标保留"),Observed.ObservationCursor,FString(TEXT("123-0")));
    TestEqual(TEXT("嵌套邀请保留"),Observed.Invitations.Num(),1);
    TestFalse(TEXT("拒绝数字观察游标"),GamePlatformPartyCodec::DecodeObservation(Observation.Replace(TEXT("\"123-0\""),TEXT("123")),Observed));
    TestFalse(TEXT("拒绝游标整数溢出"),GamePlatformPartyCodec::DecodeObservation(Observation.Replace(TEXT("123-0"),TEXT("18446744073709551616-0")),Observed));
    TestFalse(TEXT("拒绝隐式变化布尔"),GamePlatformPartyCodec::DecodeObservation(Observation.Replace(TEXT("\"changed\":true"),TEXT("\"changed\":\"true\"")),Observed));
    TestEqual(TEXT("失败保留此前授权观察"),Observed.ObservationCursor,FString(TEXT("123-0")));
    return true;
}
#endif
