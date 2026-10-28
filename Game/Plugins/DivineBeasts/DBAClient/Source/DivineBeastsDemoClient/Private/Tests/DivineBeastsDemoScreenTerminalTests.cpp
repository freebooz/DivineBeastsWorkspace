#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/DivineBeastsDemoPresentationLiveUtilities.h"
#include "Tests/DivineBeastsDemoPresentationReentryFixture.h"
#include "Definitions/GamePlatformUIScreenDefinition.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "HAL/PlatformTime.h"
#include "Services/GamePlatformWorldUIService.h"
namespace
{
using namespace DivineBeasts::Demo;using namespace DivineBeasts::Demo::Tests;
struct FScreenLiveRun:TSharedFromThis<FScreenLiveRun>
{
    FAutomationTestBase* Test=nullptr;TSharedPtr<FPresentationManualState> S;
    TStrongObjectPtr<UGamePlatformUIScreenDefinition> Definition,OtherDefinition;
    TStrongObjectPtr<UDivineBeastsDemoPresentationTestObserver> Observer;
    TStrongObjectPtr<UGamePlatformUIScreen> Retained,Other;
    TSubclassOf<UGamePlatformUILayerStack> RootClass;TStrongObjectPtr<UClass> RootClassKeep;FGuid OtherRequest;
    FGamePlatformDataLease Lease;FDelegateHandle RootChanged;int32 Step=0;bool bLoaded=false,bRootReady=false,bDone=false;double Deadline=0;
    void Finish()
    {
        if(auto* UI=S->Local<UGamePlatformUIManagerSubsystem>())
        {if(auto* W=UI->GetWorldUIService())W->OnRootLayoutChanged().Remove(RootChanged);if(Observer.IsValid())UI->OnScreenOpened.RemoveDynamic(Observer.Get(),&UDivineBeastsDemoPresentationTestObserver::OnOpened);if(Other.IsValid())UI->CloseScreen(Other.Get());if(OtherDefinition.IsValid())UI->UnregisterScreenDefinition(OtherDefinition->ScreenId);if(RootClass)UI->InstallRootLayoutClass(RootClass);}
        S->Cleanup();if(Observer.IsValid())Observer->Clear();Observer.Reset();bDone=true;
    }
    void Tick()
    {
        if(!bLoaded||!bRootReady||bDone)return;auto* UI=S->Local<UGamePlatformUIManagerSubsystem>();if(!UI){Test->AddError(TEXT("真实UIManager失效。"));Finish();return;}
        if(Step==0)
        {
            Lease=S->AssetLease;RootClass=UI->GetRootLayout()?UI->GetRootLayout()->GetClass():nullptr;
            if(RootClass)RootClassKeep.Reset(RootClass.Get());
            if(!RootClass||!UI->InstallRootLayoutClass(UDivineBeastsDemoPresentationEmptyRoot::StaticClass())){Test->AddError(TEXT("不能安装本测试空层夹具。"));Finish();return;}
            Definition.Reset(NewObject<UGamePlatformUIScreenDefinition>());Definition->ScreenId=FName(*S->Context.Token.RunId.ToString());Definition->WidgetClass=UDivineBeastsDemoPresentationTestScreen::StaticClass();Definition->InputMode=EGamePlatformUIInputMode::GameOnly;Definition->Layer=EGamePlatformUILayer::Screen;
            int32 ObservedFailures=0;S->Context.PublishObservation=[&ObservedFailures](const auto&,const FString& T){if(T.Contains(TEXT("失败")))++ObservedFailures;};
            const auto Failed=S->OpenOwnedScreen(*Definition);S->Context.PublishObservation={};
            Test->TestFalse(TEXT("真实缺层同步失败不由GUID伪报成功"),Failed.IsSuccess());Test->TestFalse(TEXT("没有已终态Opening残留"),S->Opening.IsValid());Test->TestEqual(TEXT("精确同步失败已观察一次"),ObservedFailures,1);
            if(!UI->InstallRootLayoutClass(RootClass)){Test->AddError(TEXT("不能恢复本测试Root。"));Finish();return;}
            auto* TestStack=UI->GetRootLayout()?UI->GetRootLayout()->GetActivatableStack(Definition->Layer):nullptr;
            if(!TestStack){Test->AddError(TEXT("本测试恢复的Screen栈缺失。"));Finish();return;}
            // 仅本次安装的测试Root实例使用零时长；须在任何正向测试页面入栈前设置。
            TestStack->SetTransitionDuration(0.0f);
            const auto Retried=S->OpenOwnedScreen(*Definition);
            if(!Retried.IsSuccess()){Test->AddError(TEXT("修复真实Root后同定义重试未受理。"));Finish();return;}Step=1;return;
        }
        if(Step==1&&S->Screen.IsValid())
        {
            if(!Test->TestTrue(TEXT("原页面入栈后已激活"),S->Screen->IsActivated())){Finish();return;}
            Retained.Reset(S->Screen.Get());OtherDefinition.Reset(NewObject<UGamePlatformUIScreenDefinition>());OtherDefinition->ScreenId=FName(*(S->Context.Token.RunId.ToString()+TEXT(".Other")));OtherDefinition->WidgetClass=UDivineBeastsDemoPresentationTestScreen::StaticClass();OtherDefinition->InputMode=EGamePlatformUIInputMode::GameOnly;
            UI->RegisterScreenDefinition(OtherDefinition.Get());OtherRequest=UI->OpenScreenAsync(OtherDefinition->ScreenId,nullptr).RequestId;Step=2;return;
        }
        if(Step==2&&Other.IsValid())
        {
            if(!Test->TestTrue(TEXT("不相关页面关闭前已激活"),Other->IsActivated())){Finish();return;}
            UI->CloseScreen(Other.Get());Test->TestEqual(TEXT("不相关真实ScreenId关闭不清本实例"),S->Screen.Get(),Retained.Get());UI->UnregisterScreenDefinition(OtherDefinition->ScreenId);OtherDefinition.Reset();Other.Reset();
            if(!Test->TestTrue(TEXT("原页面Native Close前已重新激活"),Retained->IsActivated())){Finish();return;}
            Retained->CloseScreen();Step=3;return;
        }
        if(Step==3&&!S->Screen.IsValid())
        {
            Test->TestTrue(TEXT("原生Close后池中/强持Widget仍有效"),Retained.IsValid());
            const auto OpenedAgain=S->OpenOwnedScreen(*Definition);if(!OpenedAgain.IsSuccess()){Test->AddError(TEXT("原生Close后不能重开本定义。"));Finish();return;}Step=4;return;
        }
        if(Step==4&&S->Screen.IsValid())
        {
            Retained.Reset(S->Screen.Get());
            if(!Test->TestTrue(TEXT("原页面Native Back前已激活"),Retained->IsActivated())){Finish();return;}
            CastChecked<UDivineBeastsDemoPresentationTestScreen>(Retained.Get())->CloseByNativeBack();Step=5;return;
        }
        if(Step==5&&!S->Screen.IsValid())
        {
            Test->TestTrue(TEXT("原生Back后对象仍有效但所有权已撤"),Retained.IsValid());const auto Cleanup=S->Cleanup();Test->TestTrue(TEXT("Stop不被池中弱指针永久阻塞"),Cleanup.IsSuccess());
            Test->TestFalse(TEXT("本定义已注销"),UI->HasScreenDefinition(Definition->ScreenId));Test->TestFalse(TEXT("真实ManualHost已释放"),S->Host->Owns(S->Context.Token));
            auto* D=IGamePlatformDataService::Get(*S->Instance());Test->TestTrue(TEXT("本资源租约实际Released"),D&&D->GetLeaseState(Lease)==EGamePlatformDataRequestState::Released);
            auto Token=S->Context.Token;Token.RunId=FGuid::NewGuid();++Token.Generation;Test->TestTrue(TEXT("后续运行可重新获得真实宿主"),S->Host->Acquire(Token,*S->World).IsSuccess());S->Host->Release(Token);Finish();
        }
    }
};
class FScreenLiveWait final:public IAutomationLatentCommand
{
    TSharedRef<FScreenLiveRun> R;
public:explicit FScreenLiveWait(TSharedRef<FScreenLiveRun> In):R(In){}
    bool Update()override{if(!R->bDone&&FPlatformTime::Seconds()>R->Deadline){R->Test->AddError(TEXT("屏幕真实事件超时，未通过。"));R->Finish();}if(!R->bDone)R->Tick();return R->bDone;}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBALiveScreenTerminals,"DivineBeasts.Demo.Presentation.LiveScreenTerminals",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FDBALiveScreenTerminals::RunTest(const FString&)
{
    auto R=MakeShared<FScreenLiveRun>();R->Test=this;R->S=MakeState(*this,TEXT("GamePlatformUI.screens"));if(!R->S)return false;
    R->Observer.Reset(NewObject<UDivineBeastsDemoPresentationTestObserver>(R->S->Instance()));TWeakPtr<FScreenLiveRun> Weak=R;
    R->Observer->Opened=[Weak](FGuid Id,FName,UGamePlatformUIScreen* W){if(auto S=Weak.Pin())if(Id==S->OtherRequest)S->Other.Reset(W);};
    R->S->Local<UGamePlatformUIManagerSubsystem>()->OnScreenOpened.AddDynamic(R->Observer.Get(),&UDivineBeastsDemoPresentationTestObserver::OnOpened);
    auto* UI=R->S->Local<UGamePlatformUIManagerSubsystem>();R->bRootReady=UI->GetRootLayout()!=nullptr;if(auto* W=UI->GetWorldUIService())R->RootChanged=W->OnRootLayoutChanged().AddLambda([Weak]{if(auto X=Weak.Pin())X->bRootReady=true;});
    R->Deadline=FPlatformTime::Seconds()+90;R->S->Load(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")),[](UObject*){return FGamePlatformResult::Success();},[Weak](const auto&,const auto& Result){if(auto S=Weak.Pin()){if(!Result.IsSuccess()){S->Test->AddError(TEXT("真实Data Cube租约失败。"));S->Finish();}else S->bLoaded=true;}});
    ADD_LATENT_AUTOMATION_COMMAND(FScreenLiveWait(R));return true;
}
#endif
