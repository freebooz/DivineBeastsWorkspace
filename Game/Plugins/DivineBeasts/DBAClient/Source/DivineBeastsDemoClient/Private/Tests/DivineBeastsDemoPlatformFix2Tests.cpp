#if WITH_DEV_AUTOMATION_TESTS
#include "GamePlatformOnlineClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/SubsystemCollection.h"
#include "UObject/StrongObjectPtr.h"
namespace DivineBeasts::Demo {bool VerifyManualFlowOwnershipForTests(FAutomationTestBase& Test);void QueueManualFlowAssetOwnershipForTests(FAutomationTestBase& Test);}
namespace
{
/** 只注入Configure失败/换绑，不能发送网络请求或实施认证业务。 */
class FRestoreConfigureProvider final:public IGamePlatformOnlineAuthProvider
{
public:
    bool bFailNext=false;int32 ConfigureCalls=0;TFunction<void()> OnConfigure;
    FGamePlatformResult Configure(const FGamePlatformOnlineConfiguration&) override
    {
        ++ConfigureCalls;auto Callback=MoveTemp(OnConfigure);if(Callback)Callback();
        if(bFailNext){bFailNext=false;return FGamePlatformResult::Failure(TEXT("Test.RestoreConfigureFailed"),TEXT("明确一次临时配置失败。"));}
        return FGamePlatformResult::Success();
    }
    void TryAutoLogin(FGamePlatformAuthCompletion) override{}
    void LoginWithCredentials(const FString&,const FString&,FGamePlatformAuthCompletion) override{}
    void Refresh(FGamePlatformAuthCompletion) override{}
    void Logout(FGamePlatformAuthLogoutCompletion) override{}
    void SendUnauthenticatedRequest(const FGuid&,const FGamePlatformAuthenticatedRequest&,FGamePlatformAuthenticatedCompletion) override{}
    void SendAuthenticatedRequest(const FGuid&,const FGamePlatformAuthenticatedRequest&,FGamePlatformAuthenticatedCompletion) override{}
    void CancelRequest(const FGuid&) override{}
    void InvalidateAuthenticationOperation() override{}
    bool ApplyAuthorization(IHttpRequest&) const override{return false;}
    void CancelAll() override{}
};
class FRestoreHarness
{
public:
    TStrongObjectPtr<UGameInstance> GI{NewObject<UGameInstance>()};
    TStrongObjectPtr<UGamePlatformOnlineClientSubsystem> Service{NewObject<UGamePlatformOnlineClientSubsystem>(GI.Get())};
    TSharedPtr<FRestoreConfigureProvider> Previous=MakeShared<FRestoreConfigureProvider>();
    TSharedPtr<FRestoreConfigureProvider> Fixture=MakeShared<FRestoreConfigureProvider>();
    FGamePlatformOnlineIdleProviderState Captured;FGamePlatformOnlineProviderRestoreLease Lease;
    FRestoreHarness(){FSubsystemCollection<UGameInstanceSubsystem> Collection;Service->Initialize(Collection);}
    ~FRestoreHarness(){Service->Deinitialize();}
    bool Install()
    {
        Service->SetProvider(Previous);
        FGamePlatformOnlineConfiguration C;C.ServiceOrigin=TEXT("https://manual.invalid");C.GameId=TEXT("ManualGame");C.ClientVersion=TEXT("1.0.0");
        if(!Service->Configure(C).IsSuccess() || !Service->CaptureIdleProviderState(Captured))return false;
        Service->SetProvider(Fixture);return true;
    }
    FGamePlatformResult Restore(){return Service->RestoreIdleProviderState(Captured,Fixture,Lease);}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualFlowOwnedHandleRegression,"DivineBeasts.PluginDemos.PlatformServices.Fix2.FlowOwnedHandles",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FManualFlowOwnedHandleRegression::RunTest(const FString&)
{return DivineBeasts::Demo::VerifyManualFlowOwnershipForTests(*this);}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualFlowAssetOwnedHandleRegression,"DivineBeasts.PluginDemos.PlatformServices.Fix2.FlowAssetOwnedHandles",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FManualFlowAssetOwnedHandleRegression::RunTest(const FString&)
{DivineBeasts::Demo::QueueManualFlowAssetOwnershipForTests(*this);return true;}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualOnlineRestoreRetryRegression,"DivineBeasts.PluginDemos.PlatformServices.Fix2.OnlineRestoreRetryLease",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FManualOnlineRestoreRetryRegression::RunTest(const FString&)
{
    FRestoreHarness H;if(!TestTrue(TEXT("真实Online空闲捕获并安装夹具"),H.Install()))return false;
    const int32 Before=H.Previous->ConfigureCalls;H.Previous->bFailNext=true;
    const auto First=H.Restore();TestFalse(TEXT("首次Configure失败不能吞掉"),First.IsSuccess());
    TestTrue(TEXT("原始配置失败码保留"),First.Code==TEXT("Test.RestoreConfigureFailed"));
    TestTrue(TEXT("失败保留有效未完成恢复租约"),H.Lease.IsValid() && !H.Lease.IsComplete());
    FGamePlatformOnlineProviderRestoreLease ForeignLease;
    TestFalse(TEXT("相同Previous指针不能替代私有恢复证明"),H.Service->RestoreIdleProviderState(H.Captured,H.Previous,ForeignLease).IsSuccess());
    TestTrue(TEXT("原租约Retry可成功恢复配置"),H.Restore().IsSuccess());
    TestTrue(TEXT("真实恢复完成"),H.Lease.IsComplete());
    TestEqual(TEXT("仅首次失败与第二次成功各Configure一次"),H.Previous->ConfigureCalls,Before+2);
    FGamePlatformOnlineIdleProviderState After;
    TestTrue(TEXT("恢复后可重新捕获空闲机制"),H.Service->CaptureIdleProviderState(After));
    TestTrue(TEXT("旧机制已精确恢复"),After.HasProvider(H.Previous));
    TestTrue(TEXT("唯一Configure恢复LoggedOut"),H.Service->GetSnapshot().State==EGamePlatformAuthState::LoggedOut);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualOnlineRestoreForeignRebindRegression,"DivineBeasts.PluginDemos.PlatformServices.Fix2.OnlineRestoreForeignRebind",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FManualOnlineRestoreForeignRebindRegression::RunTest(const FString&)
{
    {
        FRestoreHarness H;if(!TestTrue(TEXT("失败后外部换绑：安装"),H.Install()))return false;
        H.Previous->bFailNext=true;TestFalse(TEXT("第一次恢复保留临时失败"),H.Restore().IsSuccess());
        const auto External=MakeShared<FRestoreConfigureProvider>();H.Service->SetProvider(External);H.Service->SetProvider(H.Previous);
        const int32 Calls=H.Previous->ConfigureCalls;const auto Retry=H.Restore();
        TestFalse(TEXT("外部换走又换回相同指针仍拒绝"),Retry.IsSuccess());
        TestTrue(TEXT("必须是绑定所有权失败"),Retry.Code==TEXT("OnlineProviderOwnershipChanged"));
        TestEqual(TEXT("失效租约不再调用原Configure"),H.Previous->ConfigureCalls,Calls);
        TestFalse(TEXT("外部换绑后不能假称完成"),H.Lease.IsComplete());
    }
    {
        FRestoreHarness H;if(!TestTrue(TEXT("恢复回调内换绑：安装"),H.Install()))return false;
        H.Previous->OnConfigure=[&](){H.Service->SetProvider(H.Previous);}; // 相同指针也推进绑定代次。
        const auto First=H.Restore();TestFalse(TEXT("Configure中同步换绑不能提交旧配置"),First.IsSuccess());
        TestTrue(TEXT("唯一Configure源代次保护"),First.Code==TEXT("OnlineConfigurationSourceChanged"));
        const int32 Calls=H.Previous->ConfigureCalls;
        TestFalse(TEXT("重试不凭相同指针恢复权限"),H.Restore().IsSuccess());
        TestEqual(TEXT("旧租约不得再次配置外部绑定"),H.Previous->ConfigureCalls,Calls);
    }
    return true;
}
#endif
