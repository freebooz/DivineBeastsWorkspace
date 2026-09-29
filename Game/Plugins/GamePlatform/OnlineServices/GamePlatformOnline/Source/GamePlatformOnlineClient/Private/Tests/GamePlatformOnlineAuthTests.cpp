#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GamePlatformOnlineClientSubsystem.h"

namespace
{
FGamePlatformAuthProviderResult SuccessfulAuthResult()
{
    FGamePlatformAuthProviderResult Result;
    Result.bSuccess = true;
    Result.AccountId = TEXT("Account-1");
    Result.SessionId = TEXT("Session-1");
    Result.AccessExpiresAt =
        FDateTime::UtcNow() + FTimespan::FromMinutes(10);
    Result.RefreshExpiresAt =
        FDateTime::UtcNow() + FTimespan::FromHours(1);
    Result.Error = EGamePlatformAuthError::None;
    return Result;
}

class FTestGamePlatformAuthProvider final
    : public IGamePlatformOnlineAuthProvider
{
public:
    virtual FGamePlatformResult Configure(
        const FGamePlatformOnlineConfiguration&) override
    {
        bConfigured = true;
        return FGamePlatformResult::Success();
    }

    virtual void TryAutoLogin(
        FGamePlatformAuthCompletion Completion) override
    {
        if (Completion)
        {
            Completion(SuccessfulAuthResult());
        }
    }

    virtual void LoginWithCredentials(
        const FString&,
        const FString&,
        FGamePlatformAuthCompletion Completion) override
    {
        ++LoginCalls;
        if (Completion)
        {
            Completion(SuccessfulAuthResult());
        }
    }

    virtual void Refresh(
        FGamePlatformAuthCompletion Completion) override
    {
        if (Completion)
        {
            Completion(SuccessfulAuthResult());
        }
    }

    virtual void Logout(FGamePlatformAuthLogoutCompletion Completion) override
    {
        if (Completion)
        {
            FGamePlatformAuthProviderLogoutResult Result;
            Result.bServerRevoked = true;
            Completion(MoveTemp(Result));
        }
    }

    virtual void SendUnauthenticatedRequest(
        const FGuid&,
        const FGamePlatformAuthenticatedRequest&,
        FGamePlatformAuthenticatedCompletion Completion) override
    {
        if (Completion)
        {
            FGamePlatformAuthenticatedResponse Result;
            Result.Error = EGamePlatformAuthError::None;
            Result.HttpStatusCode = 200;
            Result.Body = TEXT("{\"ready\":true,\"contractVersion\":\"1.0.0\",\"service\":\"gatewayservice\"}");
            Completion(MoveTemp(Result));
        }
    }

    virtual void SendAuthenticatedRequest(
        const FGuid&,
        const FGamePlatformAuthenticatedRequest&,
        FGamePlatformAuthenticatedCompletion Completion) override
    {
        if (Completion)
        {
            FGamePlatformAuthenticatedResponse Result;
            Result.Error = EGamePlatformAuthError::None;
            Result.HttpStatusCode = 200;
            Result.Body = TEXT("{}");
            Completion(MoveTemp(Result));
        }
    }

    virtual void CancelRequest(const FGuid&) override {}

    virtual bool ApplyAuthorization(IHttpRequest&) const override
    {
        return bConfigured;
    }

    virtual void CancelAll() override {}

    bool bConfigured = false;
    int32 LoginCalls = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformOnlineAuthValidationTest,
    "GamePlatform.Online.AuthValidation",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformOnlineAuthValidationTest::RunTest(
    const FString& Parameters)
{
    UGamePlatformOnlineClientSubsystem* Online =
        NewObject<UGamePlatformOnlineClientSubsystem>();
    const TSharedPtr<FTestGamePlatformAuthProvider> Provider =
        MakeShared<FTestGamePlatformAuthProvider>();

    Online->SetProvider(Provider);

    Online->LoginWithCredentials(TEXT(""), TEXT("password"));
    TestEqual(
        TEXT("空账号被拒绝"),
        Online->GetSnapshot().Error,
        EGamePlatformAuthError::ProviderUnavailable);
    TestEqual(
        TEXT("未配置时不得调用Provider"),
        Provider->LoginCalls,
        0);

    // Configure需要真实有效配置；这里使用HTTPS测试地址，不发起网络。
    FGamePlatformOnlineConfiguration Configuration;
    Configuration.ServiceOrigin = TEXT("https://gateway.example");
    Configuration.GameId = TEXT("test-game");
    Configuration.ClientVersion = TEXT("test-client");
    TestTrue(
        TEXT("合法在线配置被接受"),
        Online->Configure(Configuration).IsSuccess());

    FString OversizedLogin;
    OversizedLogin.Reserve(257);
    for (int32 Index = 0; Index < 257; ++Index)
    {
        OversizedLogin.AppendChar(TEXT('u'));
    }
    Online->LoginWithCredentials(
        OversizedLogin,
        TEXT("password"));
    TestEqual(
        TEXT("超长账号被拒绝"),
        Online->GetSnapshot().Error,
        EGamePlatformAuthError::InvalidCredentials);
    TestEqual(
        TEXT("超长凭据不得调用Provider"),
        Provider->LoginCalls,
        0);

    // 真实成功回调统一投递到后续游戏线程任务，避免同步Provider造成重入；
    // 此简单测试只验证同步Fail-Closed边界，异步状态由集成/Latent测试覆盖。
    return true;
}

#endif

