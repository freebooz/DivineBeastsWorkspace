#include "Scenarios/DivineBeastsDemoPresentationManualState.h"
#include "Preview/GamePlatformCharacterPreviewStage.h"
#include "Cinematic/GamePlatformCinematicCameraSubsystem.h"
#include "Cinematic/GamePlatformCinematicCameraProfile.h"
#include "Feedback/GamePlatformCameraHitFeedbackSubsystem.h"
#include "Components/GamePlatformMeshDissolveComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/CameraComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
namespace DivineBeasts::Demo
{
class FManualContextContributor final:public IGamePlatformPresentationContextContributor
{
    FName Id;
public:
    explicit FManualContextContributor(FName In):Id(In){}
    FName GetContributorId()const override{return Id;}
    int32 GetPriority()const override{return 0;}
    EGamePlatformPresentationContextScope GetScope()const override{return EGamePlatformPresentationContextScope::World;}
    EGamePlatformPresentationConflictPolicy GetConflictPolicy()const override{return EGamePlatformPresentationConflictPolicy::FillMissing;}
    FGamePlatformPresentationContextPatch BuildPatch()const override{FGamePlatformPresentationContextPatch P;P.Values.RegionId=Id;return P;}
};
void FPresentationManualState::ExecutePresentation(FName Op,const FPresentationArgs& A,FDivineBeastsDemoOperationCompletion Done)
{
    if(Op==TEXT("LoadPreview"))
    {
        const auto Location=A.Location();const TWeakPtr<FPresentationManualState> Weak=AsShared();
        Load(A.Asset(TEXT("Mesh")),[Weak,Location](UObject* Object)
        {auto S=Weak.Pin();auto* Mesh=Cast<USkeletalMesh>(Object);if(!S||!S->Alive()||!Mesh)return PresentationError(TEXT("Demo.Preview.Type"),TEXT("中央Data载入对象不是SkeletalMesh。"));
            auto* Stage=S->World->SpawnActor<AGamePlatformCharacterPreviewStage>(Location,FRotator::ZeroRotator);if(!Stage)return PresentationError(TEXT("Demo.Preview.Spawn"),TEXT("真实World舞台创建失败。"));S->Stage=Stage;
            if(!Stage->ApplyPreviewAppearance(Mesh,{},nullptr,FVector::ZeroVector,FRotator::ZeroRotator,FVector::OneVector))return PresentationError(TEXT("Demo.Preview.Appearance"),TEXT("真实预览舞台拒绝外观。"));
            if(!S->Alive())return FGamePlatformResult::Cancelled(TEXT("外观应用期间来源失活。"));
            if(S->Feature==TEXT("GamePlatformVFX.dissolve")){auto* D=NewObject<UGamePlatformMeshDissolveComponent>(Stage);S->Dissolve=D;Stage->AddInstanceComponent(D);D->RegisterComponent();}
            return FGamePlatformResult::Success();},MoveTemp(Done));return;
    }
    if(Op==TEXT("Reset")){Done(Context.Token,ResetStage());return;}
    if(Op==TEXT("Rotate")||Op==TEXT("Distance"))
    {if(!Stage.IsValid()){Done(Context.Token,PresentationError(TEXT("Demo.Preview.Missing"),TEXT("先从中央Data加载真实预览Mesh。")));return;}if(Op==TEXT("Rotate"))Stage->SetPreviewYaw(A.Number(TEXT("Yaw")));else Stage->SetCameraDistance(A.Number(TEXT("Distance"),320));Done(Context.Token,FGamePlatformResult::Success());return;}
    auto* S=Local<UGamePlatformPresentationClientSubsystem>();if(!S){Done(Context.Token,PresentationError(TEXT("Demo.Presentation.Missing"),TEXT("实际LocalPlayer中立表现服务不可用。")));return;}
    if(Op==TEXT("Withdraw"))
    {bool Ok=true;if(Catalog.IsValid()){Ok=S->UnregisterCatalogFragment(Catalog);if(Ok)Catalog={};}if(Contributor.IsValid()){const bool R=S->UnregisterContextContributor(Contributor);Ok&=R;if(R)Contributor={};}Done(Context.Token,Ok?FGamePlatformResult::Success():PresentationError(TEXT("Demo.Catalog.Withdraw"),TEXT("撤销失败保留句柄供Stop重试。")));return;}
    if(Op==TEXT("Register")||Op==TEXT("Conflict"))
    {
        FGamePlatformPresentationCatalogFragment F;F.FragmentId=FName(*Context.Token.RunId.ToString());F.OwnerScopeId=F.FragmentId;F.LifecycleScope=EGamePlatformPresentationContextScope::World;
        FGamePlatformPresentationCatalogEntry Entry;Entry.EntryId=TEXT("ManualExact");Entry.SemanticTag=FGameplayTag::RequestGameplayTag(A.Id(TEXT("Semantic")),false);Entry.ProviderChannel=A.Id(TEXT("Channel"));Entry.DefinitionId=A.Definition();Entry.ContentRevision=TEXT("manual-current");Entry.Scope=EGamePlatformPresentationCatalogScope::Project;Entry.Priority=A.Int(TEXT("Priority"));
        F.Entries.Add(Entry);FString Error;
        if(Op==TEXT("Conflict")){auto Duplicate=Entry;Duplicate.EntryId=TEXT("ManualDuplicate");F.Entries.Add(Duplicate);}
        if(!S->PreflightCatalogFragment(F,Error)){Observe(Error);Done(Context.Token,PresentationError(TEXT("Demo.Catalog.Preflight"),TEXT("真实目录预检拒绝非法/歧义合同；未发布目录。")));return;}
        if(Op==TEXT("Conflict")){Done(Context.Token,PresentationError(TEXT("Demo.Catalog.ConflictNotRejected"),TEXT("完全同键歧义未按预期拒绝，需调查真实目录门禁。")));return;}
        if(Catalog.IsValid()){Done(Context.Token,PresentationError(TEXT("Demo.Catalog.Owned"),TEXT("先撤销本运行目录，避免重复注册。")));return;}
        Contributor=S->RegisterContextContributor(MakeShared<FManualContextContributor>(F.OwnerScopeId));
        const uint64 E=Epoch;Catalog=S->RegisterCatalogFragment(F);
        if(!Current(E)){if(Catalog.IsValid())S->UnregisterCatalogFragment(Catalog);if(Contributor.IsValid())S->UnregisterContextContributor(Contributor);Catalog={};Contributor={};Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("注册期间来源失活。")));return;}
        Done(Context.Token,Catalog.IsValid()&&Contributor.IsValid()?FGamePlatformResult::Success():PresentationError(TEXT("Demo.Catalog.Register"),TEXT("真实目录/贡献器未完整注册；已有句柄保留供撤销。")));return;
    }
    if(Op==TEXT("Resolve"))
    {FGamePlatformPresentationContext C;const bool ContextOk=S->BuildContext(C);FGamePlatformPresentationResolvedEntry R;const auto Status=S->ResolveCatalog(FGameplayTag::RequestGameplayTag(A.Id(TEXT("Semantic")),false),C,R);Observe(FString::Printf(TEXT("上下文=%d 目录解析=%d Entry=%s Definition=%s Provider=%s；未播放资源。"),ContextOk,int32(Status),*R.EntryId.ToString(),*R.DefinitionId.ToString(),*R.ProviderChannel.ToString()));Done(Context.Token,ContextOk&&Status==EGamePlatformPresentationCatalogResolveResult::Resolved?FGamePlatformResult::Success():PresentationError(TEXT("Demo.Catalog.Resolve"),TEXT("真实上下文或目录未解析。")));return;}
    if(Op==TEXT("Submit")||Op==TEXT("Stale"))
    {if(PresentationRequests.Num()>=64){Done(Context.Token,PresentationError(TEXT("Demo.Presentation.OwnedLimit"),TEXT("本页最多提交64请求；先Stop。")));return;}LastPresentation={};LastPresentation.RequestId=FGuid::NewGuid();LastPresentation.SemanticTag=FGameplayTag::RequestGameplayTag(A.Id(TEXT("Semantic")),false);LastPresentation.ContextId=FName(*Context.Token.RunId.ToString());LastPresentation.ProviderChannel=A.Id(TEXT("Channel"));LastPresentation.DefinitionId=A.Definition();LastPresentation.ContentRevision=TEXT("manual-current");LastPresentation.SourceLocation=A.Location();LastPresentation.WorldGeneration=S->GetWorldGeneration();LastPresentation.RequestGeneration=1;if(Op==TEXT("Stale"))LastPresentation.WorldGeneration+=100;}
    else if(!LastPresentation.RequestId.IsValid()){Done(Context.Token,PresentationError(TEXT("Demo.Presentation.NoRequest"),TEXT("先提交本运行真实请求。")));return;}
    if(Op==TEXT("Confirm"))LastPresentation.PredictionState=EGamePlatformPresentationPredictionState::Confirmed;
    if(Op==TEXT("Cancel"))LastPresentation.PredictionState=EGamePlatformPresentationPredictionState::Cancelled;
    const uint64 E=Epoch;const auto Submitted=LastPresentation;const TStrongObjectPtr<UGamePlatformPresentationClientSubsystem> Keep(S);const auto Result=S->Submit(Submitted);
    if(!Current(E)){if(World.IsValid()&&!World->bIsTearingDown&&Local<UGamePlatformPresentationClientSubsystem>()==S){auto Cancelled=Submitted;Cancelled.PredictionState=EGamePlatformPresentationPredictionState::Cancelled;S->Submit(Cancelled);}Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("中立提交期间来源失活。")));return;}
    if((Result==EGamePlatformPresentationSubmitResult::Submitted||Result==EGamePlatformPresentationSubmitResult::Pending)&&!PresentationRequests.ContainsByPredicate([&](const auto& R){return R.RequestId==Submitted.RequestId;}))PresentationRequests.Add(Submitted);
    auto R=Result==EGamePlatformPresentationSubmitResult::Submitted?FGamePlatformResult::Success():PresentationError(TEXT("Demo.Presentation.Submit"),TEXT("真实中立总线未受理；Pending/旧世界/无Provider不得冒充Submitted。"));R.Message=FString::Printf(TEXT("SubmitResult=%d；播放终态和人工视听另验。"),int32(Result));Done(Context.Token,R);
}
void FPresentationManualState::ExecuteCamera(FName Op,const FPresentationArgs& A,FDivineBeastsDemoOperationCompletion Done)
{
    if(Feature==TEXT("GamePlatformCamera.cinematic"))
    {
        auto* S=Local<UGamePlatformCinematicCameraSubsystem>();if(!S){Done(Context.Token,PresentationError(TEXT("Demo.Camera.Missing"),TEXT("当前LocalPlayer无电影机位服务。")));return;}
        if(Op==TEXT("InvalidTarget")){const bool Accepted=S->BeginCinematicView(nullptr,nullptr);Done(Context.Token,Accepted?PresentationError(TEXT("Demo.Camera.InvalidAccepted"),TEXT("非法目标意外受理。")):PresentationError(TEXT("Demo.Camera.InvalidTarget"),TEXT("真实服务拒绝空目标，原视角应保持。")));return;}
        if(Op==TEXT("EndView")){Done(Context.Token,ResetStage());return;}
        if(S->IsCinematicViewActive()){Done(Context.Token,PresentationError(TEXT("Demo.Camera.LeaseBusy"),TEXT("已有电影机位，先EndView；不接管其他页面镜头。")));return;}
        const FVector Location=A.Location();const FRotator Rotation(0,A.Number(TEXT("Yaw")),0);const TWeakPtr<FPresentationManualState> Weak=AsShared();
        Load(A.Asset(TEXT("Profile")),[Weak,Location,Rotation](UObject* O)
        {auto Self=Weak.Pin();auto* Profile=Cast<UGamePlatformCinematicCameraProfile>(O);if(!Self||!Self->Alive()||!Profile||!Profile->IsValidProfile())return PresentationError(TEXT("Demo.Camera.Profile"),TEXT("真实资源不是合法CinematicCameraProfile。"));auto* C=Self->Local<UGamePlatformCinematicCameraSubsystem>();if(!C||C->IsCinematicViewActive())return PresentationError(TEXT("Demo.Camera.LeaseBusy"),TEXT("加载期间机位已被其他来源占用。"));
            auto* Stage=Self->World->SpawnActor<AGamePlatformCharacterPreviewStage>(Location,Rotation);if(!Stage)return PresentationError(TEXT("Demo.Camera.Spawn"),TEXT("本World机位创建失败。"));Self->Stage=Stage;
            const uint64 E=Self->Epoch;Self->bOwnView=true;const bool Started=C->BeginCinematicView(Stage,Profile);
            if(!Self->Current(E)){if(Started&&IsValid(Stage))C->EndCinematicViewFor(Stage);return FGamePlatformResult::Cancelled(TEXT("机位切换期间来源失活。"));}Self->bOwnView=Started;
            return Started?FGamePlatformResult::Success():PresentationError(TEXT("Demo.Camera.Begin"),TEXT("真实电影机位服务拒绝。"));},MoveTemp(Done));return;
    }
    auto* S=Local<UGamePlatformCameraHitFeedbackSubsystem>();if(!S){Done(Context.Token,PresentationError(TEXT("Demo.Camera.Missing"),TEXT("本地镜头反馈服务不可用。")));return;}
    if(Op==TEXT("Comfort")||Op==TEXT("Reset")){S->SetUserCameraShakeScale(Op==TEXT("Reset")?OriginalShakeScale:A.Number(TEXT("Comfort"),1));Done(Context.Token,Op==TEXT("Reset")?ReleaseAsset():FGamePlatformResult::Success());return;}
    if(Op==TEXT("Duplicate"))
    {const bool Accepted=S->PlayHitCameraShake(ShakeEvent,Cast<UClass>(A.Asset(TEXT("ShakeClass")).ResolveObject()),A.Number(TEXT("Strength"),.3));Done(Context.Token,Accepted?PresentationError(TEXT("Demo.Camera.DuplicateAccepted"),TEXT("重复镜头事件意外受理。")):PresentationError(TEXT("Demo.Camera.DuplicateRejected"),TEXT("真实反馈服务拒绝重复事件。")));return;}
    const TWeakPtr<FPresentationManualState> Weak=AsShared();const float Strength=A.Number(TEXT("Strength"),.3);
    Load(A.Asset(TEXT("ShakeClass")),[Weak,Strength](UObject* O)
    {auto Self=Weak.Pin();auto* Class=Cast<UClass>(O);if(!Self||!Self->Alive()||!Class||!Class->IsChildOf(UCameraShakeBase::StaticClass())||Class->HasAnyClassFlags(CLASS_Abstract))return PresentationError(TEXT("Demo.Camera.ShakeClass"),TEXT("需要已加载非抽象CameraShake类。"));
        FCameraShakeInfo Info;Class->GetDefaultObject<UCameraShakeBase>()->GetShakeInfo(Info);
        if(!Info.Duration.IsFixed()||!FMath::IsFinite(Info.Duration.Get())||Info.Duration.Get()<=0||Info.Duration.Get()>.5f)return PresentationError(TEXT("Demo.Camera.FiniteShakeRequired"),TEXT("真实ShakeInfo必须固定有限且0..0.5秒；不提交无限或自定义期限震动。"));
        auto* S=Self->Local<UGamePlatformCameraHitFeedbackSubsystem>();if(!S)return PresentationError(TEXT("Demo.Camera.Missing"),TEXT("本地反馈服务失效。"));Self->ShakeEvent=FGuid::NewGuid();const bool Accepted=S->PlayHitCameraShake(Self->ShakeEvent,Class,Strength);
        Self->Observe(TEXT("震动受理仅影响本LP镜头；有限时长由原生Shake资产结束，平台公开API没有逐实例Stop。"));return Accepted?FGamePlatformResult::Success():PresentationError(TEXT("Demo.Camera.ShakeRejected"),TEXT("真实反馈服务拒绝（舒适度、去重或频率限制）。"));},MoveTemp(Done));
}
}
