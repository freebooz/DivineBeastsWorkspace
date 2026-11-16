// 只协调本运行精确终态；同步开屏回调暂存至返回身份已知，清理不依赖池中UObject销毁。
#include "Scenarios/DivineBeastsDemoPresentationManualState.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Definitions/GamePlatformUIScreenDefinition.h"
#include "Layers/GamePlatformUILayerStack.h"
#include "Screens/GamePlatformUIScreen.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
namespace DivineBeasts::Demo
{
void FPresentationManualState::ReconcileOwnedScreen()
{
    auto* M=Local<UGamePlatformUIManagerSubsystem>();
    if(Screen.IsValid()&&M&&!M->IsScreenOwnedByStack(Screen.Get(),ScreenStack.Get()))Screen.Reset();
    if(!Screen.IsValid())ScreenStack.Reset();
}
FGamePlatformResult FPresentationManualState::CloseOwnedScreen()
{
    if(!Screen.IsValid()){ScreenStack.Reset();return FGamePlatformResult::Success();}
    auto* M=Local<UGamePlatformUIManagerSubsystem>();
    if(!M)return PresentationError(TEXT("Demo.UI.CloseService"),TEXT("原UIManager不可用，保留精确实例。"));
    ReconcileOwnedScreen();if(!Screen.IsValid())return FGamePlatformResult::Success();
    const auto Instance=Screen;const auto Stack=ScreenStack;
    const bool Closed=M->CloseScreen(Instance.Get());
    if(Closed||!M->IsScreenOwnedByStack(Instance.Get(),Stack.Get()))
    {if(Screen==Instance){Screen.Reset();ScreenStack.Reset();}return FGamePlatformResult::Success();}
    return PresentationError(TEXT("Demo.UI.Close"),TEXT("本实例仍由原栈拥有且关闭被拒绝，保留身份供重试。"));
}
void FPresentationManualState::OnScreenTerminal(const FScreenTerminal& T)
{
    if(!Alive()||T.ScreenId!=RegisteredScreen)return;
    if(bSubmittingOpen)
    {if(SynchronousOpenTerminals.Num()<32)SynchronousOpenTerminals.Add(T);else bOpenBufferOverflow=true;return;}
    if(!T.Request.IsValid()||T.Request!=Opening)return;
    Opening.Invalidate();
    if(T.bSuccess){Screen=T.Instance;ReconcileOwnedScreen();Observe(Screen.IsValid()?TEXT("本请求真实开屏成功，精确实例已入栈。"):TEXT("本请求开屏回调时已被同步关闭，不持有池中实例。"));}
    else Observe(TEXT("本请求真实开屏失败：")+T.Reason.ToString());
}
FGamePlatformResult FPresentationManualState::OpenOwnedScreen(UGamePlatformUIScreenDefinition& D)
{
    auto* M=Local<UGamePlatformUIManagerSubsystem>();
    if(!Alive()||!M||Opening.IsValid()||bSubmittingOpen||D.bSurvivesTravel)
        return PresentationError(TEXT("Demo.UI.OpenUnavailable"),TEXT("来源失活、在途打开或跨Travel定义不可接管。"));
    ReconcileOwnedScreen();if(Screen.IsValid())return PresentationError(TEXT("Demo.UI.ScreenAlreadyOwned"),TEXT("本实例尚未关闭。"));
    if(RegisteredScreen.IsNone())
    {
        if(M->HasScreenDefinition(D.ScreenId)||!M->RegisterScreenDefinition(&D))return PresentationError(TEXT("Demo.UI.Register"),TEXT("拒绝覆盖已有定义，或真实注册失败。"));
        RegisteredScreen=D.ScreenId;RegisteredDefinition=&D;
    }
    else if(RegisteredScreen!=D.ScreenId||RegisteredDefinition.Get()!=&D)
        return PresentationError(TEXT("Demo.UI.DefinitionOwned"),TEXT("本运行只可重试自己登记的精确定义。"));
    const TStrongObjectPtr<UGamePlatformUIManagerSubsystem> Keep(M);
    ScreenStack=M->GetRootLayout()?M->GetRootLayout()->GetActivatableStack(D.Layer):nullptr;
    SynchronousOpenTerminals.Reset();bOpenBufferOverflow=false;bSubmittingOpen=true;const uint64 E=Epoch;
    const auto Request=M->OpenScreenAsync(D.ScreenId,nullptr);
    bSubmittingOpen=false;auto Terminals=MoveTemp(SynchronousOpenTerminals);
    if(!Current(E))
    {if(Request.IsValid())M->CancelOpen(Request.RequestId);return FGamePlatformResult::Cancelled(TEXT("开屏同步通知期间来源换代。"));}
    Opening=Request.RequestId;
    if(bOpenBufferOverflow){if(Request.IsValid())M->CancelOpen(Request.RequestId);Opening.Invalidate();return PresentationError(TEXT("Demo.UI.OpenTerminalOverflow"),TEXT("同步终态通知超过本调用安全上限，拒绝推断受理成功。"));}
    for(const auto& T:Terminals)if(T.Request==Request.RequestId&&T.ScreenId==D.ScreenId)
    {
        OnScreenTerminal(T);
        if(!T.bSuccess){auto R=PresentationError(TEXT("Demo.UI.OpenRejected"),TEXT("真实UIManager已同步拒绝开屏。"));R.Message=T.Reason.ToString();return R;}
        return FGamePlatformResult::Success();
    }
    return Request.IsValid()?FGamePlatformResult::Success():PresentationError(TEXT("Demo.UI.OpenRejected"),TEXT("真实开屏请求未受理。"));
}
void FPresentationManualState::OnOwnedThemeTerminal(FGuid Request,bool bSuccess)
{
    // Stop只撤执行资格；同LP/World服务、同Scope、仍持本宿主的准确终态必须继续收敛。
    auto* M=Local<UGamePlatformUIManagerSubsystem>();
    if(!Request.IsValid()||!bHost||!Host.IsValid()||!Host->Owns(Context.Token)||!World.IsValid()||World->bIsTearingDown||!Player.IsValid()||Player->GetWorld()!=World.Get()||!M||ThemeSource.Get()!=M)return;
    const auto Snapshot=M->GetThemeSnapshot();if(Snapshot.ScopeId!=OriginalTheme.ScopeId)return;
    if(Request==ThemeRestore)
    {
        ThemeRestore.Invalidate();
        if(bSuccess&&Snapshot.ThemeId==OriginalTheme.ThemeId&&Snapshot.Context.PlatformId==OriginalTheme.Context.PlatformId&&
            Snapshot.Context.SkinId==OriginalTheme.Context.SkinId&&Snapshot.Context.QualityTier==OriginalTheme.Context.QualityTier)bThemeChanged=false;
        if(Context.PublishObservation)Context.PublishObservation(Context.Token,!bThemeChanged?TEXT("原主题真实恢复完成；可重试Stop释放宿主。"):TEXT("原主题恢复失败/外部状态变化；保留原快照和宿主。"));
        return;
    }
    if(Request!=ThemeRequest)return;
    ThemeRequest.Invalidate();
    if(bSuccess)
    {
        // 记录本请求应完成的单次修订；外部更晚修订保留冲突，恢复API将拒绝覆盖。
        ++ThemeExpectedRevision;bThemeChanged=true;
        const bool Exact=Snapshot.Revision==ThemeExpectedRevision&&Snapshot.ThemeId==RequestedTheme&&
            Snapshot.Context.PlatformId==RequestedThemeContext.PlatformId&&Snapshot.Context.SkinId==RequestedThemeContext.SkinId&&Snapshot.Context.QualityTier==RequestedThemeContext.QualityTier;
        if(Context.PublishObservation)Context.PublishObservation(Context.Token,Exact?TEXT("本主题准确成功终态已记录；执行失活后也保留恢复资格。"):TEXT("本主题成功后外部状态已变化；恢复将拒绝覆盖。"));
    }
    else if(Context.PublishObservation)Context.PublishObservation(Context.Token,TEXT("本主题准确失败/取消终态已收敛，原主题保持。"));
}
}
