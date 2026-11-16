#include "Scenarios/DivineBeastsDemoGameplayHostMode.h"
#include "Scenarios/DivineBeastsDemoManualHostSubsystem.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#include "Gameplay/DivineBeastsWorldPlayerController.h"
#include "Components/GamePlatformExperienceComponent.h"
#include "Framework/GamePlatformPlayerControllerBase.h"
#include "Framework/GamePlatformPlayerStateBase.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
namespace {
FGamePlatformResult HostError(const TCHAR* C,const TCHAR* M){return FGamePlatformResult::Failure(C,M);}
class FOfflineFixtureAuthority final:public IGamePlatformGameplayAdmissionAuthority {
 TWeakObjectPtr<ADivineBeastsDemoGameplayHostMode> Mode;
public:explicit FOfflineFixtureAuthority(ADivineBeastsDemoGameplayHostMode& M):Mode(&M){}
 FGamePlatformResult ValidateCurrentAdmission(const APlayerController& C,const FGamePlatformVerifiedPlayerContext& V)const override{return Mode.IsValid()?Mode->ValidateFixtureAdmission(C,V):HostError(TEXT("Demo.Gameplay.AuthorityExpired"),TEXT("离线夹具权威已退出。"));}
};
class FOfflineFixtureSpawn final:public IGamePlatformSpawnPolicy {
 TWeakObjectPtr<ADivineBeastsDemoGameplayHostMode> Mode;TWeakObjectPtr<AActor> Anchor;
public:FOfflineFixtureSpawn(ADivineBeastsDemoGameplayHostMode& M,AActor& A):Mode(&M),Anchor(&A){}
 FGamePlatformResult CollectCandidates(const FGamePlatformSpawnRequest& Q,TArray<FGamePlatformSpawnCandidate>& Out)override{
  Out.Reset();auto* M=Mode.Get();if(!M||!Anchor.IsValid()||!M->IsExplicitOfflineHost()||Q.World.Get()!=M->GetWorld()||Q.Controller.Get()!=M->GetFixtureController()||!Q.OperationId.IsValid())return HostError(TEXT("Demo.Gameplay.SpawnSource"),TEXT("只向本离线运行的真实Controller提供候选。"));
  FGamePlatformSpawnCandidate C;C.CandidateId=TEXT("DemoGameplayFixture.Candidate");C.Source=Anchor;C.Transform=FTransform(FRotator::ZeroRotator,FVector(0,500,110));Out.Add(C);return FGamePlatformResult::Success();
 }
};
}
ADivineBeastsDemoGameplayHostMode::ADivineBeastsDemoGameplayHostMode(){PlayerControllerClass=ADivineBeastsWorldPlayerController::StaticClass();}
bool ADivineBeastsDemoGameplayHostMode::IsExplicitOfflineHost()const{
 const auto* W=GetWorld();auto* A=FCommandLine::Get();return !UE_BUILD_SHIPPING&&!UE_BUILD_TEST&&!IsRunningCommandlet()&&FDivineBeastsPluginDemoMode::IsHubPageEnabled()&&W&&W->WorldType==EWorldType::Game&&W->GetNetMode()==NM_Standalone&&W->GetAuthGameMode()==this&&W->GetOutermost()->GetName()==FDivineBeastsPluginDemoMode::GetHubMapPackage()&&FParse::Param(A,TEXT("GamePlatformManualFixture"))&&FParse::Param(A,TEXT("DBAManualGameplayHost"))&&FParse::Param(A,TEXT("FoundationGameplayOffline"))&&FParse::Param(A,TEXT("FoundationWorld"))&&FParse::Param(A,TEXT("DBAManualWorldHost"));
}
bool ADivineBeastsDemoGameplayHostMode::OwnsFixture(const FDivineBeastsDemoRunToken& T)const{return IsExplicitOfflineHost()&&Owner.IsValid()&&Owner==T;}
FGamePlatformResult ADivineBeastsDemoGameplayHostMode::AcquireFixture(const FDivineBeastsDemoRunToken& T){
 if(!IsExplicitOfflineHost()||!T.IsValid()||Owner.IsValid()||bOpening||bClosingFixture)return HostError(TEXT("Demo.Gameplay.OfflineHostRequired"),TEXT("需精确Hub ?game=本私有Mode、完整离线标志及新运行；不切换现有World的Mode。"));
 auto* Exclusive=GetGameInstance()->GetSubsystem<UDivineBeastsDemoManualHostSubsystem>();auto* S=IGamePlatformWorldService::Get(*GetWorld());if(!Exclusive||!Exclusive->Owns(T)||!S)return HostError(TEXT("Demo.Gameplay.OfflineOwner"),TEXT("原ManualHost或真实World服务未就绪。"));
 const auto W=S->GetReadiness();if(!W.Context.ContextGeneration.IsValid())return HostError(TEXT("Demo.Gameplay.WorldIdentity"),TEXT("先由既有WorldHost完成真实开发World初始化。"));
 TGuardValue<bool> Opening(bOpening,true);Owner=T;bAdmitted=false;bExperience=false;WorldGeneration=W.Context.ContextGeneration;FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;
 // All candidates are owned immediately, including partial failure. ReleaseFixture remains available.
 SpawnAnchor=GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),FVector(0,500,110),FRotator::ZeroRotator,P);if(!SpawnAnchor.IsValid())return HostError(TEXT("Demo.Gameplay.Anchor"),TEXT("自有出生候选载体创建失败。"));
 FixtureController=GetWorld()->SpawnActor<AGamePlatformPlayerControllerBase>(AGamePlatformPlayerControllerBase::StaticClass(),FVector::ZeroVector,FRotator::ZeroRotator,P);if(!FixtureController.IsValid())return HostError(TEXT("Demo.Gameplay.Controller"),TEXT("自有离线Controller创建失败。"));
 if(!FixtureController->PlayerState)FixtureController->InitPlayerState();auto* PS=FixtureController->GetPlayerState<AGamePlatformPlayerStateBase>();if(!PS)return HostError(TEXT("Demo.Gameplay.PlayerState"),TEXT("真实平台PlayerState未创建。"));
 LifecycleChanged=PS->OnLifecycleChanged.AddLambda([Weak=TWeakObjectPtr<ThisClass>(this)]{if(auto* M=Weak.Get())if(M->Owner.IsValid()&&!M->bClosingFixture)M->OnFixtureChanged.Broadcast();});
 FGamePlatformResult R;AuthorityRegistration=RegisterAdmissionAuthority(this,MakeShared<FOfflineFixtureAuthority>(*this),R);if(!R.IsSuccess())return R;
 SpawnRegistration=RegisterSpawnPolicy(TEXT("DemoGameplayFixture"),this,MakeShared<FOfflineFixtureSpawn>(*this,*SpawnAnchor.Get()),R);if(!R.IsSuccess())return R;
 Admission.AdmissionId=FGuid::NewGuid();FGamePlatformId::TryParse(TEXT("participant.demo.gameplay@1"),Admission.ParticipantId);FGamePlatformId::TryParse(TEXT("experience.demo.gameplay@1"),Admission.ExperienceId);Admission.AssignmentId=TEXT("OfflineManualFixture-")+T.RunId.ToString();Admission.ServerInstanceId=W.Context.ServerInstanceId.IsEmpty()?TEXT("OfflineManualFixture"):W.Context.ServerInstanceId;Admission.ServerStartGeneration=FMath::Max<uint64>(1,W.Context.ServerStartGeneration);Admission.ConnectionGeneration=T.Generation;Admission.SessionEpoch=T.Generation;
 return FGamePlatformResult::Success();
}
FGamePlatformResult ADivineBeastsDemoGameplayHostMode::ValidateFixtureAdmission(const APlayerController& C,const FGamePlatformVerifiedPlayerContext& V)const{
 auto* H=GetGameInstance()?GetGameInstance()->GetSubsystem<UDivineBeastsDemoManualHostSubsystem>():nullptr;auto* S=GetWorld()?IGamePlatformWorldService::Get(*GetWorld()):nullptr;
 bool B=IsExplicitOfflineHost()&&Owner.IsValid()&&!bClosingFixture&&H&&H->Owns(Owner)&&S&&S->GetReadiness().Context.ContextGeneration==WorldGeneration&&FixtureController.Get()==&C&&C.GetWorld()==GetWorld()&&C.HasAuthority()&&V.IsStructurallyValid()&&V.AdmissionId==Admission.AdmissionId&&V.ParticipantId==Admission.ParticipantId&&V.AssignmentId==Admission.AssignmentId&&V.ServerInstanceId==Admission.ServerInstanceId&&V.ServerStartGeneration==Admission.ServerStartGeneration&&V.ConnectionGeneration==Admission.ConnectionGeneration&&V.SessionEpoch==Admission.SessionEpoch&&V.ExperienceId==Admission.ExperienceId&&V.PawnDefinitionId==Admission.PawnDefinitionId;
 return B?FGamePlatformResult::Success():HostError(TEXT("Demo.Gameplay.FixtureAdmissionRejected"),TEXT("只认可本Mode、完整Run、原Controller和本地离线登记；无生产Session/票据/后端准入含义。"));
}
FGamePlatformResult ADivineBeastsDemoGameplayHostMode::BeginFixtureExperience(const FDivineBeastsDemoRunToken& T){if(!OwnsFixture(T))return HostError(TEXT("Demo.Gameplay.Owner"),TEXT("完整运行身份不匹配。"));auto* E=GetExperienceComponent();if(!E)return HostError(TEXT("Demo.Gameplay.Experience"),TEXT("真实GameState组件不存在。"));auto R=E->BeginExperience(FPrimaryAssetId(TEXT("GamePlatformDefinition:experience.demo.gameplay@1")),true);if(R.IsSuccess())bExperience=true;return R;}
FGamePlatformResult ADivineBeastsDemoGameplayHostMode::DrainFixtureExperience(const FDivineBeastsDemoRunToken& T){if(!OwnsFixture(T))return HostError(TEXT("Demo.Gameplay.Owner"),TEXT("完整运行身份不匹配。"));auto* E=GetExperienceComponent();if(!E)return HostError(TEXT("Demo.Gameplay.Experience"),TEXT("真实Experience失效。"));return E->BeginExperienceDrain(TEXT("ManualFixtureDrain"));}
FGamePlatformResult ADivineBeastsDemoGameplayHostMode::SubmitFixtureAdmission(const FDivineBeastsDemoRunToken& T,bool Stale){if(!OwnsFixture(T)||!FixtureController.IsValid())return HostError(TEXT("Demo.Gameplay.Owner"),TEXT("原自有Controller不存在。"));auto V=Admission;if(Stale)++V.ConnectionGeneration;auto R=SubmitVerifiedAdmission(*FixtureController.Get(),V);if(R.IsSuccess())bAdmitted=true;return R;}
FGamePlatformResult ADivineBeastsDemoGameplayHostMode::ConfirmFixturePreparation(const FDivineBeastsDemoRunToken& T,bool Stale){if(!OwnsFixture(T)||!FixtureController.IsValid())return HostError(TEXT("Demo.Gameplay.Owner"),TEXT("原自有Controller不存在。"));auto Token=FixtureController->GetPreparationToken();if(Stale)Token.TokenId=FGuid::NewGuid();return AcceptPreparation(*FixtureController.Get(),Token);}
FGamePlatformResult ADivineBeastsDemoGameplayHostMode::RevokeFixtureAdmission(const FDivineBeastsDemoRunToken& T){if(!OwnsFixture(T)||!FixtureController.IsValid())return HostError(TEXT("Demo.Gameplay.Owner"),TEXT("原Controller失效。"));if(!bAdmitted)return FGamePlatformResult::Success();FGamePlatformAdmissionRevocation R;R.AdmissionId=Admission.AdmissionId;R.ConnectionGeneration=Admission.ConnectionGeneration;R.SessionEpoch=Admission.SessionEpoch;auto Result=RevokeVerifiedAdmission(*FixtureController.Get(),R);if(Result.IsSuccess())bAdmitted=false;return Result;}
FGamePlatformResult ADivineBeastsDemoGameplayHostMode::ReleaseFixture(const FDivineBeastsDemoRunToken& T){
 if(!Owner.IsValid())return FGamePlatformResult::Success();if(!(Owner==T)||bOpening||bClosingFixture)return HostError(TEXT("Demo.Gameplay.HostBusy"),TEXT("原生Host栈在途或身份不匹配；保留资源，Stop重试。"));TGuardValue<bool> Closing(bClosingFixture,true);
 if(bExperience){auto* E=GetExperienceComponent();if(!E)return HostError(TEXT("Demo.Gameplay.ExperienceLost"),TEXT("保留体验所有权。"));auto R=E->BeginExperienceDrain(TEXT("ManualFixtureStop"));if(!R.IsSuccess())return R;bExperience=false;bAdmitted=false;}
 if(AuthorityRegistration.IsValid()){if(!UnregisterAdmissionAuthority(AuthorityRegistration))return HostError(TEXT("Demo.Gameplay.AuthorityCleanup"),TEXT("保留完整Authority句柄。"));AuthorityRegistration={};}
 if(SpawnRegistration.IsValid()){if(!UnregisterSpawnPolicy(SpawnRegistration))return HostError(TEXT("Demo.Gameplay.SpawnCleanup"),TEXT("保留完整SpawnPolicy句柄。"));SpawnRegistration={};}
 if(FixtureController.IsValid()){auto* C=FixtureController.Get();auto* PS=C->GetPlayerState<AGamePlatformPlayerStateBase>();if(PS)PS->OnLifecycleChanged.Remove(LifecycleChanged);if(APawn* Pawn=C->GetPawn())if(!Pawn->Destroy())return HostError(TEXT("Demo.Gameplay.PawnCleanup"),TEXT("自有Spawn Pawn尚未销毁。"));if(PS&&!PS->Destroy())return HostError(TEXT("Demo.Gameplay.StateCleanup"),TEXT("自有PlayerState尚未销毁。"));if(!C->Destroy())return HostError(TEXT("Demo.Gameplay.ControllerCleanup"),TEXT("自有Controller尚未销毁。"));FixtureController.Reset();}
 if(SpawnAnchor.IsValid()){if(!SpawnAnchor->Destroy())return HostError(TEXT("Demo.Gameplay.AnchorCleanup"),TEXT("自有Anchor未销毁。"));SpawnAnchor.Reset();}Owner={};Admission={};bAdmitted=false;bExperience=false;WorldGeneration.Invalidate();return FGamePlatformResult::Success();
}
FString ADivineBeastsDemoGameplayHostMode::FixtureObservation()const{auto* C=FixtureController.Get();auto* PS=C?C->GetPlayerState<AGamePlatformPlayerStateBase>():nullptr;auto V=PS?PS->GetLifecycleSnapshot():FGamePlatformPlayerLifecycleSnapshot();return FString::Printf(TEXT("OfflineFixtureController=%d PlayerStage=%d SpawnGen=%lld Pawn=%s Authority=%d Policy=%d；本地真实平台生命周期，不是生产准入。"),C!=nullptr,int32(V.Stage),V.SpawnGeneration,*GetNameSafe(C?C->GetPawn():nullptr),AuthorityRegistration.IsValid(),SpawnRegistration.IsValid());}
void ADivineBeastsDemoGameplayHostMode::EndPlay(const EEndPlayReason::Type R){if(Owner.IsValid())ReleaseFixture(Owner);Super::EndPlay(R);}
