#if WITH_DEV_AUTOMATION_TESTS

#include "GamePlatformOnlineClientSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IGamePlatformOnlineService.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

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
    enum class ERequestMode : uint8
    {
        Normal,
        UnauthorizedOnce,
        AlwaysUnauthorized,
        TransientOnce
    };

    virtual FGamePlatformResult Configure(
        const FGamePlatformOnlineConfiguration&) override
    {
        bConfigured = true;
        return FGamePlatformResult::Success();
    }

    virtual void TryAutoLogin(
        FGamePlatformAuthCompletion Completion) override
    {
        PendingLogin = MoveTemp(Completion);
    }

    virtual void LoginWithCredentials(
        const FString&,
        const FString&,
        FGamePlatformAuthCompletion Completion) override
    {
        ++LoginCalls;
        PendingLogin = MoveTemp(Completion);
    }

    virtual void Refresh(
        FGamePlatformAuthCompletion Completion) override
    {
        ++RefreshCalls;
        PendingRefresh = MoveTemp(Completion);
    }

    virtual void Logout(
        FGamePlatformAuthLogoutCompletion Completion) override
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
        ++UnauthenticatedRequestCalls;
        if (Completion)
        {
            FGamePlatformAuthenticatedResponse Result;
            Result.Error = EGamePlatformAuthError::None;
            Result.HttpStatusCode = 200;
            Result.Body =
                TEXT("{\"ready\":true,\"contractVersion\":\"1.0.0\",\"service\":\"gatewayservice\"}");
            Completion(MoveTemp(Result));
        }
    }

    virtual void SendAuthenticatedRequest(
        const FGuid& RequestId,
        const FGamePlatformAuthenticatedRequest& Request,
        FGamePlatformAuthenticatedCompletion Completion) override
    {
        ++AuthenticatedRequestCalls;
        LastRequest = Request;
        int32& Attempt = Attempts.FindOrAdd(RequestId);
        ++Attempt;

        FGamePlatformAuthenticatedResponse Result;
        if (Request.RelativePath == TEXT("/v1/player/profile"))
        {
            Result.Error = EGamePlatformAuthError::None;
            Result.HttpStatusCode = 200;
            Result.Body =
                TEXT("{\"playerId\":\"Account-1\",\"gameId\":\"test-game\",\"displayName\":\"Player\",\"dataVersion\":1,\"revision\":9007199254740993,\"tutorialCompleted\":false,\"defaultWorldId\":\"world-a\",\"selectedCharacterId\":\"\",\"ownedCharacterIds\":[]}");
        }
        else if (RequestMode == ERequestMode::AlwaysUnauthorized ||
                 (RequestMode == ERequestMode::UnauthorizedOnce &&
                  Attempt == 1))
        {
            Result.Error = EGamePlatformAuthError::AuthExpired;
            Result.HttpStatusCode = 401;
            Result.bMayHaveReachedServer = true;
        }
        else if (RequestMode == ERequestMode::TransientOnce &&
                 Attempt == 1)
        {
            Result.Error = EGamePlatformAuthError::ServiceUnavailable;
            Result.HttpStatusCode = 503;
            Result.RetryAfterSeconds = 0.01;
            Result.bMayHaveReachedServer = true;
        }
        else
        {
            Result.Error = EGamePlatformAuthError::None;
            Result.HttpStatusCode = 200;
            Result.Body = TEXT("{}");
        }

        if (Completion)
        {
            Completion(MoveTemp(Result));
        }
    }

    virtual void CancelRequest(const FGuid& RequestId) override
    {
        CancelledRequests.Add(RequestId);
    }

    virtual void InvalidateAuthenticationOperation() override
    {
        ++InvalidatedAuthenticationOperations;
        PendingLogin = {};
    }

    virtual bool ApplyAuthorization(IHttpRequest&) const override
    {
        return bConfigured;
    }

    virtual void CancelAll() override
    {
        ++CancelAllCalls;
        PendingLogin = {};
        PendingRefresh = {};
    }

    void CompleteLogin()
    {
        FGamePlatformAuthCompletion Completion = MoveTemp(PendingLogin);
        if (Completion)
        {
            Completion(SuccessfulAuthResult());
        }
    }

    void CompleteRefresh()
    {
        FGamePlatformAuthCompletion Completion = MoveTemp(PendingRefresh);
        if (Completion)
        {
            Completion(SuccessfulAuthResult());
        }
    }

    bool HasPendingRefresh() const
    {
        return static_cast<bool>(PendingRefresh);
    }

    bool bConfigured = false;
    int32 LoginCalls = 0;
    int32 RefreshCalls = 0;
    int32 AuthenticatedRequestCalls = 0;
    int32 UnauthenticatedRequestCalls = 0;
    int32 InvalidatedAuthenticationOperations = 0;
    int32 CancelAllCalls = 0;
    ERequestMode RequestMode = ERequestMode::Normal;
    FGamePlatformAuthenticatedRequest LastRequest;
    TMap<FGuid, int32> Attempts;
    TSet<FGuid> CancelledRequests;
    FGamePlatformAuthCompletion PendingLogin;
    FGamePlatformAuthCompletion PendingRefresh;
};

class FGamePlatformOnlineProductionPathCommand final
    : public IAutomationLatentCommand
{
public:
    explicit FGamePlatformOnlineProductionPathCommand(
        FAutomationTestBase* InTest)
        : Test(InTest)
    {
    }

    virtual ~FGamePlatformOnlineProductionPathCommand() override
    {
        Cleanup();
    }

    virtual bool Update() override
    {
        if (FPlatformTime::Seconds() - StartedSeconds > 30.0)
        {
            Test->AddError(TEXT("GamePlatformOnline生产路径测试超过30秒。"));
            Cleanup();
            return true;
        }

        switch (Phase)
        {
        case 0:
            return Start();
        case 1:
            return WaitForLogin();
        case 2:
            return WaitForFirstRefresh();
        case 3:
            return WaitForReadReplay();
        case 4:
            return WaitForExplicitRefresh();
        case 5:
            return WaitForInvalidIdempotency();
        case 6:
            return WaitForReadRetry();
        case 7:
            return WaitForNonIdempotentWrite();
        case 8:
            return WaitForWriteRefresh();
        case 9:
            return WaitForLogout();
        default:
            Cleanup();
            return true;
        }
    }

private:
    bool Start()
    {
        if (!GEngine)
        {
            Test->AddError(TEXT("生产路径测试需要已初始化的UE引擎。"));
            return true;
        }

        Instance.Reset(NewObject<UGameInstance>(GEngine));
        Instance->InitializeStandalone(
            FName(*(
                TEXT("OnlineTest_") +
                FGuid::NewGuid().ToString(EGuidFormats::Digits))));

        Online =
            Instance->GetSubsystem<UGamePlatformOnlineClientSubsystem>();
        Service = IGamePlatformOnlineService::Get(*Instance.Get());
        Test->TestNotNull(TEXT("真实GameInstance创建Online子系统"), Online);
        Test->TestNotNull(TEXT("公共Online门面已注册"), Service);
        Test->TestTrue(
            TEXT("公共门面与生产子系统是同一实例"),
            Service == static_cast<IGamePlatformOnlineService*>(Online));
        if (!Online || !Service)
        {
            Cleanup();
            return true;
        }

        Provider = MakeShared<FTestGamePlatformAuthProvider>();
        Online->SetProvider(Provider);

        FGamePlatformOnlineConfiguration Configuration;
        Configuration.ServiceOrigin = TEXT("https://gateway.example");
        Configuration.GameId = TEXT("test-game");
        Configuration.ClientVersion = TEXT("test-client");
        Configuration.RetryBaseDelaySeconds = 0.01;
        Configuration.MaxRetryDelaySeconds = 0.05;
        Test->TestTrue(
            TEXT("测试Provider接受合法平台配置"),
            Service->Configure(Configuration).IsSuccess());

        FGamePlatformOnlineLoginRequest Invalid;
        Invalid.AccountName = TEXT("");
        Invalid.Credential = TEXT("password");
        Service->Login(
            MoveTemp(Invalid),
            {},
            [this](const FGamePlatformOnlineAuthenticationResult& Result)
            {
                ++InvalidLoginCompletions;
                InvalidLoginResult = Result;
            });
        Test->TestEqual(
            TEXT("无效登录不得调用Provider"),
            Provider->LoginCalls,
            0);

        FGamePlatformOnlineLoginRequest Login;
        Login.AccountName = TEXT("user-a");
        Login.Credential = TEXT("password-a");
        Service->Login(
            MoveTemp(Login),
            {},
            [this](const FGamePlatformOnlineAuthenticationResult& Result)
            {
                ++LoginCompletions;
                LoginResult = Result;
            });

        // 登录进行中时，即使第二次调用的凭据本身无效，也只能返回 AuthenticationBusy，
        // 不能推进认证代次或使首个合法登录失效。
        FGamePlatformOnlineLoginRequest BusyInvalid;
        BusyInvalid.AccountName = TEXT("");
        BusyInvalid.Credential = TEXT("");
        Service->Login(
            MoveTemp(BusyInvalid),
            {},
            [this](const FGamePlatformOnlineAuthenticationResult& Result)
            {
                ++BusyInvalidLoginCompletions;
                BusyInvalidLoginResult = Result;
            });

        FGamePlatformOnlineLoginRequest Busy;
        Busy.AccountName = TEXT("user-b");
        Busy.Credential = TEXT("password-b");
        Service->Login(
            MoveTemp(Busy),
            {},
            [this](const FGamePlatformOnlineAuthenticationResult& Result)
            {
                ++BusyLoginCompletions;
                BusyLoginResult = Result;
            });

        Test->TestEqual(
            TEXT("并发第二次登录不得进入Provider"),
            Provider->LoginCalls,
            1);
        Provider->CompleteLogin();
        Phase = 1;
        return false;
    }

    bool WaitForLogin()
    {
        if (InvalidLoginCompletions == 0 ||
            LoginCompletions == 0 ||
            BusyInvalidLoginCompletions == 0 ||
            BusyLoginCompletions == 0)
        {
            return false;
        }

        Test->TestEqual(
            TEXT("无效账号返回参数错误"),
            InvalidLoginResult.Error,
            EGamePlatformOnlineError::Unauthenticated);
        Test->TestTrue(
            TEXT("第一个登录真实成功"),
            LoginResult.Result.IsSuccess());
        Test->TestEqual(
            TEXT("并发无效登录也只能返回AuthenticationBusy"),
            BusyInvalidLoginResult.Error,
            EGamePlatformOnlineError::AuthenticationBusy);
        Test->TestEqual(
            TEXT("并发第二登录返回AuthenticationBusy"),
            BusyLoginResult.Error,
            EGamePlatformOnlineError::AuthenticationBusy);
        Test->TestEqual(
            TEXT("登录后公共状态为SignedIn"),
            Service->GetAuthentication().State,
            EGamePlatformOnlineAuthState::SignedIn);

        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> SameOriginRequest =
            FHttpModule::Get().CreateRequest();
        SameOriginRequest->SetURL(TEXT("https://gateway.example/telemetry/v1/batches"));
        Test->TestTrue(
            TEXT("同源平台请求允许附加认证"),
            Online->ApplyAuthorization(*SameOriginRequest));

        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> ForeignRequest =
            FHttpModule::Get().CreateRequest();
        ForeignRequest->SetURL(TEXT("https://example.invalid/telemetry"));
        Test->TestFalse(
            TEXT("外域请求禁止附加认证"),
            Online->ApplyAuthorization(*ForeignRequest));

        Service->ProbeService(
            {},
            [this](const FGamePlatformOnlineProbeResult& Result)
            {
                ++ProbeCompletions;
                ProbeResult = Result;
            });
        Service->GetCurrentPlayerProfile(
            {},
            [this](const FGamePlatformOnlineProfileResult& Result)
            {
                ++ProfileCompletions;
                ProfileResult = Result;
            });

        Provider->RequestMode =
            FTestGamePlatformAuthProvider::ERequestMode::UnauthorizedOnce;
        FGamePlatformAuthenticatedRequest ReadRequest;
        ReadRequest.Verb = TEXT("GET");
        ReadRequest.RelativePath = TEXT("/test/read");
        FirstReadHandle = Online->SendAuthenticatedRequest(
            ReadRequest,
            {},
            [this](FGamePlatformAuthenticatedResponse Result)
            {
                ++ReadCompletions;
                ReadErrors.Add(Result.Error);
            });
        SecondReadHandle = Online->SendAuthenticatedRequest(
            MoveTemp(ReadRequest),
            {},
            [this](FGamePlatformAuthenticatedResponse Result)
            {
                ++ReadCompletions;
                ReadErrors.Add(Result.Error);
            });

        Phase = 2;
        return false;
    }

    bool WaitForFirstRefresh()
    {
        if (ProbeCompletions == 0 ||
            ProfileCompletions == 0 ||
            Provider->RefreshCalls == 0 ||
            !Provider->HasPendingRefresh())
        {
            return false;
        }

        Test->TestTrue(TEXT("真实Probe成功"), ProbeResult.Result.IsSuccess());
        Test->TestTrue(TEXT("Probe确认依赖就绪"), ProbeResult.bReady);
        Test->TestEqual(
            TEXT("Profile int64修订号无精度丢失"),
            ProfileResult.Profile.Revision,
            static_cast<int64>(9007199254740993LL));
        Test->TestEqual(
            TEXT("两个401只启动一次Refresh"),
            Provider->RefreshCalls,
            1);

        Provider->CompleteRefresh();
        Phase = 3;
        return false;
    }

    bool WaitForReadReplay()
    {
        if (ReadCompletions < 2)
        {
            return false;
        }

        for (EGamePlatformAuthError Error : ReadErrors)
        {
            Test->TestEqual(
                TEXT("安全读取在一次刷新后成功重放"),
                Error,
                EGamePlatformAuthError::None);
        }
        Test->TestEqual(
            TEXT("第一读取只发送两次"),
            Provider->Attempts.FindRef(FirstReadHandle.RequestId),
            2);
        Test->TestEqual(
            TEXT("第二读取只发送两次"),
            Provider->Attempts.FindRef(SecondReadHandle.RequestId),
            2);

        FirstExplicitRefresh = Service->RefreshAuthentication(
            {},
            [this](const FGamePlatformOnlineAuthenticationResult& Result)
            {
                ++CancelledRefreshCompletions;
                CancelledRefreshResult = Result;
            });
        SecondExplicitRefresh = Service->RefreshAuthentication(
            {},
            [this](const FGamePlatformOnlineAuthenticationResult& Result)
            {
                ++SuccessfulRefreshCompletions;
                SuccessfulRefreshResult = Result;
            });

        Test->TestEqual(
            TEXT("两个显式刷新共享一个Provider调用"),
            Provider->RefreshCalls,
            2);
        Test->TestTrue(
            TEXT("可取消单个显式刷新等待者"),
            Service->Cancel(FirstExplicitRefresh));
        Provider->CompleteRefresh();
        Phase = 4;
        return false;
    }

    bool WaitForExplicitRefresh()
    {
        if (CancelledRefreshCompletions == 0 ||
            SuccessfulRefreshCompletions == 0)
        {
            return false;
        }

        Test->TestEqual(
            TEXT("取消的刷新等待者只收到Cancelled"),
            CancelledRefreshResult.Error,
            EGamePlatformOnlineError::Cancelled);
        Test->TestTrue(
            TEXT("另一个刷新等待者不受取消影响"),
            SuccessfulRefreshResult.Result.IsSuccess());

        InvalidIdempotencyProviderCalls =
            Provider->AuthenticatedRequestCalls;
        FGamePlatformAuthenticatedRequest InvalidWrite;
        InvalidWrite.Verb = TEXT("POST");
        InvalidWrite.RelativePath = TEXT("/test/idempotent");
        InvalidWrite.bIdempotent = true;
        Online->SendAuthenticatedRequest(
            MoveTemp(InvalidWrite),
            {},
            [this](FGamePlatformAuthenticatedResponse Result)
            {
                ++InvalidIdempotencyCompletions;
                InvalidIdempotencyError = Result.Error;
            });
        Phase = 5;
        return false;
    }

    bool WaitForInvalidIdempotency()
    {
        if (InvalidIdempotencyCompletions == 0)
        {
            return false;
        }

        Test->TestEqual(
            TEXT("幂等写缺少Idempotency-Key被拒绝"),
            InvalidIdempotencyError,
            EGamePlatformAuthError::InvalidRequest);
        Test->TestEqual(
            TEXT("非法幂等写不得进入Provider"),
            Provider->AuthenticatedRequestCalls,
            InvalidIdempotencyProviderCalls);

        Provider->RequestMode =
            FTestGamePlatformAuthProvider::ERequestMode::TransientOnce;
        FGamePlatformAuthenticatedRequest RetryRead;
        RetryRead.Verb = TEXT("GET");
        RetryRead.RelativePath = TEXT("/test/retry");
        RetryReadHandle = Online->SendAuthenticatedRequest(
            MoveTemp(RetryRead),
            {},
            [this](FGamePlatformAuthenticatedResponse Result)
            {
                ++RetryReadCompletions;
                RetryReadResult = Result;
            });
        Phase = 6;
        return false;
    }

    bool WaitForReadRetry()
    {
        if (RetryReadCompletions == 0)
        {
            return false;
        }

        Test->TestTrue(
            TEXT("临时失败的安全读取按策略重试成功"),
            RetryReadResult.IsSuccess());
        Test->TestEqual(
            TEXT("安全读取只重试一次"),
            Provider->Attempts.FindRef(RetryReadHandle.RequestId),
            2);

        Provider->RequestMode =
            FTestGamePlatformAuthProvider::ERequestMode::AlwaysUnauthorized;
        FGamePlatformAuthenticatedRequest Write;
        Write.Verb = TEXT("POST");
        Write.RelativePath = TEXT("/test/write");
        Write.bIdempotent = false;
        NonIdempotentWriteHandle = Online->SendAuthenticatedRequest(
            MoveTemp(Write),
            {},
            [this](FGamePlatformAuthenticatedResponse Result)
            {
                ++NonIdempotentWriteCompletions;
                NonIdempotentWriteResult = Result;
            });
        Phase = 7;
        return false;
    }

    bool WaitForNonIdempotentWrite()
    {
        if (NonIdempotentWriteCompletions == 0 ||
            !Provider->HasPendingRefresh())
        {
            return false;
        }

        Test->TestEqual(
            TEXT("非幂等写401不自动重放"),
            Provider->Attempts.FindRef(
                NonIdempotentWriteHandle.RequestId),
            1);
        Test->TestEqual(
            TEXT("非幂等写以AuthExpired结束"),
            NonIdempotentWriteResult.Error,
            EGamePlatformAuthError::AuthExpired);
        Test->TestEqual(
            TEXT("非幂等401只为后续上下文启动一次刷新"),
            Provider->RefreshCalls,
            3);

        Provider->RequestMode =
            FTestGamePlatformAuthProvider::ERequestMode::Normal;
        Provider->CompleteRefresh();
        Phase = 8;
        return false;
    }

    bool WaitForWriteRefresh()
    {
        if (Service->GetAuthentication().State !=
            EGamePlatformOnlineAuthState::SignedIn)
        {
            return false;
        }

        Service->Logout(
            [this](const FGamePlatformOnlineLogoutResult& Result)
            {
                ++LogoutCompletions;
                LogoutResult = Result;
            });
        Phase = 9;
        return false;
    }

    bool WaitForLogout()
    {
        if (LogoutCompletions == 0)
        {
            return false;
        }

        Test->TestTrue(
            TEXT("远端撤销确认后Logout成功"),
            LogoutResult.Result.IsSuccess());
        Test->TestEqual(
            TEXT("Logout区分服务端已撤销"),
            LogoutResult.Disposition,
            EGamePlatformOnlineLogoutDisposition::ServerRevoked);
        Test->TestEqual(
            TEXT("Logout后公共认证为SignedOut"),
            Service->GetAuthentication().State,
            EGamePlatformOnlineAuthState::SignedOut);

        Cleanup();
        return true;
    }

    void Cleanup()
    {
        if (!Instance.IsValid())
        {
            return;
        }

        UWorld* World = Instance->GetWorld();
        if (World)
        {
            World->DestroyWorld(false);
        }
        Instance->Shutdown();
        Test->TestNull(
            TEXT("GameInstance关闭后公共Online门面已反注册"),
            IGamePlatformOnlineService::Get(*Instance.Get()));
        if (World && GEngine)
        {
            GEngine->DestroyWorldContext(World);
        }

        Service = nullptr;
        Online = nullptr;
        Provider.Reset();
        Instance.Reset();
    }

    FAutomationTestBase* Test = nullptr;
    double StartedSeconds = FPlatformTime::Seconds();
    int32 Phase = 0;

    TStrongObjectPtr<UGameInstance> Instance;
    UGamePlatformOnlineClientSubsystem* Online = nullptr;
    IGamePlatformOnlineService* Service = nullptr;
    TSharedPtr<FTestGamePlatformAuthProvider> Provider;

    int32 InvalidLoginCompletions = 0;
    int32 LoginCompletions = 0;
    int32 BusyInvalidLoginCompletions = 0;
    int32 BusyLoginCompletions = 0;
    int32 ProbeCompletions = 0;
    int32 ProfileCompletions = 0;
    int32 ReadCompletions = 0;
    int32 CancelledRefreshCompletions = 0;
    int32 SuccessfulRefreshCompletions = 0;
    int32 InvalidIdempotencyCompletions = 0;
    int32 RetryReadCompletions = 0;
    int32 NonIdempotentWriteCompletions = 0;
    int32 LogoutCompletions = 0;
    int32 InvalidIdempotencyProviderCalls = 0;

    FGamePlatformOnlineAuthenticationResult InvalidLoginResult;
    FGamePlatformOnlineAuthenticationResult LoginResult;
    FGamePlatformOnlineAuthenticationResult BusyInvalidLoginResult;
    FGamePlatformOnlineAuthenticationResult BusyLoginResult;
    FGamePlatformOnlineProbeResult ProbeResult;
    FGamePlatformOnlineProfileResult ProfileResult;
    FGamePlatformOnlineAuthenticationResult CancelledRefreshResult;
    FGamePlatformOnlineAuthenticationResult SuccessfulRefreshResult;
    FGamePlatformAuthenticatedResponse RetryReadResult;
    FGamePlatformAuthenticatedResponse NonIdempotentWriteResult;
    FGamePlatformOnlineLogoutResult LogoutResult;
    EGamePlatformAuthError InvalidIdempotencyError =
        EGamePlatformAuthError::Unknown;
    TArray<EGamePlatformAuthError> ReadErrors;

    FGamePlatformOnlineRequestHandle FirstReadHandle;
    FGamePlatformOnlineRequestHandle SecondReadHandle;
    FGamePlatformOnlineRequestHandle FirstExplicitRefresh;
    FGamePlatformOnlineRequestHandle SecondExplicitRefresh;
    FGamePlatformOnlineRequestHandle RetryReadHandle;
    FGamePlatformOnlineRequestHandle NonIdempotentWriteHandle;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformOnlineProductionPathTest,
    "GamePlatform.Online.ProductionPath",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FGamePlatformOnlineProductionPathTest::RunTest(
    const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(
        FGamePlatformOnlineProductionPathCommand(this));
    return true;
}

#endif

