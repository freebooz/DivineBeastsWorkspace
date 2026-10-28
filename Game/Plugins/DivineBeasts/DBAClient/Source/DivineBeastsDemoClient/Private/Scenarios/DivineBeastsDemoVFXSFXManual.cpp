#include "Scenarios/DivineBeastsDemoPresentationManualState.h"
#include "Interfaces/GamePlatformVFXService.h"
#include "Interfaces/IGamePlatformSFXService.h"
#include "Interfaces/IGamePlatformSFXUserSettingsService.h"
#include "Components/GamePlatformMeshDissolveComponent.h"
#include "Preview/GamePlatformCharacterPreviewStage.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
namespace DivineBeasts::Demo
{
void FPresentationManualState::ExecuteVFX(FName Op,const FPresentationArgs& A,FDivineBeastsDemoOperationCompletion Done)
{
    if(Op==TEXT("LoadPreview")){ExecutePresentation(Op,A,MoveTemp(Done));return;}
    auto* S=IGamePlatformVFXService::Get(World.Get());
    if(!S){Done(Context.Token,PresentationError(TEXT("Demo.VFX.Missing"),TEXT("真实World不提供VFX服务。")));return;}
    if(Op==TEXT("Reset")){const auto R=ResetPlayback();if(auto* D=Dissolve.Get())D->ResetDissolve();Done(Context.Token,R);return;}
    if(Feature==TEXT("GamePlatformVFX.dissolve"))
    {
        auto* D=Dissolve.Get();if(!D){Done(Context.Token,PresentationError(TEXT("Demo.Dissolve.MeshMissing"),TEXT("先加载实现GP_Dissolve合同的真实Mesh/材质样板。")));return;}
        bool Accepted=true;
        if(Op==TEXT("RefreshMeshes"))D->RefreshDissolveMeshes();
        else if(Op==TEXT("Dissolve")||Op==TEXT("Duplicate"))
        {FGamePlatformMeshDissolveSettings V;V.DelaySeconds=A.Number(TEXT("Delay"),.65);V.DurationSeconds=A.Number(TEXT("Duration"),1.5);V.EdgeStrength=A.Number(TEXT("EdgeStrength"),1.5);
            if(Op==TEXT("Dissolve"))++DissolveGeneration;Accepted=D->StartDissolve(DissolveGeneration,V);}
        else Accepted=false;
        Done(Context.Token,Accepted?FGamePlatformResult::Success():PresentationError(TEXT("Demo.Dissolve.Rejected"),TEXT("真实溶解组件拒绝：检查重复代次及材质合同。")));return;
    }
    if(Op==TEXT("Disturb"))
    {FString Error;const int32 Count=S->DisturbFauna(A.Location(),A.Number(TEXT("Radius"),500),A.Number(TEXT("Strength"),.5),A.Number(TEXT("Duration"),2),Error);Observe(FString::Printf(TEXT("真实群落受影响=%d；CPU行为信息不代表GPU可见数量。"),Count));Done(Context.Token,Error.IsEmpty()?FGamePlatformResult::Success():PresentationError(TEXT("Demo.Fauna.Rejected"),*Error));return;}
    if(Op==TEXT("Snapshot")&&Feature==TEXT("GamePlatformVFX.fauna"))
    {FGamePlatformFaunaSnapshot Snapshot;bool Ok=S->GetFaunaSnapshot(A.Id(TEXT("ContextId")),Snapshot);Observe(Ok?TEXT("已取得本Context真实CPU群落快照，GPU可见数未知。"):TEXT("本Context无真实群落快照。"));Done(Context.Token,Ok?FGamePlatformResult::Success():PresentationError(TEXT("Demo.Fauna.NoSnapshot"),TEXT("没有对应Context群落。")));return;}
    if(Op==TEXT("InspectPreload"))
    {FString Error;const auto R=S->GetPreloadState(Preload,Error);Observe(FString::Printf(TEXT("显式查询本预载：State=%d；Ready只证明资源预载，未生成System。"),int32(R)));Done(Context.Token,(R==EGamePlatformVFXPreloadState::Failed||R==EGamePlatformVFXPreloadState::Invalid)?PresentationError(TEXT("Demo.VFX.PreloadState"),*Error):FGamePlatformResult::Success());return;}
    if(Op==TEXT("CancelPreload"))
    {const bool Accepted=S->CancelPreload(Preload);if(Accepted)Preload={};Done(Context.Token,Accepted?FGamePlatformResult::Success():PresentationError(TEXT("Demo.VFX.PreloadCancel"),TEXT("本预载取消被拒绝，保留完整句柄。")));return;}
    if(Op==TEXT("Snapshot"))
    {if(!VFXHandles.IsEmpty()){const auto R=S->GetPlaybackSnapshot(VFXHandles.Last());Observe(FString::Printf(TEXT("VFX显式快照 State=%d Code=%d；受理/Playing/画面/GPU分开。"),int32(R.State),int32(R.Code)));Done(Context.Token,FGamePlatformResult::Success());}else Done(Context.Token,PresentationError(TEXT("Demo.VFX.NoHandle"),TEXT("当前无本运行实例句柄。")));return;}
    const bool New=Op==TEXT("Play")||Op==TEXT("Predict")||Op==TEXT("Preload");
    if(New)
    {
        if(VFXHandles.Num()>=64){Done(Context.Token,PresentationError(TEXT("Demo.VFX.OwnedLimit"),TEXT("本页最多持64句柄；先Reset。")));return;}
        LastVFX={};LastVFX.RequestId=FGuid::NewGuid();LastVFX.ActivationId=Context.Token.RunId;LastVFX.ContextId=A.Id(TEXT("ContextId"));
        LastVFX.DefinitionId=A.Definition();LastVFX.QualityTier=static_cast<EGamePlatformVFXQualityTier>(A.Int(TEXT("Quality"),2));
        LastVFX.SpawnContext.Location=A.Location();LastVFX.SpawnContext.TargetLocation=A.Location()+FVector(A.Number(TEXT("TargetOffset"),300),0,0);
        LastVFX.SpawnContext.ImpactLocation=A.Location();LastVFX.SpawnContext.Rotation=FRotator(0,A.Number(TEXT("Yaw")),0);
        if(Stage.IsValid())LastVFX.SpawnContext.AttachComponent=Stage->GetRootComponent();
        else if(auto* PC=Player->GetPlayerController(World.Get()))if(APawn* Pawn=PC->GetPawn())LastVFX.SpawnContext.AttachComponent=Pawn->GetRootComponent();
        LastVFX.PredictionState=Op==TEXT("Predict")?EGamePlatformVFXPredictionState::Predicted:EGamePlatformVFXPredictionState::None;
        VFXRequests.Add(LastVFX.RequestId);
    }
    else if(!LastVFX.RequestId.IsValid()){Done(Context.Token,PresentationError(TEXT("Demo.VFX.NoRequest"),TEXT("先创建本次播放/预测请求。")));return;}
    if(Op==TEXT("Preload"))
    {if(Preload.IsValid()){Done(Context.Token,PresentationError(TEXT("Demo.VFX.PreloadOwned"),TEXT("先取消上一份预载。")));return;}Preload=S->Preload(LastVFX);Done(Context.Token,Preload.IsValid()?FGamePlatformResult::Success():PresentationError(TEXT("Demo.VFX.PreloadRejected"),TEXT("真实预载拒绝。")));return;}
    if(Op==TEXT("Confirm"))LastVFX.PredictionState=EGamePlatformVFXPredictionState::Confirmed;
    if(Op==TEXT("Correct"))LastVFX.PredictionState=EGamePlatformVFXPredictionState::Corrected;
    if(Op==TEXT("Cancel"))LastVFX.PredictionState=EGamePlatformVFXPredictionState::Cancelled;
    const uint64 E=Epoch;const auto Result=S->Play(LastVFX);
    if(!Current(E)){if(Result.Handle.IsValid()&&World.IsValid()&&!World->bIsTearingDown)if(auto* Live=IGamePlatformVFXService::Get(World.Get());Live==S)Live->Stop(Result.Handle);Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("播放同步重入期间来源失活。")));return;}
    if(Result.Handle.IsValid())VFXHandles.AddUnique(Result.Handle);
    Observe(FString::Printf(TEXT("VFX提交 Code=%d；Handle不等于可见或GPU预算通过。"),int32(Result.Code)));
    Done(Context.Token,Result.IsAccepted()?FGamePlatformResult::Success():(Result.Code==EGamePlatformVFXResultCode::Cancelled?FGamePlatformResult::Cancelled(TEXT("真实VFX取消已接纳。")):PresentationError(TEXT("Demo.VFX.Rejected"),TEXT("唯一VFX执行链拒绝，查看真实结果码与终态。"))));
}
void FPresentationManualState::ExecuteSFX(FName Op,const FPresentationArgs& A,FDivineBeastsDemoOperationCompletion Done)
{
    if(Feature==TEXT("GamePlatformSFX.user_volume"))
    {
        auto* S=Instance()?IGamePlatformSFXUserSettingsService::Get(*Instance()):nullptr;
        if(!S){Done(Context.Token,PresentationError(TEXT("Demo.SFX.SettingsMissing"),TEXT("本GI音量服务不可用。")));return;}
        if(Op==TEXT("Commit")){Done(Context.Token,S->CommitVolumes());return;}
        if(Op==TEXT("Reset"))
        {const uint64 E=Epoch;auto R=S->SetVolume(EGamePlatformSFXSoundGroup::Effects,OriginalVolumes.Effects);if(Current(E)&&R.IsSuccess())R=S->SetVolume(EGamePlatformSFXSoundGroup::Ambient,OriginalVolumes.Ambient);if(Current(E)&&R.IsSuccess())R=S->SetVolume(EGamePlatformSFXSoundGroup::Music,OriginalVolumes.Music);Done(Context.Token,R);return;}
        Done(Context.Token,S->SetVolume(static_cast<EGamePlatformSFXSoundGroup>(A.Int(TEXT("Group"))),A.Number(TEXT("Gain"),1)));return;
    }
    auto* S=IGamePlatformSFXService::Get(World.Get());if(!S){Done(Context.Token,PresentationError(TEXT("Demo.SFX.Missing"),TEXT("当前World不提供音效服务。")));return;}
    if(Op==TEXT("Reset")){Done(Context.Token,ResetPlayback());return;}
    if(Op==TEXT("Snapshot"))
    {if(SFXHandles.IsEmpty()){Done(Context.Token,PresentationError(TEXT("Demo.SFX.NoHandle"),TEXT("当前无本运行音效。")));return;}const auto R=S->GetPlaybackSnapshot(SFXHandles.Last());Observe(FString::Printf(TEXT("SFX State=%d Code=%d；可听输出等待人耳验收。"),int32(R.State),int32(R.Code)));Done(Context.Token,FGamePlatformResult::Success());return;}
    if(Op==TEXT("SetParameter")||Op==TEXT("SetGain"))
    {const bool Accepted=!SFXHandles.IsEmpty()&&(Op==TEXT("SetParameter")?S->SetFloatParameter(SFXHandles.Last(),A.Id(TEXT("Parameter")),A.Number(TEXT("Value"))):S->SetVolumeMultiplier(SFXHandles.Last(),A.Number(TEXT("Gain"),1)));Done(Context.Token,Accepted?FGamePlatformResult::Success():PresentationError(TEXT("Demo.SFX.ParameterRejected"),TEXT("检查活动真实组件、参数白名单与范围；Loading不能假报参数已应用。")));return;}
    if(Op==TEXT("Play")||Op==TEXT("Predict"))
    {
        if(SFXHandles.Num()>=64){Done(Context.Token,PresentationError(TEXT("Demo.SFX.OwnedLimit"),TEXT("本页最多持64句柄，先Reset。")));return;}
        LastSFX={};LastSFX.RequestId=FGuid::NewGuid();LastSFX.DefinitionId=A.Definition();LastSFX.Location=A.Location();
        LastSFX.ContextId=FName(*Context.Token.RunId.ToString());LastSFX.VolumeMultiplier=A.Number(TEXT("Gain"),1);LastSFX.PitchMultiplier=A.Number(TEXT("Pitch"),1);LastSFX.StartTimeSeconds=A.Number(TEXT("StartTime"));
        // 2D/World/Attached由真实Definition决定，不能通过UI参数伪造空间覆盖。
        if(auto* PC=Player->GetPlayerController(World.Get()))if(APawn* Pawn=PC->GetPawn())LastSFX.AttachComponent=Pawn->GetRootComponent();
        LastSFX.AttachPointName=A.Id(TEXT("Socket"))==TEXT("Default")?NAME_None:A.Id(TEXT("Socket"));LastSFX.PredictionState=Op==TEXT("Predict")?EGamePlatformSFXPredictionState::Predicted:EGamePlatformSFXPredictionState::None;
        SFXRequests.Add(LastSFX.RequestId);
    }
    else if(!LastSFX.RequestId.IsValid()){Done(Context.Token,PresentationError(TEXT("Demo.SFX.NoRequest"),TEXT("先创建本运行播放/预测请求。")));return;}
    if(Op==TEXT("Cancel")){const bool Accepted=S->StopByRequestId(LastSFX.RequestId,0);Done(Context.Token,Accepted?FGamePlatformResult::Success():PresentationError(TEXT("Demo.SFX.CancelRejected"),TEXT("本请求取消未被接纳。")));return;}
    if(Op==TEXT("Confirm"))LastSFX.PredictionState=EGamePlatformSFXPredictionState::Confirmed;
    if(Op==TEXT("Correct"))LastSFX.PredictionState=EGamePlatformSFXPredictionState::Corrected;
    const uint64 E=Epoch;const auto R=S->Play(LastSFX);
    if(!Current(E)){if(R.Handle.IsValid()&&World.IsValid()&&!World->bIsTearingDown)if(auto* Live=IGamePlatformSFXService::Get(World.Get());Live==S)Live->Stop(R.Handle,0);Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("音效同步重入期间来源失活。")));return;}
    if(R.Handle.IsValid()&&!SFXHandles.ContainsByPredicate([&](const auto& H){return PresentationManualSameSFXHandle(H,R.Handle);}))SFXHandles.Add(R.Handle);
    Observe(FString::Printf(TEXT("SFX提交 Code=%d；Definition决定2D/World/Attached，Submitted/Playing不能代替实际听觉。"),int32(R.Code)));
    Done(Context.Token,R.IsAccepted()?FGamePlatformResult::Success():PresentationError(TEXT("Demo.SFX.Rejected"),TEXT("真实音效执行链拒绝，查看结果码。")));
}
}
