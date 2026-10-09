// 领域真实Online请求路径回归：测试Provider只控制网络完成时序，不含真实票据/后端或支付。
// 覆盖正常业务完成、业务解析失败、取消迟到响应和传输退出后弱引用释放；不测生产HTTP对象内存。
#include "Transport/GamePlatformCommerceGatewayHttpTransport.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "Types/GamePlatformResult.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"
#include "UObject/StrongObjectPtr.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FDomainLifetimeProvider final : public IGamePlatformOnlineAuthProvider
{
public:
    FGamePlatformAuthCompletion LoginCompletion;
    FGamePlatformAuthenticatedCompletion Pending;
    FGamePlatformResult Configure(const FGamePlatformOnlineConfiguration&) override { return FGamePlatformResult::Success(); }
    void TryAutoLogin(FGamePlatformAuthCompletion Completion) override { LoginCompletion = MoveTemp(Completion); }
    void LoginWithCredentials(const FString&, const FString&, FGamePlatformAuthCompletion Completion) override { LoginCompletion = MoveTemp(Completion); }
    void Refresh(FGamePlatformAuthCompletion Completion) override { LoginCompletion = MoveTemp(Completion); }
    void Logout(FGamePlatformAuthLogoutCompletion Completion) override { Completion({}); }
    void SendUnauthenticatedRequest(const FGuid&, const FGamePlatformAuthenticatedRequest&, FGamePlatformAuthenticatedCompletion Completion) override { Pending = MoveTemp(Completion); }
    void SendAuthenticatedRequest(const FGuid&, const FGamePlatformAuthenticatedRequest&, FGamePlatformAuthenticatedCompletion Completion) override { Pending = MoveTemp(Completion); }
    void CancelRequest(const FGuid&) override { Pending = {}; }
    void InvalidateAuthenticationOperation() override { LoginCompletion = {}; }
    bool ApplyAuthorization(IHttpRequest&) const override { return false; }
    void CancelAll() override { LoginCompletion = {}; Pending = {}; }
    void Complete(int32 Status, const FString& Body = TEXT("{}"))
    {
        auto Completion = MoveTemp(Pending);
        FGamePlatformAuthenticatedResponse Response;
        Response.HttpStatusCode = Status; Response.Body = Body;
        Response.Error = Status == 200 ? EGamePlatformAuthError::None : EGamePlatformAuthError::Forbidden;
        Completion(MoveTemp(Response));
    }
};
class FDomainLifetimeCommand final : public IAutomationLatentCommand
{
public:
    explicit FDomainLifetimeCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    ~FDomainLifetimeCommand() override
    {
        Transport.Reset();
        if (Instance.Get())
        {
            UWorld* World = Instance->GetWorld();
            if (World) { World->DestroyWorld(false); }
            Instance->Shutdown();
            if (World) { GEngine->DestroyWorldContext(World); }
        }
    }
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 15.0) { Test->AddError(TEXT("领域Online终态回归超时")); return true; }
        if (Phase == 0)
        {
            Instance.Reset(NewObject<UGameInstance>(GEngine));
            Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
            Online = Instance->GetSubsystem<UGamePlatformOnlineClientSubsystem>();
            if (!Online) { Test->AddError(TEXT("Online实例缺失")); return true; }
            Provider = MakeShared<FDomainLifetimeProvider>(); Online->SetProvider(Provider);
            FGamePlatformOnlineConfiguration Config; Config.ServiceOrigin = TEXT("https://domain-tests.example"); Config.GameId = TEXT("domain-test"); Config.ClientVersion = TEXT("test");
            Test->TestTrue(TEXT("测试Online配置合法"), Online->Configure(Config).IsSuccess());
            Online->LoginWithCredentials(TEXT("Fixture"), TEXT("FixtureOnly"));
            FGamePlatformAuthProviderResult Auth; Auth.bSuccess = true; Auth.Error = EGamePlatformAuthError::None;
            Auth.AccountId = TEXT("Fixture"); Auth.SessionId = TEXT("FixtureSession");
            Auth.AccessExpiresAt = FDateTime::UtcNow() + FTimespan::FromMinutes(10); Auth.RefreshExpiresAt = FDateTime::UtcNow() + FTimespan::FromHours(1);
            auto Login = MoveTemp(Provider->LoginCompletion); Login(Auth); ++Phase; return false;
        }
        if (Phase == 1)
        {
            if (Online->GetSnapshot().State != EGamePlatformAuthState::Authenticated) { return false; }
            Start(); ++Phase; return false;
        }
        if (Phase == 2)
        {
            if (!Provider->Pending) { return false; }
            Provider->Complete(200); ++Phase; return false;
        }
        if (Phase == 3)
        {
            if (Completions != 1) { return false; }
            Test->TestEqual(TEXT("无效JSON明确完成为解析错误"), LastError, EGamePlatformCommerceError::InvalidResponse);
            Transport.Reset(); Test->TestFalse(TEXT("解析失败终态后传输弱引用释放"), Weak.IsValid());
            Start(); ++Phase; return false;
        }
        if (Phase == 4)
        {
            if (!Provider->Pending) { return false; }
            auto Late = MoveTemp(Provider->Pending);
            Transport.Reset(); Test->TestFalse(TEXT("取消/退出不被完成委托强持有"), Weak.IsValid());
            FGamePlatformAuthenticatedResponse Response; Response.Error = EGamePlatformAuthError::None; Response.HttpStatusCode = 200; Response.Body = TEXT("{}");
            Late(Response); ++Phase; return false;
        }
        if (Phase == 5)
        {
            Test->TestEqual(TEXT("取消后迟到响应不触发旧领域回调"), Completions, 1);
            Start(); ++Phase; return false;
        }
        if (Phase == 6)
        {
            if (!Provider->Pending) { return false; }
            Provider->Complete(403); ++Phase; return false;
        }
        if (Phase == 7)
        {
            if (Completions < 2) { return false; }
            Test->TestEqual(TEXT("认证拒绝明确映射为权限错误"), LastError, EGamePlatformCommerceError::Unauthorized);
            Transport.Reset(); Test->TestFalse(TEXT("错误终态后传输弱引用释放"), Weak.IsValid());
            Start(); ++Phase; return false;
        }
        if (Phase == 8)
        {
            if (!Provider->Pending) { return false; }
            Provider->Complete(200, TEXT("{\"server_time_utc\":\"2026-09-30T00:00:00Z\",\"catalog\":{\"catalog_revision\":1,\"generated_at\":\"2026-09-30T00:00:00Z\"}}")); ++Phase; return false;
        }
        if (Completions < 3) { return false; }
        Test->TestEqual(TEXT("正常领域JSON完成成功"), LastError, EGamePlatformCommerceError::None);
        Transport.Reset(); Test->TestFalse(TEXT("正常终态后传输弱引用释放"), Weak.IsValid());
        auto Unconfigured = MakeShared<FGamePlatformCommerceGatewayHttpTransport, ESPMode::ThreadSafe>(static_cast<UGamePlatformOnlineClientSubsystem*>(nullptr));
        Test->TestFalse(TEXT("未配置启动失败不发起网络请求"), Unconfigured->BeginGetCatalog([](auto, auto) {}));
        return true;
    }
private:
    void Start()
    {
        Transport = MakeShared<FGamePlatformCommerceGatewayHttpTransport, ESPMode::ThreadSafe>(Online); Weak = Transport;
        Test->TestTrue(TEXT("领域通过Online受理读取"), Transport->BeginGetCatalog([this](auto, auto Error) { ++Completions; LastError = Error; }));
    }
    FAutomationTestBase* Test;
    double Started = FPlatformTime::Seconds();
    int32 Phase = 0; int32 Completions = 0;
    EGamePlatformCommerceError LastError = EGamePlatformCommerceError::None;
    TStrongObjectPtr<UGameInstance> Instance;
    UGamePlatformOnlineClientSubsystem* Online = nullptr;
    TSharedPtr<FDomainLifetimeProvider> Provider;
    TSharedPtr<FGamePlatformCommerceGatewayHttpTransport, ESPMode::ThreadSafe> Transport;
    TWeakPtr<FGamePlatformCommerceGatewayHttpTransport, ESPMode::ThreadSafe> Weak;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDomainTransportOwnershipTest, "GamePlatform.Commerce.Client.OnlineOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDomainTransportOwnershipTest::RunTest(const FString&)
{ ADD_LATENT_AUTOMATION_COMMAND(FDomainLifetimeCommand(this)); return true; }
#endif
