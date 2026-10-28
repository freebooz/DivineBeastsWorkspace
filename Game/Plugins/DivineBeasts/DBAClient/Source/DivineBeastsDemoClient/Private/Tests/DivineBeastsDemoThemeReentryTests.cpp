#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/DivineBeastsDemoPresentationLiveUtilities.h"
#include "Tests/DivineBeastsDemoPresentationReentryFixture.h"
#include "HAL/PlatformTime.h"
namespace
{
using namespace DivineBeasts::Demo;using namespace DivineBeasts::Demo::Tests;
struct FThemeReentryRun:TSharedFromThis<FThemeReentryRun>
{
    FAutomationTestBase* Test=nullptr;TSharedPtr<FPresentationManualState> S;
    TStrongObjectPtr<UDivineBeastsDemoPresentationTestObserver> Observer;
    FGuid Expected;FGamePlatformUIThemeSnapshot Original;FGamePlatformResult StopInPublish;
    int32 Step=-1,StopCalls=0;bool bEvent=false,bSuccess=false,bStopOnChanged=false,bDone=false;double Deadline=0;
    UGamePlatformUIManagerSubsystem* UI()const{return S->Local<UGamePlatformUIManagerSubsystem>();}
    void Finish()
    {
        if(auto* M=UI()){if(Expected.IsValid())M->CancelThemeRequest(Expected);if(Observer.IsValid()){M->OnThemeChanged.RemoveDynamic(Observer.Get(),&UDivineBeastsDemoPresentationTestObserver::OnChanged);M->OnThemeRequestFinished.RemoveDynamic(Observer.Get(),&UDivineBeastsDemoPresentationTestObserver::OnFinished);}}
        if(Observer.IsValid())Observer->Clear();Observer.Reset();S->UnbindEvents();
        // 外部修订冲突用例只撤本测试宿主，不把失败Stop伪报成功，也不回写外部主题。
        if(S->bHost&&S->Host.IsValid()){S->Host->Release(S->Context.Token);S->bHost=false;}bDone=true;
    }
    void Switch()
    {auto Values=ThemeValues(*S);FGamePlatformResult Accepted;S->ExecuteUI(TEXT("SwitchTheme"),FPresentationArgs{S->Context,Values},[&](const auto&,const auto& R){Accepted=R;});Expected=S->ThemeRequest;bEvent=false;if(!Accepted.IsSuccess()||!Expected.IsValid()){Test->AddError(TEXT("真实适配SwitchTheme未受理。"));Finish();}}
    void FreshState()
    {S->UnbindEvents();S=MakeState(*Test,TEXT("GamePlatformUI.theme"));if(!S){bDone=true;return;}S->OriginalTheme=UI()->GetThemeSnapshot();S->ThemeExpectedRevision=S->OriginalTheme.Revision;S->ThemeSource=UI();Original=S->OriginalTheme;}
    void Tick()
    {
        if(bDone||!bEvent)return;bEvent=false;
        if(Step==-1)
        {
            if(!bSuccess||!CanBeginPresentationThemeFixture(UI()->GetThemeSnapshot())){Test->AddError(TEXT("真实Data主题基线未完成。"));Finish();return;}
            S->OriginalTheme=UI()->GetThemeSnapshot();S->ThemeExpectedRevision=S->OriginalTheme.Revision;S->ThemeSource=UI();Original=S->OriginalTheme;
            Step=0;Switch();if(bDone)return;const auto Owned=Expected;FGamePlatformResult Probe;
            TArray<FDivineBeastsDemoParameterValue> Empty;S->ExecuteUI(TEXT("RejectInvalidTheme"),FPresentationArgs{S->Context,Empty},[&](const auto&,const auto& R){Probe=R;});
            Test->TestFalse(TEXT("真实非法请求拒绝"),Probe.IsSuccess());Test->TestEqual(TEXT("非法探针不覆盖有效在途身份"),S->ThemeRequest,Owned);
            const auto Stopped=S->Cleanup();Test->TestTrue(TEXT("在途主题真实取消后Stop释放宿主"),Stopped.IsSuccess());Test->TestFalse(TEXT("取消后没有孤儿ThemeRequest"),S->ThemeRequest.IsValid());
            // 等待另一次真实服务完成使Data调度前进，不伪造旧完成回调。
            Expected=UI()->RequestThemeAsync(Original.ThemeId,Original.Context);bEvent=false;if(!Expected.IsValid()){Test->AddError(TEXT("取消后的真实后续请求未受理。"));Finish();}return;
        }
        if(Step==0)
        {
            Test->TestTrue(TEXT("取消后的真实Data链能继续完成"),bSuccess);Test->TestEqual(TEXT("已Stop旧请求不能覆盖后续主题"),UI()->GetThemeSnapshot().ThemeId,Original.ThemeId);
            FreshState();if(bDone)return;Step=1;bStopOnChanged=true;Switch();return;
        }
        if(Step==1)
        {
            Test->TestTrue(TEXT("发布后准确成功terminal仍已到达"),bSuccess);Test->TestEqual(TEXT("ThemeChanged中只发一次Stop"),StopCalls,1);Test->TestFalse(TEXT("发布中Stop没有伪装成功"),StopInPublish.IsSuccess());
            Test->TestFalse(TEXT("旧Epoch不丢准确主题终态"),S->ThemeRequest.IsValid());Test->TestTrue(TEXT("真实完成记录恢复所有权"),S->bThemeChanged);Test->TestTrue(TEXT("失败Stop保留宿主"),S->Host->Owns(S->Context.Token));
            bStopOnChanged=false;const auto Restore=S->Cleanup();Expected=S->ThemeRestore;bEvent=false;
            Test->TestFalse(TEXT("原主题恢复Pending保持未完成"),Restore.IsSuccess());if(!Expected.IsValid()){Test->AddError(TEXT("恢复没有真实请求身份。"));Finish();return;}Step=2;return;
        }
        if(Step==2)
        {
            Test->TestTrue(TEXT("原主题真实恢复成功"),bSuccess);const auto Done=S->Cleanup();Test->TestTrue(TEXT("终态后Stop重试成功"),Done.IsSuccess());Test->TestFalse(TEXT("原宿主已释放"),S->Host->Owns(S->Context.Token));
            const auto Now=UI()->GetThemeSnapshot();Test->TestEqual(TEXT("原完整Platform恢复"),Now.Context.PlatformId,Original.Context.PlatformId);Test->TestEqual(TEXT("原Skin恢复"),Now.Context.SkinId,Original.Context.SkinId);Test->TestEqual(TEXT("原Quality恢复"),Now.Context.QualityTier,Original.Context.QualityTier);
            FreshState();if(bDone)return;Step=3;Switch();return;
        }
        if(Step==3)
        {Test->TestTrue(TEXT("另一次自身主题真实完成"),bSuccess);Step=4;Expected=UI()->RequestThemeAsync(Original.ThemeId,Original.Context);bEvent=false;return;}
        Test->TestTrue(TEXT("外部真实主题形成新修订"),bSuccess);const auto Before=UI()->GetThemeSnapshot();const auto Conflict=S->Cleanup();
        Test->TestFalse(TEXT("外部新修订阻止旧快照恢复"),Conflict.IsSuccess());Test->TestFalse(TEXT("冲突不得启动恢复"),S->ThemeRestore.IsValid());Test->TestEqual(TEXT("外部修订未被改写"),UI()->GetThemeSnapshot().Revision,Before.Revision);Finish();
    }
};
class FThemeReentryWait final:public IAutomationLatentCommand
{
    TSharedRef<FThemeReentryRun> R;
public:explicit FThemeReentryWait(TSharedRef<FThemeReentryRun> In):R(In){}
    bool Update()override{if(!R->bDone&&FPlatformTime::Seconds()>R->Deadline){R->Test->AddError(TEXT("主题重入真实事件超时；未通过。"));R->Finish();}if(!R->bDone)R->Tick();return R->bDone;}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBALiveThemeStopReentry,"DivineBeasts.Demo.Presentation.LiveThemeStopReentry",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FDBALiveThemeStopReentry::RunTest(const FString&)
{
    auto R=MakeShared<FThemeReentryRun>();R->Test=this;R->S=MakeState(*this,TEXT("GamePlatformUI.theme"));if(!R->S)return false;
    R->Observer.Reset(NewObject<UDivineBeastsDemoPresentationTestObserver>(R->S->Instance()));TWeakPtr<FThemeReentryRun> Weak=R;
    R->Observer->Finished=[Weak](FGuid Id,bool Success){if(auto S=Weak.Pin())if(Id==S->Expected){S->bEvent=true;S->bSuccess=Success;}};
    R->Observer->Changed=[Weak](int64){if(auto S=Weak.Pin())if(S->bStopOnChanged){S->bStopOnChanged=false;++S->StopCalls;S->StopInPublish=S->S->Cleanup();}};
    R->UI()->OnThemeRequestFinished.AddDynamic(R->Observer.Get(),&UDivineBeastsDemoPresentationTestObserver::OnFinished);R->UI()->OnThemeChanged.AddDynamic(R->Observer.Get(),&UDivineBeastsDemoPresentationTestObserver::OnChanged);
    // 专用测试进程显式通过真实唯一服务建立非空基线；人工适配的空初态拒绝规则保持。
    FGamePlatformUIThemeContext Baseline;Baseline.PlatformId=TEXT("Windows");R->Expected=R->UI()->RequestThemeAsync(FPrimaryAssetId(TEXT("GamePlatformDefinition"),TEXT("dba.ui.theme.default@1")),Baseline);R->Deadline=FPlatformTime::Seconds()+120;
    if(!R->Expected.IsValid()){AddError(TEXT("基线主题真实请求未受理。"));R->Finish();return false;}ADD_LATENT_AUTOMATION_COMMAND(FThemeReentryWait(R));return true;
}
#endif
