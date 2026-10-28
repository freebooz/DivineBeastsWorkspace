// T06开发人工页：项目私有Editor适配或平台中立租约；仅游戏线程，事件驱动。
// 只拥有本Run对象/完整租约，停止失败保留身份供重试；不操作用户图、账号或生产服务器。
#include "Scenarios/DivineBeastsDemoPresentationManualState.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Interfaces/GamePlatformVFXService.h"
#include "Interfaces/IGamePlatformSFXService.h"
#include "Interfaces/IGamePlatformSFXUserSettingsService.h"
#include "Interfaces/IGamePlatformSurfaceService.h"
#include "Marks/IGamePlatformSurfaceMarkService.h"
#include "Cinematic/GamePlatformCinematicCameraSubsystem.h"
#include "Feedback/GamePlatformCameraHitFeedbackSubsystem.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Adaptive/GamePlatformUIAdaptiveSubsystem.h"
#include "Services/GamePlatformNotificationService.h"
#include "Services/GamePlatformFeedbackService.h"
#include "Services/GamePlatformWorldUIService.h"
#include "Gameplay/DivineBeastsWorldPlayerController.h"
#include "Screens/GamePlatformUIScreen.h"
#include "Preview/GamePlatformCharacterPreviewStage.h"
#include "Components/GamePlatformMeshDissolveComponent.h"
#include "Components/GamePlatformSnowFootContactComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
namespace DivineBeasts::Demo
{
bool FPresentationManualState::Alive()const
{return bActive&&!bCleaning&&World.IsValid()&&!World->bIsTearingDown&&Player.IsValid()&&Player->GetWorld()==World.Get()&&(Feature!=TEXT("GamePlatformSurface.editor_wind")||(Host.IsValid()&&Host->OwnsEditorFixture(Context.Token)));}
UGameInstance* FPresentationManualState::Instance()const{return World.IsValid()?World->GetGameInstance():nullptr;}
IGamePlatformDataService* FPresentationManualState::Data()const{return Instance()?IGamePlatformDataService::Get(*Instance()):nullptr;}
void FPresentationManualState::Observe(FString Text)const
{if(Alive()&&Context.PublishObservation)Context.PublishObservation(Context.Token,Text.Left(4096));}
FGamePlatformResult FPresentationManualState::Prepare()
{
    auto* Widget=Cast<UUserWidget>(Context.SampleWidget.Get());
    Player=Widget?Widget->GetOwningLocalPlayer():nullptr;
    if(!World.IsValid()||World->bIsTearingDown||!Player.IsValid()||Player->GetWorld()!=World.Get()||!Data())
        return PresentationError(TEXT("Demo.Presentation.Source"),TEXT("需要同World/LocalPlayer的真实SampleWidget及中央Data服务。"));
    Host=Instance()->GetSubsystem<UDivineBeastsDemoManualHostSubsystem>();
    if(!Host.IsValid())return PresentationError(TEXT("Demo.Presentation.HostMissing"),TEXT("先应用T04 ManualHost；不初始化第二份平台子系统。"));
    const auto Acquired=Feature==TEXT("GamePlatformSurface.editor_wind")?Host->AcquireEditorFixture(Context.Token,*World,Feature):Host->Acquire(Context.Token,*World);if(!Acquired.IsSuccess())return Acquired;bHost=true;
    if(Feature.ToString().StartsWith(TEXT("GamePlatformVFX."))&&!IGamePlatformVFXService::Get(World.Get()))return PresentationError(TEXT("Demo.VFX.Missing"),TEXT("本World实际VFX服务不可用。"));
    if(Feature.ToString().StartsWith(TEXT("GamePlatformSFX."))&&!IGamePlatformSFXService::Get(World.Get()))return PresentationError(TEXT("Demo.SFX.Missing"),TEXT("本World实际SFX服务不可用。"));
    if(Feature.ToString().StartsWith(TEXT("GamePlatformPresentation."))&&!Local<UGamePlatformPresentationClientSubsystem>())return PresentationError(TEXT("Demo.Presentation.Missing"),TEXT("同LP中立表现服务不可用。"));
    if(Feature.ToString().StartsWith(TEXT("GamePlatformUI."))&&!Local<UGamePlatformUIManagerSubsystem>())return PresentationError(TEXT("Demo.UI.Missing"),TEXT("同LP实际UIManager不可用。"));
    if(Feature==TEXT("GamePlatformUI.world_projection"))
    {CameraController=Cast<ADivineBeastsWorldPlayerController>(Player->GetPlayerController(World.Get()));if(!CameraController.IsValid())return PresentationError(TEXT("Demo.UI.FinalCameraSourceMissing"),TEXT("世界投影需同LP已公开最终镜头通知的WorldPlayerController；不靠输入或Tick猜镜头。"));}
    bActive=true;BindEvents();
    if(!Alive())return PresentationError(TEXT("Demo.Presentation.Reentered"),TEXT("准备通知期间来源已失活。"));
    if(Feature==TEXT("GamePlatformSFX.user_volume"))
    {auto* S=IGamePlatformSFXUserSettingsService::Get(*Instance());if(!S)return PresentationError(TEXT("Demo.SFX.SettingsMissing"),TEXT("真实用户音量服务不可用。"));OriginalVolumes=S->GetSnapshot();bVolumesCaptured=true;Observe(FString::Printf(TEXT("首次音量快照 Effects=%.2f Ambient=%.2f Music=%.2f Revision=%lld"),OriginalVolumes.Effects,OriginalVolumes.Ambient,OriginalVolumes.Music,OriginalVolumes.Revision));}
    if(Feature==TEXT("GamePlatformSurface.environment"))
    {auto* S=IGamePlatformSurfaceService::Get(*World);if(!S)return PresentationError(TEXT("Demo.Surface.Missing"),TEXT("当前世界不提供Surface服务。"));OriginalSurface=S->GetEnvironmentState();LastSurface=OriginalSurface;bSurfaceCaptured=true;}
    if(Feature==TEXT("GamePlatformCamera.hit_shake"))
    {auto* S=Local<UGamePlatformCameraHitFeedbackSubsystem>();if(!S)return PresentationError(TEXT("Demo.Camera.Missing"),TEXT("当前本地玩家镜头反馈服务不可用。"));OriginalShakeScale=S->GetUserCameraShakeScale();bShakeScaleCaptured=true;}
    if(Feature==TEXT("GamePlatformUI.theme"))
    {auto* S=Local<UGamePlatformUIManagerSubsystem>();if(!S)return PresentationError(TEXT("Demo.UI.ThemeMissing"),TEXT("同LP主题服务不可用。"));ThemeSource=S;OriginalTheme=S->GetThemeSnapshot();ThemeExpectedRevision=OriginalTheme.Revision;
        if(!CanBeginPresentationThemeFixture(OriginalTheme))return PresentationError(TEXT("Demo.UI.ThemeBaselineRequired"),TEXT("先由正式主题装配完成合法非空基线且无在途请求；空主题原控件样式缺恢复合同，不允许修改。"));}
    if(Feature==TEXT("GamePlatformUI.adaptive"))if(auto* S=Local<UGamePlatformUIAdaptiveSubsystem>()){const auto C=S->GetAdaptiveContext();Observe(FString::Printf(TEXT("首次适配快照 Viewport=%.0fx%.0f DPI=%.3f；后续只收真实输入/视口通知。"),C.ViewportSize.X,C.ViewportSize.Y,C.DPIScale));}
    Observe(TEXT("已绑定真实世界和排他人工宿主；命令受理、服务终态与人工画面/听觉分别记录。"));
    return FGamePlatformResult::Success();
}
void FPresentationManualState::BindEvents()
{
    const TWeakPtr<FPresentationManualState> Weak=AsShared();const uint64 E=Epoch;
    WorldCleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Weak](UWorld* W,bool,bool)
    {if(auto S=Weak.Pin())if(S->World.Get()==W){S->bActive=false;++S->Epoch;S->Cleanup();}});
    if(auto* V=IGamePlatformVFXService::Get(World.Get()))VFXCompleted=V->AddCompletionHandler(FGamePlatformVFXPlaybackCompleted::FDelegate::CreateLambda([Weak,E](const auto& Snapshot)
    {if(auto S=Weak.Pin())if(S->Current(E)&&S->VFXHandles.Contains(Snapshot.Handle))S->Observe(FString::Printf(TEXT("VFX终态：Handle=%s Generation=%d State=%d Definition=%s；GPU可见/预算未验收。"),*Snapshot.Handle.Id.ToString(),Snapshot.Handle.Generation,int32(Snapshot.State),*Snapshot.DefinitionId.ToString()));}));
    if(auto* V=IGamePlatformSFXService::Get(World.Get()))SFXCompleted=V->AddCompletionHandler(FGamePlatformSFXPlaybackCompleted::FDelegate::CreateLambda([Weak,E](const auto& Snapshot)
    {if(auto S=Weak.Pin())if(S->Current(E)&&S->SFXHandles.ContainsByPredicate([&](const auto& H){return PresentationManualSameSFXHandle(H,Snapshot.Handle);}))S->Observe(FString::Printf(TEXT("SFX终态：Handle=%s Generation=%d State=%d；仍需实际扬声器听觉验收。"),*Snapshot.Handle.Id.ToString(),Snapshot.Handle.Generation,int32(Snapshot.State)));}));
    if(auto* S=IGamePlatformSFXUserSettingsService::Get(*Instance()))VolumesChanged=S->AddChangedHandler(FGamePlatformSFXUserVolumesChanged::FDelegate::CreateLambda([Weak,E](const auto& V)
    {if(auto S=Weak.Pin())if(S->Current(E)&&S->Feature==TEXT("GamePlatformSFX.user_volume"))S->Observe(FString::Printf(TEXT("用户混音事件 Revision=%lld Effects=%.2f Ambient=%.2f Music=%.2f"),V.Revision,V.Effects,V.Ambient,V.Music));}));
    if(auto* S=IGamePlatformSurfaceService::Get(*World))SurfaceChanged=S->AddStateChangedHandler(FGamePlatformSurfaceStateChanged::FDelegate::CreateLambda([Weak,E](const auto& V,int32 R)
    {if(auto S=Weak.Pin())if(S->Current(E)&&S->Feature==TEXT("GamePlatformSurface.environment"))S->Observe(FString::Printf(TEXT("表面状态事件 Revision=%d Wet=%.2f Snow=%.2f；接受不等于MPC/GPU写入完成。"),R,V.GlobalWetness,V.GlobalSnowAmount));}));
    if(auto* M=IGamePlatformSurfaceMarkService::Get(*World))MarkSpawned=M->AddSpawnedHandler(FGamePlatformSurfaceMarkSpawned::FDelegate::CreateLambda([Weak,E](const auto& R)
    {if(auto S=Weak.Pin())if(S->Current(E)&&S->MarkRequests.Contains(R.EventId))S->Observe(FString::Printf(TEXT("本痕迹真实Spawned事件 Id=%s；有限寿命由原生服务接管，停止观察不冒充立即擦除。"),*R.EventId.ToString()));}));
    if(auto* S=Local<UGamePlatformPresentationClientSubsystem>())RequestObserved=S->OnRequestObserved().AddLambda([Weak,E](const auto& R,auto Result)
    {if(auto S=Weak.Pin())if(S->Current(E)&&R.RequestId==S->LastPresentation.RequestId)S->Observe(FString::Printf(TEXT("中立请求事件 Request=%s WorldGeneration=%d Submit=%d；播放器终态独立观察。"),*R.RequestId.ToString(),R.WorldGeneration,int32(Result)));});
    Observer.Reset(NewObject<UDivineBeastsDemoPresentationObserver>(Instance()));
    Observer->Opened=[Weak](FGuid Request,FName Id,UGamePlatformUIScreen* Screen)
    {if(auto S=Weak.Pin())S->OnScreenTerminal({Request,Id,true,Screen,{}});};
    Observer->OpenFailed=[Weak](FGuid Request,FName Id,FText Reason)
    {if(auto S=Weak.Pin())S->OnScreenTerminal({Request,Id,false,nullptr,Reason});};
    Observer->Closed=[Weak](FName Id)
    {if(auto S=Weak.Pin())if(Id==S->RegisteredScreen){S->ReconcileOwnedScreen();S->Observe(TEXT("本次ScreenId真实关闭通知，精确栈所有权已收敛。"));}};
    // 清理期仍只接纳同来源、完整请求身份的终态；不恢复普通Execute资格。
    Observer->ThemeFinished=[Weak](FGuid Request,bool Success,FText)
    {if(auto S=Weak.Pin())S->OnOwnedThemeTerminal(Request,Success);};
    Observer->Adaptive=[Weak,E](auto Value){if(auto S=Weak.Pin())if(S->Current(E)&&S->Feature==TEXT("GamePlatformUI.adaptive"))S->Observe(FString::Printf(TEXT("自适应变化事件：Viewport=%.0fx%.0f DPI=%.3f；固定字号由UI合同管理。"),Value.ViewportSize.X,Value.ViewportSize.Y,Value.DPIScale));};
    if(auto* S=Local<UGamePlatformUIManagerSubsystem>())
    {S->OnScreenOpened.AddDynamic(Observer.Get(),&UDivineBeastsDemoPresentationObserver::OnOpened);S->OnScreenOpenFailed.AddDynamic(Observer.Get(),&UDivineBeastsDemoPresentationObserver::OnOpenFailed);S->OnScreenClosed.AddDynamic(Observer.Get(),&UDivineBeastsDemoPresentationObserver::OnClosed);S->OnThemeRequestFinished.AddDynamic(Observer.Get(),&UDivineBeastsDemoPresentationObserver::OnThemeFinished);}
    if(auto* S=Local<UGamePlatformUIAdaptiveSubsystem>())S->OnAdaptiveContextChanged.AddDynamic(Observer.Get(),&UDivineBeastsDemoPresentationObserver::OnAdaptive);
    if(auto* C=CameraController.Get())FinalCameraChanged=C->OnFinalCameraViewChanged.AddLambda([Weak,E]
    {if(auto S=Weak.Pin())if(S->Current(E)&&S->WorldUI.IsValid()&&S->Player->GetPlayerController(S->World.Get())==S->CameraController.Get())if(auto* UI=S->Local<UGamePlatformUIManagerSubsystem>())if(auto* W=UI->GetWorldUIService())W->RefreshProjection(S->WorldUI);});
}
void FPresentationManualState::UnbindEvents()
{
    FWorldDelegates::OnWorldCleanup.Remove(WorldCleanup);WorldCleanup.Reset();
    if(auto* C=CameraController.Get())C->OnFinalCameraViewChanged.Remove(FinalCameraChanged);FinalCameraChanged.Reset();CameraController.Reset();
    if(World.IsValid())
    {if(auto* S=IGamePlatformVFXService::Get(World.Get()))S->RemoveCompletionHandler(VFXCompleted);if(auto* S=IGamePlatformSFXService::Get(World.Get()))S->RemoveCompletionHandler(SFXCompleted);if(auto* S=IGamePlatformSurfaceService::Get(*World))S->RemoveStateChangedHandler(SurfaceChanged);if(auto* S=IGamePlatformSurfaceMarkService::Get(*World))S->RemoveSpawnedHandler(MarkSpawned);}
    if(Instance())if(auto* S=IGamePlatformSFXUserSettingsService::Get(*Instance()))S->RemoveChangedHandler(VolumesChanged);
    if(auto* S=Local<UGamePlatformPresentationClientSubsystem>())S->OnRequestObserved().Remove(RequestObserved);
    if(Observer.IsValid())
    {Observer->Clear();if(auto* S=Local<UGamePlatformUIManagerSubsystem>()){S->OnScreenOpened.RemoveDynamic(Observer.Get(),&UDivineBeastsDemoPresentationObserver::OnOpened);S->OnScreenOpenFailed.RemoveDynamic(Observer.Get(),&UDivineBeastsDemoPresentationObserver::OnOpenFailed);S->OnScreenClosed.RemoveDynamic(Observer.Get(),&UDivineBeastsDemoPresentationObserver::OnClosed);S->OnThemeRequestFinished.RemoveDynamic(Observer.Get(),&UDivineBeastsDemoPresentationObserver::OnThemeFinished);}if(auto* S=Local<UGamePlatformUIAdaptiveSubsystem>())S->OnAdaptiveContextChanged.RemoveDynamic(Observer.Get(),&UDivineBeastsDemoPresentationObserver::OnAdaptive);Observer.Reset();}
}
FGamePlatformResult FPresentationManualState::ReleaseAsset()
{
    if(!AssetLease.IsValid())return FGamePlatformResult::Success();auto* S=Data();
    const auto Result=S?S->ReleaseResources(AssetLease):PresentationError(TEXT("Demo.Presentation.ReleaseMissing"),TEXT("Data失效，未确认本资源租约释放。"));
    if(Result.IsSuccess())AssetLease={};return Result;
}
void FPresentationManualState::Load(const FSoftObjectPath& Path,TFunction<FGamePlatformResult(UObject*)> Apply,FDivineBeastsDemoOperationCompletion Done)
{
    if(AssetLease.IsValid()||LoadingCompletion||!Path.IsValid()||!Path.GetSubPathString().IsEmpty()||Path.ToString().Contains(TEXT("/Review/"))||Path.ToString().Contains(TEXT("/V3/")))
    {Done(Context.Token,PresentationError(TEXT("Demo.Presentation.ResourceBusy"),TEXT("先Reset释放上一份资源；仅接受真实顶层资源软路径。")));return;}
    auto* S=Data();if(!S){Done(Context.Token,PresentationError(TEXT("Demo.Presentation.DataMissing"),TEXT("中央Data失效。")));return;}
    LoadingCompletion=MoveTemp(Done);const uint64 E=Epoch;const TWeakPtr<FPresentationManualState> Weak=AsShared();FGamePlatformResult Accepted;
    // Data合同明确下一轮或更晚回调；只有完整租约匹配才消费，不假设缓存同步完成。
    AssetLease=S->AcquireResources({Path},EGamePlatformDataLifetime::World,World.Get(),[Weak,E,Path,Apply=MoveTemp(Apply)](const auto& Lease,const auto& Result)
    {
        auto Self=Weak.Pin();if(!Self||!Self->Current(E)||!Self->LoadingCompletion)return;
        const auto& H=Self->AssetLease;
        if(H.ScopeId!=Lease.ScopeId||H.LeaseId!=Lease.LeaseId||H.Generation!=Lease.Generation||H.IssuerProof!=Lease.IssuerProof||H.DefinitionId!=Lease.DefinitionId||H.Bundles!=Lease.Bundles||H.ResourcePaths!=Lease.ResourcePaths)return;
        auto Done=MoveTemp(Self->LoadingCompletion);auto Outcome=Result;
        TStrongObjectPtr<UObject> Loaded(Path.ResolveObject());
        if(Outcome.IsSuccess())Outcome=Apply(Loaded.Get());
        if(!Self->Current(E))Outcome=FGamePlatformResult::Cancelled(TEXT("加载应用期间来源已停止。"));
        Done(Self->Context.Token,Outcome);
    },Accepted);
    if(!Current(E))return;
    if(!Accepted.IsSuccess()){auto Callback=MoveTemp(LoadingCompletion);if(Callback)Callback(Context.Token,Accepted);}
}
FGamePlatformResult FPresentationManualState::ResetStage()
{
    if(auto* D=Dissolve.Get())D->ResetDissolve();Dissolve.Reset();
    if(bOwnView&&Stage.IsValid())
    {auto* C=Local<UGamePlatformCinematicCameraSubsystem>();auto* PC=Player.IsValid()?Player->GetPlayerController(World.Get()):nullptr;
        const bool Restored=C&&C->EndCinematicViewFor(Stage.Get());
        const bool Relinquished=C&&!C->IsCinematicViewActive()&&PC&&PC->GetViewTarget()!=Stage.Get();
        if(!Restored&&!Relinquished)return PresentationError(TEXT("Demo.Camera.Restore"),TEXT("指定机位未确认恢复，保留舞台和租约供Stop重试。"));bOwnView=false;}
    if(auto* S=Stage.Get())S->Destroy();Stage.Reset();return ReleaseAsset();
}
FGamePlatformResult FPresentationManualState::ResetPlayback()
{
    auto Result=FGamePlatformResult::Success();const uint64 E=Epoch;const auto Source=World;
    auto SameSource=[&]{return Epoch==E&&World==Source&&World.IsValid()&&!World->bIsTearingDown;};
    // 外部Stop可同步广播并重入Cleanup；遍历稳定值副本，仅按完整身份撤销仍拥有的记录。
    const auto VFXWork=VFXHandles;
    for(const auto& H:VFXWork)
    {
        if(!SameSource())return FGamePlatformResult::Cancelled(TEXT("停止播放期间来源已换代。"));
        auto* S=IGamePlatformVFXService::Get(World.Get());
        if(!S){Result=PresentationError(TEXT("Demo.VFX.StopService"),TEXT("原世界VFX服务不可用，保留完整句柄。"));break;}
        const bool Released=!S->IsActive(H)||S->Stop(H);
        if(!SameSource())return FGamePlatformResult::Cancelled(TEXT("VFX同步完成重入后不再修改旧执行栈。"));
        if(Released)VFXHandles.RemoveAll([&](const auto& Own){return Own==H;});
        else Result=PresentationError(TEXT("Demo.VFX.Stop"),TEXT("本实例未确认停止，完整句柄保留。"));
    }
    if(Preload.IsValid())
    {
        const auto Owned=Preload;auto* S=IGamePlatformVFXService::Get(World.Get());FString Error;
        if(!S)Result=PresentationError(TEXT("Demo.VFX.PreloadCancel"),TEXT("原预载服务不可用，句柄保留。"));
        else
        {
            const auto State=S->GetPreloadState(Owned,Error);
            const bool Released=State==EGamePlatformVFXPreloadState::Cancelled||State==EGamePlatformVFXPreloadState::Invalid||S->CancelPreload(Owned);
            if(!SameSource())return FGamePlatformResult::Cancelled(TEXT("预载取消期间来源已换代。"));
            if(Released&&Preload.Id==Owned.Id&&Preload.Generation==Owned.Generation&&Preload.World==Owned.World)Preload={};
            else if(!Released)Result=PresentationError(TEXT("Demo.VFX.PreloadCancel"),TEXT("本预载未确认取消，完整句柄保留。"));
        }
    }
    const auto SFXWork=SFXHandles;
    for(const auto& H:SFXWork)
    {
        if(!SameSource())return FGamePlatformResult::Cancelled(TEXT("停止播放期间来源已换代。"));
        auto* S=IGamePlatformSFXService::Get(World.Get());
        if(!S){Result=PresentationError(TEXT("Demo.SFX.StopService"),TEXT("原世界SFX服务不可用，保留完整句柄。"));break;}
        const bool Released=!S->IsActive(H)||S->Stop(H,0);
        if(!SameSource())return FGamePlatformResult::Cancelled(TEXT("SFX同步完成重入后不再修改旧执行栈。"));
        if(Released)SFXHandles.RemoveAll([&](const auto& Own){return PresentationManualSameSFXHandle(Own,H);});
        else Result=PresentationError(TEXT("Demo.SFX.Stop"),TEXT("本音效未确认停止，完整句柄保留。"));
    }
    return Result;
}
FGamePlatformResult FPresentationManualState::Cleanup()
{
    if(bCleaning)return PresentationError(TEXT("Demo.Presentation.CleanupReentry"),TEXT("清理正在进行，不能同步重入。"));
    bActive=false;bCleaning=true;++Epoch;
    if(Feature==TEXT("GamePlatformUI.theme"))
    {
        auto* UI=Local<UGamePlatformUIManagerSubsystem>();
        if(!UI&&(ThemeRequest.IsValid()||bThemeChanged)){bCleaning=false;return PresentationError(TEXT("Demo.UI.ThemeRestoreMissing"),TEXT("原主题服务失效，恢复身份保留。"));}
        if(ThemeRequest.IsValid()&&UI){const auto Request=ThemeRequest;if(!UI->CancelThemeRequest(Request)){bCleaning=false;return PresentationError(TEXT("Demo.UI.ThemeTerminalPending"),TEXT("本主题正发布或等待准确终态；保留请求与清理订阅，终态后重试Stop。"));}if(ThemeRequest==Request)ThemeRequest.Invalidate();}
        if(ThemeRestore.IsValid()){bCleaning=false;return PresentationError(TEXT("Demo.UI.ThemeRestorePending"),TEXT("等待原主题真实异步恢复终态；暂保留观察者和宿主，事件后重试Stop。"));}
        if(bThemeChanged&&UI)
        {const auto R=UI->RestoreThemeSnapshot(OriginalTheme,ThemeExpectedRevision,ThemeRestore);if(!R.IsSuccess()){bCleaning=false;return R;}if(ThemeRestore.IsValid()){bCleaning=false;return PresentationError(TEXT("Demo.UI.ThemeRestorePending"),TEXT("原主题恢复请求已受理，等待真实终态后重试Stop。"));}bThemeChanged=false;}
    }
    UnbindEvents();auto Pending=MoveTemp(LoadingCompletion);
    auto Result=ResetPlayback();auto Keep=[&](const FGamePlatformResult& R){if(Result.IsSuccess()&&!R.IsSuccess())Result=R;};
    Keep(CloseEditorWind());
    if(auto* F=Feet.Get())F->DestroyComponent();Feet.Reset();
    if(auto* S=Local<UGamePlatformUIManagerSubsystem>())
    {
        if(Opening.IsValid()){S->CancelOpen(Opening);Opening.Invalidate();}
        if(ThemeRequest.IsValid()){S->CancelThemeRequest(ThemeRequest);ThemeRequest.Invalidate();}
        Keep(CloseOwnedScreen());
        if(!RegisteredScreen.IsNone()&&!Screen.IsValid()){if(S->UnregisterScreenDefinition(RegisteredScreen)){RegisteredScreen=NAME_None;RegisteredDefinition.Reset();}else Keep(PresentationError(TEXT("Demo.UI.Unregister"),TEXT("本页面定义未确认注销。")));}
        if(Notification.IsValid()){if(auto* N=S->GetNotificationService()){N->DismissNotification(Notification);Notification.Invalidate();}else Keep(PresentationError(TEXT("Demo.UI.NotificationCleanup"),TEXT("通知服务失效，保留本身份。")));}
        if(Feedback.IsValid()){if(auto* N=S->GetFeedbackService()){N->DismissFeedback(Feedback);Feedback.Invalidate();}else Keep(PresentationError(TEXT("Demo.UI.FeedbackCleanup"),TEXT("反馈服务失效，保留本身份。")));}
        if(WorldUI.IsValid()){if(S->UnregisterWorldUI(WorldUI))WorldUI.Invalidate();else Keep(PresentationError(TEXT("Demo.UI.WorldRemove"),TEXT("本世界UI未确认注销。")));}
    }
    else if(Opening.IsValid()||Screen.IsValid()||!RegisteredScreen.IsNone()||WorldUI.IsValid()||Notification.IsValid()||Feedback.IsValid())Keep(PresentationError(TEXT("Demo.UI.ServiceMissing"),TEXT("原本地玩家UI服务失效，保留未清理身份。")));
    if(auto* P=Local<UGamePlatformPresentationClientSubsystem>())
    {for(int32 I=PresentationRequests.Num()-1;I>=0;--I){auto Request=PresentationRequests[I];Request.PredictionState=EGamePlatformPresentationPredictionState::Cancelled;const auto C=P->Submit(Request);if(C==EGamePlatformPresentationSubmitResult::Submitted||C==EGamePlatformPresentationSubmitResult::StaleWorld)PresentationRequests.RemoveAt(I);else Keep(PresentationError(TEXT("Demo.Presentation.Cancel"),TEXT("本中立请求取消未确认，保留身份供重试。")));}if(Catalog.IsValid()){if(P->UnregisterCatalogFragment(Catalog))Catalog={};else Keep(PresentationError(TEXT("Demo.Catalog.Withdraw"),TEXT("目录未确认撤销。")));}if(Contributor.IsValid()){if(P->UnregisterContextContributor(Contributor))Contributor={};else Keep(PresentationError(TEXT("Demo.Contributor.Withdraw"),TEXT("贡献器未确认撤销。")));}}
    if(bVolumesCaptured&&Instance())
    {if(auto* S=IGamePlatformSFXUserSettingsService::Get(*Instance())){const auto A=S->SetVolume(EGamePlatformSFXSoundGroup::Effects,OriginalVolumes.Effects);Keep(A);const auto B=S->SetVolume(EGamePlatformSFXSoundGroup::Ambient,OriginalVolumes.Ambient);Keep(B);const auto C=S->SetVolume(EGamePlatformSFXSoundGroup::Music,OriginalVolumes.Music);Keep(C);const auto R=S->CommitVolumes();Keep(R);if(A.IsSuccess()&&B.IsSuccess()&&C.IsSuccess()&&R.IsSuccess())bVolumesCaptured=false;}else Keep(PresentationError(TEXT("Demo.SFX.Restore"),TEXT("音量服务不可用，恢复待重试。")));}
    if(bSurfaceCaptured&&World.IsValid())
    {if(auto* S=IGamePlatformSurfaceService::Get(*World)){const auto R=S->ApplyEnvironmentState(OriginalSurface);if(R.IsStateAccepted()&&(R.Status==EGamePlatformSurfaceUpdateStatus::Applied||R.Status==EGamePlatformSurfaceUpdateStatus::Unchanged)&&R.FailedParameterCount==0)bSurfaceCaptured=false;else Keep(PresentationError(TEXT("Demo.Surface.Restore"),TEXT("完整原表面/MPC恢复尚未完成，保留快照供重试。")));}else Keep(PresentationError(TEXT("Demo.Surface.RestoreService"),TEXT("原表面服务失效。")));}
    if(bShakeScaleCaptured){if(auto* S=Local<UGamePlatformCameraHitFeedbackSubsystem>()){S->SetUserCameraShakeScale(OriginalShakeScale);bShakeScaleCaptured=false;}else Keep(PresentationError(TEXT("Demo.Camera.ComfortRestore"),TEXT("舒适度原值未恢复。")));}
    // 已失败的页面/注册不能先丢其Data需求；真正释放由下一次Stop成功后执行。
    if(!Screen.IsValid()&&RegisteredScreen.IsNone())Keep(ResetStage());
    if(Result.IsSuccess()&&bHost){if(auto* H=Host.Get()){const auto R=H->Release(Context.Token);Keep(R);if(R.IsSuccess())bHost=false;}else Keep(PresentationError(TEXT("Demo.Presentation.HostLost"),TEXT("排他宿主失效，未确认释放。")));}
    bCleaning=false;if(Pending)Pending(Context.Token,FGamePlatformResult::Cancelled(TEXT("停止本次资源加载操作。")));return Result;
}
void FPresentationManualState::Execute(FName Op,const FPresentationArgs& A,FDivineBeastsDemoOperationCompletion Done)
{
    if(Feature.ToString().StartsWith(TEXT("GamePlatformVFX.")))ExecuteVFX(Op,A,MoveTemp(Done));
    else if(Feature.ToString().StartsWith(TEXT("GamePlatformSFX.")))ExecuteSFX(Op,A,MoveTemp(Done));
    else if(Feature.ToString().StartsWith(TEXT("GamePlatformPresentation.")))ExecutePresentation(Op,A,MoveTemp(Done));
    else if(Feature.ToString().StartsWith(TEXT("GamePlatformCamera.")))ExecuteCamera(Op,A,MoveTemp(Done));
    else if(Feature.ToString().StartsWith(TEXT("GamePlatformUI.")))ExecuteUI(Op,A,MoveTemp(Done));
    else ExecuteSurface(Op,A,MoveTemp(Done));
}
class FPresentationManualScenario final:public IDivineBeastsDemoScenarioAdapter
{
    TSharedRef<FPresentationManualState> State;
public:
    FPresentationManualScenario(UWorld& W,FName F):State(MakeShared<FPresentationManualState>()){State->World=&W;State->Feature=F;}
    ~FPresentationManualScenario()override{State->Cleanup();}
    void Prepare(const FDivineBeastsDemoScenarioContext& C,FDivineBeastsDemoOperationCompletion Done)override
    {
        auto S=State;if(S->bBound){Done(C.Token,PresentationError(TEXT("Demo.Presentation.Rebind"),TEXT("适配不能重绑定另一Token。")));return;}
        S->bBound=true;S->Context=C;
        if(!C.Token.IsValid()||C.World.Get()!=S->World.Get()||C.Scenario.FeatureId!=S->Feature||!ValidatePresentationManualContract(C.Scenario))
        {Done(C.Token,PresentationError(TEXT("Demo.Presentation.Contract"),TEXT("完整运行身份、来源或编译白名单不匹配。")));return;}
        auto Result=S->Prepare();if(!Result.IsSuccess())S->bActive=false;Done(C.Token,Result);
    }
    void Execute(FName Op,const TArray<FDivineBeastsDemoParameterValue>& V,FDivineBeastsDemoOperationCompletion Done)override
    {
        auto S=State;if(!S->Alive()||!S->Host.IsValid()||!S->Host->Owns(S->Context.Token)||!PresentationManualOperations(S->Feature).Contains(Op)||Op==TEXT("Stop")||V.Num()!=S->Context.Scenario.Parameters.Num())
        {Done(S->Context.Token,PresentationError(TEXT("Demo.Presentation.Operation"),TEXT("来源失活、宿主非本运行、操作或参数数量非法。")));return;}
        const auto Valid=ValidatePresentationManualValues(S->Context.Scenario,V);if(!Valid.IsSuccess()){Done(S->Context.Token,Valid);return;}
        S->Execute(Op,FPresentationArgs{S->Context,V},MoveTemp(Done));
    }
    void Stop(FDivineBeastsDemoOperationCompletion Done)override{auto S=State;Done(S->Context.Token,S->Cleanup());}
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePresentationManualScenario(UWorld& W,FName F)
{if(!PresentationManualFeatures().Contains(F))return nullptr;return MakeUnique<FPresentationManualScenario>(W,F);}
}
