#include "Scenarios/DivineBeastsDemoPresentationManualState.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Adaptive/GamePlatformUIAdaptiveSubsystem.h"
#include "Definitions/GamePlatformUIScreenDefinition.h"
#include "Screens/GamePlatformUIScreen.h"
#include "Services/GamePlatformNotificationService.h"
#include "Services/GamePlatformFeedbackService.h"
#include "Services/GamePlatformWorldUIService.h"
#include "Notifications/GamePlatformNotificationWidget.h"
#include "Feedback/GamePlatformFeedbackWidget.h"
#include "WorldUI/GamePlatformWorldWidgetBase.h"
#include "Interfaces/IGamePlatformSurfaceService.h"
#include "Marks/IGamePlatformSurfaceMarkService.h"
#include "Marks/GamePlatformSurfaceMarkCatalog.h"
#include "CollisionQueryParams.h"
#include "Components/GamePlatformSnowFootContactComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Settings/GamePlatformSurfaceSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include <limits>
namespace DivineBeasts::Demo
{
void FPresentationManualState::ExecuteUI(FName Op,const FPresentationArgs& A,FDivineBeastsDemoOperationCompletion Done)
{
    auto* S=Local<UGamePlatformUIManagerSubsystem>();if(!S){Done(Context.Token,PresentationError(TEXT("Demo.UI.Missing"),TEXT("实际LocalPlayer UIManager不可用。")));return;}
    if(Op==TEXT("RefreshAdaptive"))
    {auto* V=Local<UGamePlatformUIAdaptiveSubsystem>();if(!V){Done(Context.Token,PresentationError(TEXT("Demo.UI.AdaptiveMissing"),TEXT("同LocalPlayer自适应服务不可用。")));return;}V->RefreshAdaptiveContext();if(!Alive()){Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("自适应事件期间来源已失活。")));return;}const auto C=V->GetAdaptiveContext();Observe(FString::Printf(TEXT("首次/显式快照 Viewport=%.0fx%.0f DPI=%.3f；之后由真实输入/视口变化通知。"),C.ViewportSize.X,C.ViewportSize.Y,C.DPIScale));Done(Context.Token,FGamePlatformResult::Success());return;}
    if(Op==TEXT("InspectTheme")){const auto V=S->GetThemeSnapshot();Observe(FString::Printf(TEXT("当前主题=%s Revision=%lld Platform=%s Skin=%s Quality=%d Pending=%d；Stop以完整原快照条件恢复。"),*V.ThemeId.ToString(),V.Revision,*V.Context.PlatformId.ToString(),*V.Context.SkinId.ToString(),V.Context.QualityTier,V.bRequestPending));Done(Context.Token,FGamePlatformResult::Success());return;}
    if(Op==TEXT("CancelTheme")){const bool Ok=ThemeRequest.IsValid()&&S->CancelThemeRequest(ThemeRequest);if(Ok)ThemeRequest.Invalidate();Done(Context.Token,Ok?FGamePlatformResult::Success():PresentationError(TEXT("Demo.UI.ThemeCancel"),TEXT("本主题无在途请求或真实取消被拒绝。")));return;}
    if(Op==TEXT("SwitchTheme"))
    {const auto Current=S->GetThemeSnapshot();if(!OriginalTheme.ThemeId.IsValid()||Current.ScopeId!=OriginalTheme.ScopeId||Current.Revision!=ThemeExpectedRevision||Current.bRequestPending){Done(Context.Token,PresentationError(TEXT("Demo.UI.ThemeConflict"),TEXT("主题基线、修订或外部请求变化，拒绝覆盖。")));return;}
        RequestedTheme=FPrimaryAssetId(TEXT("GamePlatformDefinition"),FName(*(A.Id(TEXT("ThemeId")).ToString()+TEXT("@")+FString::FromInt(A.Int(TEXT("ThemeVersion"),1)))));RequestedThemeContext={};RequestedThemeContext.PlatformId=A.Id(TEXT("Platform"));RequestedThemeContext.SkinId=A.Id(TEXT("Skin"))==TEXT("Default")?NAME_None:A.Id(TEXT("Skin"));RequestedThemeContext.QualityTier=A.Int(TEXT("ThemeQuality"),-1);ThemeSource=S;ThemeRequest=S->RequestThemeAsync(RequestedTheme,RequestedThemeContext);Done(Context.Token,ThemeRequest.IsValid()?FGamePlatformResult::Success():PresentationError(TEXT("Demo.UI.ThemeRejected"),TEXT("真实主题请求未受理。")));return;}
    if(Op==TEXT("RejectInvalidTheme")){const FGuid Rejected=S->RequestThemeAsync(FPrimaryAssetId(),FGamePlatformUIThemeContext());if(Rejected.IsValid())S->CancelThemeRequest(Rejected);Done(Context.Token,Rejected.IsValid()?PresentationError(TEXT("Demo.UI.InvalidThemeAccepted"),TEXT("非法空主题身份意外受理。")):PresentationError(TEXT("Demo.UI.InvalidTheme"),TEXT("真实主题入口拒绝空PrimaryAssetId，原在途请求身份保持。")));return;}
    if(Op==TEXT("CancelOpen")){const bool Ok=Opening.IsValid()&&S->CancelOpen(Opening);if(Ok)Opening.Invalidate();Done(Context.Token,Ok?FGamePlatformResult::Success():PresentationError(TEXT("Demo.UI.CancelOpen"),TEXT("无本运行在途打开请求或真实取消被拒绝。")));return;}
    if(Op==TEXT("Close")||Op==TEXT("Reset"))
    {if(Opening.IsValid()){S->CancelOpen(Opening);Opening.Invalidate();}const auto Closed=CloseOwnedScreen();if(!Closed.IsSuccess()){Done(Context.Token,Closed);return;}if(!RegisteredScreen.IsNone()){if(!S->UnregisterScreenDefinition(RegisteredScreen)){Done(Context.Token,PresentationError(TEXT("Demo.UI.Unregister"),TEXT("本定义未确认注销。")));return;}RegisteredScreen=NAME_None;RegisteredDefinition.Reset();}Done(Context.Token,ReleaseAsset());return;}
    if(Op==TEXT("Dismiss"))
    {if(Notification.IsValid()){auto* N=S->GetNotificationService();if(!N){Done(Context.Token,PresentationError(TEXT("Demo.UI.NotificationService"),TEXT("通知服务失效，未释放身份。")));return;}N->DismissNotification(Notification);Notification.Invalidate();}
        if(Feedback.IsValid()){auto* F=S->GetFeedbackService();if(!F){Done(Context.Token,PresentationError(TEXT("Demo.UI.FeedbackService"),TEXT("反馈服务失效，未释放身份。")));return;}F->DismissFeedback(Feedback);Feedback.Invalidate();}Done(Context.Token,ReleaseAsset());return;}
    if(Op==TEXT("RemoveWorldUI")){const bool Ok=WorldUI.IsValid()&&S->UnregisterWorldUI(WorldUI);if(Ok)WorldUI.Invalidate();Done(Context.Token,Ok?ReleaseAsset():PresentationError(TEXT("Demo.UI.WorldRemove"),TEXT("本实例未确认注销。")));return;}
    if(Op==TEXT("MoveWorldUI")||Op==TEXT("HideWorldUI"))
    {WorldUIRequest.WorldLocation=A.Location();WorldUIRequest.Resource.CurrentValue=A.Number(TEXT("Current"),75);WorldUIRequest.bDisplayRequested=Op!=TEXT("HideWorldUI");const bool Ok=WorldUI.IsValid()&&S->UpdateWorldUI(WorldUI,WorldUIRequest);Done(Context.Token,Ok?FGamePlatformResult::Success():PresentationError(TEXT("Demo.UI.WorldUpdate"),TEXT("本世界UI实例更新被拒绝。")));return;}
    const TWeakPtr<FPresentationManualState> Weak=AsShared();
    if(Op==TEXT("Open"))
    {
        Load(A.Asset(TEXT("ScreenDefinition")),[Weak](UObject* O)
        {auto Self=Weak.Pin();auto* D=Cast<UGamePlatformUIScreenDefinition>(O);if(!Self||!Self->Alive()||!D)return PresentationError(TEXT("Demo.UI.DefinitionType"),TEXT("真实资源不是UIScreenDefinition。"));return Self->OpenOwnedScreen(*D);},MoveTemp(Done));return;
    }
    const FVector Location=A.Location();const double Current=A.Number(TEXT("Current"),75);const float Lifetime=A.Number(TEXT("Lifetime"),1);
    Load(A.Asset(TEXT("WidgetClass")),[Weak,Op,Location,Current,Lifetime](UObject* O)
    {
        auto Self=Weak.Pin();auto* Class=Cast<UClass>(O);if(!Self||!Self->Alive()||!Class||Class->HasAnyClassFlags(CLASS_Abstract))return PresentationError(TEXT("Demo.UI.WidgetType"),TEXT("需要已加载非抽象原生/蓝图Widget类。"));auto* M=Self->Local<UGamePlatformUIManagerSubsystem>();if(!M)return PresentationError(TEXT("Demo.UI.Missing"),TEXT("本LP UIManager失效。"));const uint64 E=Self->Epoch;
        if(Op==TEXT("Notify")&&Class->IsChildOf(UGamePlatformNotificationWidget::StaticClass()))
        {FGamePlatformUINotificationRequest R;R.RequestId=FGuid::NewGuid();R.NotificationKey=FName(*Self->Context.Token.RunId.ToString());R.Channel=TEXT("Manual");R.Title=FText::FromString(TEXT("SSLM通知样板"));R.Message=FText::FromString(TEXT("平台通知服务真实显示；仅本运行可关闭。"));R.LifetimeSeconds=0;const auto Id=M->SubmitNotification(R,Class);if(!Self->Current(E)){if(auto* N=M->GetNotificationService())N->DismissNotification(Id);return FGamePlatformResult::Cancelled(TEXT("通知重入失活。"));}Self->Notification=Id;return Id.IsValid()?FGamePlatformResult::Success():PresentationError(TEXT("Demo.UI.NotificationRejected"),TEXT("真实通知服务拒绝。"));}
        if(Op==TEXT("Feedback")&&Class->IsChildOf(UGamePlatformFeedbackWidget::StaticClass()))
        {FGamePlatformUIFeedbackRequest R;R.OccurrenceId=FGuid::NewGuid();R.Channel=TEXT("Manual");R.Text=FText::FromString(TEXT("+75 视觉反馈"));R.LifetimeSeconds=Lifetime;R.ScreenPosition=FVector2D(800,300);const auto Id=M->SubmitFeedback(R,Class);if(!Self->Current(E)){if(auto* F=M->GetFeedbackService())F->DismissFeedback(Id);return FGamePlatformResult::Cancelled(TEXT("反馈重入失活。"));}Self->Feedback=Id;return Id.IsValid()?FGamePlatformResult::Success():PresentationError(TEXT("Demo.UI.FeedbackRejected"),TEXT("真实反馈服务拒绝。"));}
        if(Op==TEXT("RegisterWorldUI")&&Class->IsChildOf(UGamePlatformWorldWidgetBase::StaticClass()))
        {FGamePlatformWorldUIRequest R;R.RequestId=FGuid::NewGuid();R.SurfaceKey=FName(*Self->Context.Token.RunId.ToString());R.PrimaryText=FText::FromString(TEXT("SSLM事件投影"));R.WorldLocation=Location;R.Resource.ResourceId=TEXT("Manual");R.Resource.CurrentValue=Current;R.Resource.MaximumValue=100;const auto Id=M->RegisterWorldUI(R,Class);if(!Self->Current(E)){M->UnregisterWorldUI(Id);return FGamePlatformResult::Cancelled(TEXT("世界UI注册重入失活。"));}Self->WorldUI=Id;Self->WorldUIRequest=R;return Id.IsValid()?FGamePlatformResult::Success():PresentationError(TEXT("Demo.UI.WorldRejected"),TEXT("真实世界投影服务拒绝。"));}
        return PresentationError(TEXT("Demo.UI.ClassMismatch"),TEXT("当前命令与真实Widget基类不匹配。"));
    },MoveTemp(Done));
}
void FPresentationManualState::ExecuteSurface(FName Op,const FPresentationArgs& A,FDivineBeastsDemoOperationCompletion Done)
{
    if(Feature==TEXT("GamePlatformSurface.editor_wind")){Done(Context.Token,ExecuteEditorWind(Op,A));return;}
    auto* S=World.IsValid()?IGamePlatformSurfaceService::Get(*World):nullptr;if(!S){Done(Context.Token,PresentationError(TEXT("Demo.Surface.Missing"),TEXT("本World无Surface服务。")));return;}
    if(Feature==TEXT("GamePlatformSurface.marks"))
    {
        auto* M=IGamePlatformSurfaceMarkService::Get(*World);if(!M){Done(Context.Token,PresentationError(TEXT("Demo.Surface.MarkService"),TEXT("本World痕迹服务不可用。")));return;}
        if(Op==TEXT("Counts")){int32 Active=0,Pending=0;M->GetCounts(Active,Pending);Observe(FString::Printf(TEXT("显式World痕迹快照 Active=%d Pending=%d；包含本World其他生产者，不能当本运行独占数。"),Active,Pending));Done(Context.Token,FGamePlatformResult::Success());return;}
        if(Op==TEXT("SubmitMark"))
        {if(MarkRequests.Num()>=64){Done(Context.Token,PresentationError(TEXT("Demo.Surface.MarkLimit"),TEXT("单运行最多提交64个有限期痕迹。")));return;}FHitResult Hit;const auto P=A.Location();FCollisionQueryParams Query(SCENE_QUERY_STAT(DBAManualMark),false);
            if(!World->LineTraceSingleByChannel(Hit,P+FVector(0,0,100),P-FVector(0,0,500),ECC_Visibility,Query)||!Hit.bBlockingHit){Done(Context.Token,PresentationError(TEXT("Demo.Surface.NoHit"),TEXT("真实场景射线无命中；不伪造表面位置。")));return;}
            LastMark={};LastMark.EventId=FGuid::NewGuid();LastMark.Kind=static_cast<EGamePlatformSurfaceMarkKind>(A.Int(TEXT("Kind")));LastMark.Location=Hit.ImpactPoint;LastMark.Normal=Hit.ImpactNormal;LastMark.HitComponent=Hit.GetComponent();LastMark.SizeScale=A.Number(TEXT("Size"),1);MarkRequests.Add(LastMark.EventId);}
        else if(!LastMark.EventId.IsValid()){Done(Context.Token,PresentationError(TEXT("Demo.Surface.NoMark"),TEXT("先提交本运行真实命中痕迹。")));return;}
        const auto Path=GetDefault<UGamePlatformSurfaceSettings>()->MarkCatalog.ToSoftObjectPath();const auto Request=LastMark;const TWeakPtr<FPresentationManualState> Weak=AsShared();
        auto Apply=[Weak,Request](UObject* Object)
        {auto Self=Weak.Pin();auto* Catalog=Cast<UGamePlatformSurfaceMarkCatalog>(Object);const auto* Style=Catalog?Catalog->FindStyle(Request.Kind,Request.SurfaceType):nullptr;
            if(!Self||!Self->Alive()||!Style||!FMath::IsFinite(Style->LifetimeSeconds)||Style->LifetimeSeconds<=0||Style->LifetimeSeconds>120)return PresentationError(TEXT("Demo.Surface.FiniteMarkRequired"),TEXT("必须有真实目录配方且寿命0..120秒；不提交永久痕迹。"));
            auto* Service=IGamePlatformSurfaceMarkService::Get(*Self->World);if(!Service)return PresentationError(TEXT("Demo.Surface.MarkService"),TEXT("原世界痕迹服务失效。"));
            const auto R=Service->Submit(Request);Self->Observe(FString::Printf(TEXT("痕迹Submit=%d Active=%d Pending=%d；有限寿命交原生服务，Stop只撤本观察，不清其他痕迹。"),int32(R.Status),R.ActiveCount,R.PendingCount));
            return (R.Status==EGamePlatformSurfaceMarkStatus::Spawned||R.Status==EGamePlatformSurfaceMarkStatus::Pending)?FGamePlatformResult::Success():PresentationError(TEXT("Demo.Surface.MarkRejected"),TEXT("真实痕迹服务拒绝/去重，Pending不能当实际渲染。"));};
        if(AssetLease.IsValid()){if(AssetLease.ResourcePaths.Contains(Path))Done(Context.Token,Apply(Path.ResolveObject()));else Done(Context.Token,PresentationError(TEXT("Demo.Surface.CatalogChanged"),TEXT("目录配置已变，先Stop释放本快照。")));}
        else Load(Path,MoveTemp(Apply),MoveTemp(Done));return;
    }
    if(Feature==TEXT("GamePlatformSurface.snow_contact"))
    {
        if(Op==TEXT("DetachFeet")){if(auto* F=Feet.Get())F->DestroyComponent();Feet.Reset();Done(Context.Token,FGamePlatformResult::Success());return;}
        auto* PC=Player->GetPlayerController(World.Get());auto* C=PC?Cast<ACharacter>(PC->GetPawn()):nullptr;
        if(!C||!C->GetMesh()||C->FindComponentByClass<UGamePlatformSnowFootContactComponent>()||!GetDefault<UGamePlatformSurfaceSettings>()->bEnableSnowContacts||S->GetEnvironmentState().GlobalSnowAmount<.01f)
        {Done(Context.Token,PresentationError(TEXT("Demo.Surface.FootPrerequisite"),TEXT("需要同LP真实Character/Mesh、启用雪接触与积雪，且不得重复附加现有足底组件。")));return;}
        auto* F=NewObject<UGamePlatformSnowFootContactComponent>(C);Feet=F;F->SetContactMesh(C->GetMesh());F->SetContactBoneCandidates({A.Id(TEXT("LeftFoot"))},{A.Id(TEXT("RightFoot"))});C->AddInstanceComponent(F);F->RegisterComponent();
        Observe(TEXT("已装配本运行足底组件；人工走动/跳跃，平台最终骨骼事件+真实地面检测驱动雪印，禁止伪造接触。"));Done(Context.Token,FGamePlatformResult::Success());return;
    }
    FGamePlatformSurfaceUpdateResult R;
    if(Op==TEXT("RefreshBinding"))R=S->RefreshMaterialBinding();
    else
    {
        auto State=Op==TEXT("Reset")?OriginalSurface:LastSurface;
        if(Op==TEXT("ApplyState"))
        {State.GlobalWetness=A.Number(TEXT("Wetness"));State.GlobalSnowAmount=A.Number(TEXT("Snow"));State.GlobalSnowHeightCm=A.Number(TEXT("SnowHeight"));State.GlobalMossInfluence=A.Number(TEXT("Moss"),1);State.GlobalPuddleAmount=A.Number(TEXT("Puddle"));State.RainIntensity=A.Number(TEXT("Rain"));State.SnowIntensity=A.Number(TEXT("SnowIntensity"));State.TemperatureCelsius=A.Number(TEXT("Temperature"),20);
            State.Wind.From=FVector4(A.Number(TEXT("WindFromYaw")),A.Number(TEXT("WindFromStrength")),A.Number(TEXT("WindFromGust")),A.Number(TEXT("WindFromHz")));
            State.Wind.To=FVector4(A.Number(TEXT("WindToYaw")),A.Number(TEXT("WindToStrength")),A.Number(TEXT("WindToGust")),A.Number(TEXT("WindToHz")));
            State.Wind.Timing=FVector4(World->GetTimeSeconds(),A.Number(TEXT("WindSeconds"),2),A.Number(TEXT("WindPhase")),0);State.Wind.Seed=A.Int(TEXT("Seed"),2026);}
        if(Op==TEXT("RejectNonFinite"))State.GlobalWetness=std::numeric_limits<float>::quiet_NaN();
        const uint64 E=Epoch;R=S->ApplyEnvironmentState(State);if(Current(E)&&R.IsStateAccepted())LastSurface=S->GetEnvironmentState();
    }
    Observe(FString::Printf(TEXT("表面真实结果 Status=%d Revision=%d Updated=%d Failed=%d；缓存接受和材质写入分别记录。"),int32(R.Status),R.Revision,R.UpdatedParameterCount,R.FailedParameterCount));
    auto Outcome=R.IsStateAccepted()?FGamePlatformResult::Success():PresentationError(TEXT("Demo.Surface.Rejected"),TEXT("真实表面服务未接受此状态。"));
    // 只有真实完整写入/无变化可说已应用；Pending、缺MPC合同保持明确未完成结果。
    if(R.IsStateAccepted()&&R.Status!=EGamePlatformSurfaceUpdateStatus::Applied&&R.Status!=EGamePlatformSurfaceUpdateStatus::Unchanged)
        Outcome=PresentationError(TEXT("Demo.Surface.MaterialIncomplete"),TEXT("状态已接受，但MPC仍Pending/缺失/合同不完整；不能按材质成功处理。"));
    Done(Context.Token,Outcome);
}
}
