// 仅在已启动的显式Standalone ManualFixture运行；真正Data/主题异步，不建立服务替身。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/DivineBeastsDemoPresentationLiveUtilities.h"
#include "Scenarios/DivineBeastsDemoPresentationObserver.h"
#include "Scenarios/DivineBeastsDemoManualHostSubsystem.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "HAL/PlatformTime.h"
namespace
{
struct FThemeLiveRun:TSharedFromThis<FThemeLiveRun>
{
    FAutomationTestBase* Test=nullptr;
    TWeakObjectPtr<UGamePlatformUIManagerSubsystem> UI;
    TWeakObjectPtr<UDivineBeastsDemoManualHostSubsystem> Host;
    TStrongObjectPtr<UDivineBeastsDemoPresentationObserver> Observer;
    FGamePlatformUIThemeSnapshot Original;
    FDivineBeastsDemoRunToken Token;
    FGuid Expected;
    int32 Step=0;
    int64 Revision=0;
    bool bEvent=false,bSuccess=false,bDone=false;
    double Deadline=0;
    void Finish()
    {if(UI.IsValid()){if(Expected.IsValid())UI->CancelThemeRequest(Expected);if(Observer.IsValid())UI->OnThemeRequestFinished.RemoveDynamic(Observer.Get(),&UDivineBeastsDemoPresentationObserver::OnThemeFinished);}if(Observer.IsValid())Observer->Clear();Observer.Reset();if(Host.IsValid())Host->Release(Token);bDone=true;}
    void Issue(FPrimaryAssetId Id)
    {bEvent=false;Expected=UI->RequestThemeAsync(Id,Original.Context);if(!Expected.IsValid()){Test->AddError(TEXT("真实主题异步请求未接纳。"));Finish();}}
    void OnEvent(FGuid Id,bool Success)
    {if(Id==Expected){bEvent=true;bSuccess=Success;}}
    void Advance()
    {
        if(bDone||!bEvent)return;bEvent=false;
        if(!UI.IsValid()){Test->AddError(TEXT("真实主题宿主失效。"));Finish();return;}
        const auto Now=UI->GetThemeSnapshot();
        if(Step==-1)
        {if(!bSuccess||!DivineBeasts::Demo::CanBeginPresentationThemeFixture(Now)){Test->AddError(TEXT("真实唯一服务未完成合法非空基线。"));Finish();return;}Original=Now;Step=0;Issue(Original.ThemeId);return;}
        if(Step==0)
        {Test->TestTrue(TEXT("真实主题异步成功"),bSuccess);Revision=Now.Revision;++Step;Issue(FPrimaryAssetId(Original.ThemeId.PrimaryAssetType,TEXT("presentation.manual.missing@1")));return;}
        if(Step==1)
        {Test->TestFalse(TEXT("缺失定义异步失败"),bSuccess);Test->TestEqual(TEXT("失败保留原修订"),Now.Revision,Revision);++Step;Issue(Original.ThemeId);if(!bDone)UI->CancelThemeRequest(Expected);return;}
        if(Step==2)
        {Test->TestFalse(TEXT("真实取消以失败终态通知"),bSuccess);Test->TestEqual(TEXT("取消不切主题"),Now.Revision,Revision);++Step;Issue(Original.ThemeId);return;}
        if(Step==3)
        {
            Test->TestTrue(TEXT("另一次真实切换成功形成外部新修订"),bSuccess);FGuid Request;
            Test->TestFalse(TEXT("旧预期修订恢复拒绝"),UI->RestoreThemeSnapshot(Original,Revision,Request).IsSuccess());Revision=Now.Revision;
            auto Missing=Original;Missing.ThemeId=FPrimaryAssetId(Original.ThemeId.PrimaryAssetType,TEXT("presentation.manual.missing@1"));++Step;
            const auto R=UI->RestoreThemeSnapshot(Missing,Revision,Expected);if(!R.IsSuccess()||!Expected.IsValid()){Test->AddError(TEXT("恢复失败路径未真实受理。"));Finish();}return;
        }
        if(Step==4)
        {Test->TestFalse(TEXT("恢复缺定义真实失败，保留重试资格"),bSuccess);Test->TestEqual(TEXT("失败恢复不改当前修订"),Now.Revision,Revision);++Step;const auto R=UI->RestoreThemeSnapshot(Original,Revision,Expected);if(!R.IsSuccess()||!Expected.IsValid()){Test->AddError(TEXT("恢复重试未被接纳。"));Finish();}return;}
        Test->TestTrue(TEXT("恢复重试实际异步成功"),bSuccess);Test->TestEqual(TEXT("原主题身份恢复"),Now.ThemeId,Original.ThemeId);
        Test->TestEqual(TEXT("原Platform恢复"),Now.Context.PlatformId,Original.Context.PlatformId);Test->TestEqual(TEXT("原Skin恢复"),Now.Context.SkinId,Original.Context.SkinId);Test->TestEqual(TEXT("原Quality恢复"),Now.Context.QualityTier,Original.Context.QualityTier);Finish();
    }
};
class FThemeLiveWait final:public IAutomationLatentCommand
{
    TSharedRef<FThemeLiveRun> Run;
public:
    explicit FThemeLiveWait(TSharedRef<FThemeLiveRun> R):Run(R){}
    bool Update()override
    {if(Run->bDone)return true;if(FPlatformTime::Seconds()>Run->Deadline){Run->Test->AddError(TEXT("主题真实终态等待超时；状态/原快照须人工恢复，不能报通过。"));Run->Finish();return true;}Run->Advance();return Run->bDone;}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBAThemeLiveLifecycleTest,"DivineBeasts.Demo.Presentation.LiveThemeLifecycle",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FDBAThemeLiveLifecycleTest::RunTest(const FString&)
{
    UWorld* W=nullptr;ULocalPlayer* Player=nullptr;
    if(!DivineBeasts::Demo::Tests::FindStandalone(*this,W,Player))return false;
    auto R=MakeShared<FThemeLiveRun>();R->Test=this;R->UI=Player->GetSubsystem<UGamePlatformUIManagerSubsystem>();R->Host=W->GetGameInstance()->GetSubsystem<UDivineBeastsDemoManualHostSubsystem>();
    if(!R->UI.IsValid()||!R->Host.IsValid()){AddError(TEXT("真实主题/人工宿主不可用。"));return false;}
    R->Original=R->UI->GetThemeSnapshot();if(R->Original.bRequestPending){AddError(TEXT("空主题初态或外部在途请求禁止正向修改。"));return false;}
    R->Token.RunId=FGuid::NewGuid();R->Token.WorldId=FGuid::NewGuid();R->Token.Generation=1;
    if(!R->Host->Acquire(R->Token,*W).IsSuccess()){AddError(TEXT("未获排他ManualHost。"));return false;}
    R->Observer.Reset(NewObject<UDivineBeastsDemoPresentationObserver>(W->GetGameInstance()));TWeakPtr<FThemeLiveRun> Weak=R;
    R->Observer->ThemeFinished=[Weak](FGuid Id,bool Success,FText){if(auto S=Weak.Pin())S->OnEvent(Id,Success);};
    R->UI->OnThemeRequestFinished.AddDynamic(R->Observer.Get(),&UDivineBeastsDemoPresentationObserver::OnThemeFinished);
    R->Deadline=FPlatformTime::Seconds()+90;R->Step=-1;R->Original.Context.PlatformId=TEXT("Windows");R->Issue(FPrimaryAssetId(TEXT("GamePlatformDefinition"),TEXT("dba.ui.theme.default@1")));ADD_LATENT_AUTOMATION_COMMAND(FThemeLiveWait(R));return true;
}
#endif
