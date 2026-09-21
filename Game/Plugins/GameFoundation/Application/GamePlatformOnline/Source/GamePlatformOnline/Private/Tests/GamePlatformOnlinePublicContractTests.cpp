// 公共契约先行测试：防止默认结果误报成功、开发明文默认开放及退出状态混淆。
// 需要正式主工程编译；当前历史空描述文件阻断 UE，尚未执行此测试。
#if WITH_DEV_AUTOMATION_TESTS
#include "Interfaces/IGamePlatformOnlineService.h"
#include "Misc/AutomationTest.h"
#include <type_traits>
#include <utility>

static_assert(std::is_same_v<decltype(IGamePlatformOnlineService::Get(std::declval<UGameInstance&>())), IGamePlatformOnlineService*>);
static_assert(std::is_same_v<decltype(std::declval<IGamePlatformOnlineService&>().Configure(std::declval<const FGamePlatformOnlineConfiguration&>())), FGamePlatformResult>);
static_assert(std::is_same_v<decltype(std::declval<IGamePlatformOnlineService&>().GetAuthentication()), FGamePlatformOnlineAuthSnapshot>);
static_assert(std::is_same_v<decltype(std::declval<IGamePlatformOnlineService&>().GetDiagnostics()), FGamePlatformOnlineDiagnostics>);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformOnlinePublicDefaultsTest,
    "GamePlatform.Online.Contract.SafeDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformOnlinePublicDefaultsTest::RunTest(const FString& Parameters)
{
    const FGamePlatformOnlineConfiguration Configuration;
    TestTrue(TEXT("证书校验默认开启"), Configuration.bVerifyCertificates);
    TestFalse(TEXT("明文开发例外默认关闭"), Configuration.bAllowLoopbackHttpDevelopment);
    TestTrue(TEXT("配置不内置远端地址"), Configuration.ServiceOrigin.IsEmpty());
    TestFalse(TEXT("默认结果不是成功"), FGamePlatformOnlineResult{}.Result.IsSuccess());
    TestTrue(TEXT("默认认证为未登录"), FGamePlatformOnlineAuthSnapshot{}.State == EGamePlatformOnlineAuthState::SignedOut);
    TestTrue(TEXT("默认退出结果不得声称本地或远端已退出"), FGamePlatformOnlineLogoutResult{}.Disposition == EGamePlatformOnlineLogoutDisposition::NotExecuted);
    TestFalse(TEXT("默认请求身份无效"), FGamePlatformOnlineRequestHandle{}.RequestId.IsValid());
    TestTrue(TEXT("默认允许跨地图实例请求"), FGamePlatformOnlineRequestOptions{}.Lifetime == EGamePlatformOnlineRequestLifetime::GameInstance);
    return true;
}
#endif
