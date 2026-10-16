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
    return true;
}
#endif
