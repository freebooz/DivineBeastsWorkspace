#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GamePlatformOnlineClientSubsystem.h"

namespace
{
class FTestGamePlatformAuthProvider final : public IGamePlatformOnlineAuthProvider
{
public:
    virtual void TryAutoLogin(FGamePlatformAuthCompletion Completion) override
    {
        Completion(true, TEXT("AutoAccount"), EGamePlatformAuthError::None);
    }

    virtual void LoginWithCredentials(
        const FString& LoginName,
        const FString& Password,
        FGamePlatformAuthCompletion Completion) override
    {
        ++LoginCalls;
        Completion(true, TEXT("Account-1"), EGamePlatformAuthError::None);
    }

    virtual void Refresh(FGamePlatformAuthCompletion Completion) override
    {
        Completion(true, TEXT("Account-1"), EGamePlatformAuthError::None);
    }

    virtual void Logout(TFunction<void()> Completion) override
    {
        if (Completion)
        {
            Completion();
        }
    }

    virtual FString GetAuthorizationHeaderValue() const override
    {
        return TEXT("Bearer test-secret");
    }

    int32 LoginCalls = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformOnlineAuthLifecycleTest,
    "GamePlatform.Online.AuthLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformOnlineAuthLifecycleTest::RunTest(const FString& Parameters)
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
        EGamePlatformAuthError::InvalidCredentials);
    TestEqual(TEXT("无效凭据不得调用Provider"), Provider->LoginCalls, 0);

    FString OversizedLogin;
    OversizedLogin.Reserve(257);
    for (int32 Index = 0; Index < 257; ++Index)
    {
        OversizedLogin.AppendChar(TEXT('u'));
    }
    Online->LoginWithCredentials(OversizedLogin, TEXT("password"));
    TestEqual(
        TEXT("超长账号被拒绝"),
        Online->GetSnapshot().Error,
        EGamePlatformAuthError::InvalidCredentials);
    TestEqual(TEXT("超长凭据不得调用Provider"), Provider->LoginCalls, 0);

    Online->LoginWithCredentials(TEXT("user"), TEXT("password"));
    TestEqual(
        TEXT("有效登录进入Authenticated"),
        Online->GetSnapshot().State,
        EGamePlatformAuthState::Authenticated);
    TestEqual(TEXT("Provider仅被调用一次"), Provider->LoginCalls, 1);
    TestEqual(TEXT("公开快照保存AccountId"), Online->GetSnapshot().AccountId, FString(TEXT("Account-1")));
    TestEqual(
        TEXT("Token只通过瞬时接口读取"),
        Online->GetAuthorizationHeaderValueTransient(),
        FString(TEXT("Bearer test-secret")));

    Online->Logout();
    TestEqual(
        TEXT("登出回到LoggedOut"),
        Online->GetSnapshot().State,
        EGamePlatformAuthState::LoggedOut);
    TestTrue(TEXT("登出清空AccountId"), Online->GetSnapshot().AccountId.IsEmpty());
    return true;
}

#endif
