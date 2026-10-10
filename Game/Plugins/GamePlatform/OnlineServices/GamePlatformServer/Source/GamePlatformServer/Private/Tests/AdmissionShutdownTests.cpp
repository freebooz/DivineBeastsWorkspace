// 平台服务器HTTP准入所有权回归：只创建未发送请求，验证真实Provider关闭/取消，不连接生产后端。
#include "Server/GamePlatformHttpAdmissionProvider.h"
#include "HttpModule.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformAdmissionShutdownTest,
    "GamePlatform.Server.Admission.Shutdown", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformAdmissionShutdownTest::RunTest(const FString& Parameters)
{
    FGamePlatformHttpAdmissionProvider Provider;
    int32 Completions = 0;
    const auto RequestA = FHttpModule::Get().CreateRequest();
    const auto RequestB = FHttpModule::Get().CreateRequest();
    RequestA->OnProcessRequestComplete().BindLambda([](auto, auto, bool) {});
    RequestB->OnProcessRequestComplete().BindLambda([](auto, auto, bool) {});
    Provider.TrackRequest(FGuid::NewGuid(), RequestA, [&](auto Result)
    {
        ++Completions;
        TestTrue(TEXT("完成通知前全部委托已解绑"), !RequestA->OnProcessRequestComplete().IsBound() && !RequestB->OnProcessRequestComplete().IsBound());
        TestEqual(TEXT("关闭返回明确取消码"), Result.ErrorCode, FName(TEXT("ServerAdmissionCancelled")));
        Provider.Shutdown(); // 外部完成重入关闭不重复通知，不死锁。
    });
    Provider.TrackRequest(FGuid::NewGuid(), RequestB, [&](auto Result) { ++Completions; });
    Provider.Shutdown();
    Provider.Shutdown();
    TestEqual(TEXT("两个操作各一个终态"), Completions, 2);
    TestEqual(TEXT("关闭账本为空"), Provider.Requests.Num(), 0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformAdmissionCancelTest,
    "GamePlatform.Server.Admission.Cancel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformAdmissionCancelTest::RunTest(const FString& Parameters)
{
    FGamePlatformHttpAdmissionProvider Provider;
    const FGuid OperationId = FGuid::NewGuid();
    const auto Request = FHttpModule::Get().CreateRequest();
    int32 Completions = 0;
    Provider.TrackRequest(OperationId, Request, [&](auto Result) { ++Completions; });
    Provider.CancelOperation(OperationId);
    Provider.CancelOperation(OperationId);
    Provider.Shutdown();
    TestEqual(TEXT("取消后关闭仍仅一个终态"), Completions, 1);
    TestFalse(TEXT("取消已解绑完成回调"), Request->OnProcessRequestComplete().IsBound());
    return true;
}
#endif
