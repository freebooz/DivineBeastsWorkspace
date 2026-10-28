#include "Scenarios/DivineBeastsDemoGameplayState.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Interfaces/IGamePlatformAbilityActivationGate.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Components/GamePlatformAIStateComponent.h"
#include "Components/GamePlatformAITargetComponent.h"
#include "Components/GamePlatformExperienceComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Actors/GamePlatformCombatProjectile.h"
#include "Services/GamePlatformQuestClientSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Feedback/GamePlatformLocalHitstopSubsystem.h"
#include "Feedback/GamePlatformHitFlashWorldSubsystem.h"
#include "Locomotion/GamePlatformLocomotionAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Async/Async.h"
#include "Scenarios/DivineBeastsDemoGameplayHostMode.h"
namespace DivineBeasts::Demo {
bool FGameplayManualState::Alive()const{return bActive&&!bCleaning&&World.IsValid()&&!World->bIsTearingDown&&Player.IsValid()&&Player->GetWorld()==World.Get()&&Host.IsValid()&&Host->Owns(Context.Token);}
IGamePlatformDataService* FGameplayManualState::Data()const{return World.IsValid()&&World->GetGameInstance()?IGamePlatformDataService::Get(*World->GetGameInstance()):nullptr;}
void FGameplayManualState::Observe(const FString& Text)const{if(Alive()&&Context.PublishObservation)Context.PublishObservation(Context.Token,Text.Left(4096));}
void FGameplayManualState::Snapshot()const{
 FString T=FString::Printf(TEXT("Run=%s；隔离Standalone权威夹具，不等于生产准入/联网/视听签收。Feature=%s"),*Context.Token.RunId.ToString(),*Feature.ToString());
 for(auto F:{Source,Target,Alternate})if(F.IsValid()){
  auto B=F->ASC->GetAvatarBindingSnapshot();auto E=F->Eligibility->GetSnapshot();auto C=F->CastFacts->GetCastFacts();auto* A=F->Combat->GetCombatAttributeSet();
  T+=FString::Printf(TEXT(" [%s Auth=%d Active=%d Health=%.2f Energy=%.2f CombatGen=%d ASCGen=%d Bound=%d Cast=%s/%d Pos=%s TargetEligible=%d TargetGen=%d Commit=%d AIState=%d]"),*F->GetName(),F->HasAuthority(),E.bActive,F->Combat->GetCombatHealth(),A?A->GetEnergy():-1,F->GetCombatAvatarGeneration(),B.AvatarGeneration,B.bBound,*C.ActivationId.ToString(),C.bActive,*F->GetActorLocation().ToCompactString(),F->AITarget->IsEligibleAsAITarget(nullptr),F->AITarget->GetAITargetGeneration(),F->CommittedInputs,int32(F->AIState->GetSnapshot().PublicState));
 }
 if(Projectile.IsValid()){const auto& P=Projectile->GetFlightFacts();T+=FString::Printf(TEXT(" Projectile=%s Started=%d Finished=%d TrueCollision=%d Pos=%s"),*P.OccurrenceId.ToString(),P.bStarted,P.bFinished,P.bImpacted,*Projectile->GetActorLocation().ToCompactString());}
 if(Source.IsValid()){if(auto* A=Cast<UGamePlatformLocomotionAnimInstance>(Source->GetMesh()->GetAnimInstance()))T+=FString::Printf(TEXT(" AnimSpeed=%.2f Vertical=%.2f Falling=%d Grounded=%d"),A->GroundSpeedCmPerSecond,A->VerticalSpeedCmPerSecond,A->bIsFalling,A->bIsGrounded);if(auto* H=Player->GetSubsystem<UGamePlatformLocalHitstopSubsystem>())T+=FString::Printf(TEXT(" VisualHitstop=%d"),H->IsVisualHitstopActive(Source->GetMesh()));}
 if(auto* H=World->GetSubsystem<UGamePlatformHitFlashWorldSubsystem>())T+=FString::Printf(TEXT(" WorldFlashCount=%d (contains foreign effects; not own-count)"),H->GetActiveFlashCount());
 if(Experience.IsValid()){auto E=Experience->GetExperienceSnapshot();T+=FString::Printf(TEXT(" ExperienceStage=%d Epoch=%lld Revision=%lld"),int32(E.Stage),E.ExperienceEpoch,E.StateRevision);}
 if(GameplayHost.IsValid())T+=GameplayHost->FixtureObservation();
 T+=FString::Printf(TEXT(" DefinitionSelection=%s SelectionGen=%llu DefinitionStatus=%d Code=%s"),*DefinitionSelection.ToString(),DefinitionSelectionGeneration,int32(DefinitionResult.Status),*DefinitionResult.Code.ToString());
 if(CharacterIdentity.IsValid())T+=FString::Printf(TEXT(" CharacterReady=%d DefinitionVersion=%d LoadStatus=%d LoadCode=%s"),CharacterIdentity->IsCharacterReady(),CharacterIdentity->GetDefinitionVersion(),int32(CharacterIdentity->GetLastDefinitionLoadResult().Status),*CharacterIdentity->GetLastDefinitionLoadResult().Code.ToString());
 if(auto* Q=Player.IsValid()?Player->GetSubsystem<UGamePlatformQuestClientSubsystem>():nullptr)T+=FString::Printf(TEXT(" QuestCache=%d Tracked=%d；只读权威投影，绝不合成快照。"),Q->GetSortedSnapshotsView().Num(),Q->GetTrackedQuestIds().Num());
 Observe(T);
}
FGamePlatformResult FGameplayManualState::Prepare(){
 auto* W=Cast<UUserWidget>(Context.SampleWidget.Get());Player=W?W->GetOwningLocalPlayer():nullptr;
 if(!World.IsValid()||!World->IsGameWorld()||!World->HasBegunPlay()||World->GetNetMode()!=NM_Standalone||!World->GetAuthGameMode()||!Player.IsValid()||Player->GetWorld()!=World.Get()||!Data())return GameplayError(TEXT("Demo.Gameplay.Host"),TEXT("需同GI/LP Standalone GameWorld、真实Widget和Data；不初始化替代子系统。"));
 Host=World->GetGameInstance()->GetSubsystem<UDivineBeastsDemoManualHostSubsystem>();if(!Host.IsValid())return GameplayError(TEXT("Demo.Gameplay.ManualHost"),TEXT("缺显式DemoMode+ManualFixture。"));
 auto R=Host->Acquire(Context.Token,*World);if(!R.IsSuccess())return R;bHost=true;bActive=true;
 Observer.Reset(NewObject<UDivineBeastsDemoGameplayObserver>(World->GetGameInstance()));auto Weak=TWeakPtr<FGameplayManualState>(AsShared());const auto E=Epoch;
 Observer->Changed=[Weak,E]{if(auto S=Weak.Pin())if(S->Current(E))S->Snapshot();};
 WorldCleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Weak](UWorld* W,bool,bool){if(auto S=Weak.Pin())if(S->World.Get()==W){S->bActive=false;++S->Epoch;S->Cleanup();}});
 if(auto* Q=Player->GetSubsystem<UGamePlatformQuestClientSubsystem>())QuestChanged=Q->OnChanged.AddLambda([Weak,E]{if(auto S=Weak.Pin())if(S->Current(E))S->Snapshot();});
 if(auto* H=Player->GetSubsystem<UGamePlatformLocalHitstopSubsystem>())HitstopFinished=H->OnVisualHitstopFinished().AddLambda([Weak,E](auto*){if(auto S=Weak.Pin())if(S->Current(E))S->Snapshot();});
 Snapshot();return Alive()?R:FGamePlatformResult::Cancelled(TEXT("初次观察期间已停止。"));
}
struct FManualFixtureGate final:IGamePlatformAbilityActivationGate {
 TWeakObjectPtr<ADivineBeastsDemoGameplayFixture> Fixture;
 explicit FManualFixtureGate(ADivineBeastsDemoGameplayFixture& F):Fixture(&F){}
 FGamePlatformResult Evaluate(const UGamePlatformAbilitySystemComponent& C)const override{
  auto* F=Fixture.Get();return F&&F->ASC==&C&&F->HasAuthority()&&C.GetAvatarActor()==F&&F->CanSourceCombat()&&F->Eligibility->GetGameplayAvatarGeneration()==F->GetCombatAvatarGeneration()?FGamePlatformResult::Success():GameplayError(TEXT("Demo.Gameplay.FixtureInactive"),TEXT("本夹具未Active、死亡或代次不一致；此Gate不授予生产准入。"));
 }
};
FGamePlatformResult FGameplayManualState::BindFixtureAvatar(){if(!Source.IsValid())return GameplayError(TEXT("Demo.Gameplay.Fixture"),TEXT("先创建夹具。"));const auto E=Epoch;if(!Source->ASC->BindAbilityActorInfo(Source.Get(),Source.Get())||!Current(E))return GameplayError(TEXT("Demo.Gameplay.Binding"),TEXT("绑定未提交或来源失活。"));return Source->ASC->SetActivationGate(Source.Get(),MakeShared<FManualFixtureGate>(*Source.Get()));}
FGamePlatformResult FGameplayManualState::CreateFixture(){
 if(Source.IsValid()||Target.IsValid()||Alternate.IsValid())return GameplayError(TEXT("Demo.Gameplay.Exists"),TEXT("夹具已存在；先Reset。"));
 const auto E=Epoch;int32 Index=0;
 for(auto* Slot:{&Source,&Target,&Alternate}){
  FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
  auto* F=World->SpawnActor<ADivineBeastsDemoGameplayFixture>(ADivineBeastsDemoGameplayFixture::StaticClass(),FVector(-350+350*Index,100,100),FRotator::ZeroRotator,P);*Slot=F;
  if(!Current(E)||!F)return GameplayError(TEXT("Demo.Gameplay.Spawn"),TEXT("真实角色出生失败/来源已停；保留已有对象等待Stop清理。"));
  if(!F->ASC->BindAbilityActorInfo(F,F)||!Current(E))return GameplayError(TEXT("Demo.Gameplay.ActorInfo"),TEXT("ASC绑定失败。"));
  auto R=F->ASC->SetActivationGate(F,MakeShared<FManualFixtureGate>(*F));if(!R.IsSuccess()||!Current(E))return R;
  const auto Weak=TWeakPtr<FGameplayManualState>(AsShared());
  FFixtureSubscriptions Sub;Sub.Actor=F;
  Sub.Binding=F->ASC->OnAvatarBindingChanged().AddLambda([Weak,E](const auto&){if(auto S=Weak.Pin())if(S->Current(E))S->Snapshot();});
  Sub.Eligibility=F->Eligibility->OnEligibilityChanged().AddLambda([Weak,E](const auto&){if(auto S=Weak.Pin())if(S->Current(E))S->Snapshot();});
  Sub.Combat=F->Combat->OnCombatObservationChanged().AddLambda([Weak,E]{if(auto S=Weak.Pin())if(S->Current(E))S->Snapshot();});
  Sub.Cast=F->CastFacts->OnCastFactsChanged().AddLambda([Weak,E](auto*,const auto&){if(auto S=Weak.Pin())if(S->Current(E))S->Snapshot();});
  Sub.Transform=F->GetRootComponent()->TransformUpdated.AddLambda([Weak,E](auto*,auto,auto){if(auto S=Weak.Pin())if(S->Current(E))S->Snapshot();});FixtureSubscriptions.Add(Sub);
  F->AIState->OnStateChanged.AddUniqueDynamic(Observer.Get(),&UDivineBeastsDemoGameplayObserver::OnAIChanged);
  TArray<FDelegateHandle> H;for(auto A:{UGamePlatformCombatAttributeSet::GetHealthAttribute(),UGamePlatformCombatAttributeSet::GetEnergyAttribute()})H.Add(F->ASC->GetGameplayAttributeValueChangeDelegate(A).AddLambda([Weak,E](const auto&){if(auto S=Weak.Pin())if(S->Current(E))S->Snapshot();}));AttributeHandlers.Add({TWeakObjectPtr<UGamePlatformAbilitySystemComponent>(F->ASC.Get()),H});++Index;
 }
 return FGamePlatformResult::Success();
}
FGamePlatformResult FGameplayManualState::ActivateFixture(bool B){
 const auto E=Epoch;for(auto F:{Source,Target,Alternate}){if(!F.IsValid())return GameplayError(TEXT("Demo.Gameplay.Fixture"),TEXT("先创建真实隔离夹具。"));F->Eligibility->SetServerPlayerActive(B);if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("资格事件期间来源已停。"));}return FGamePlatformResult::Success();
}
FGamePlatformResult FGameplayManualState::ReleaseResources(){
 if(Resources.IsValid()){auto R=Data()?Data()->ReleaseResources(Resources):GameplayError(TEXT("Demo.Gameplay.DataLost"),TEXT("Data失效；保留完整租约供Stop重试。"));if(!R.IsSuccess())return R;Resources={};}
 while(!RetainedResources.IsEmpty()){auto R=Data()?Data()->ReleaseResources(RetainedResources.Last()):GameplayError(TEXT("Demo.Gameplay.DataLost"),TEXT("Data失效；保留完整租约。"));if(!R.IsSuccess())return R;RetainedResources.Pop();}return FGamePlatformResult::Success();
}
void FGameplayManualState::Load(const TArray<FSoftObjectPath>& Paths,TFunction<FGamePlatformResult()> Apply,FDivineBeastsDemoOperationCompletion Done){
 if(Loading||RetainedResources.Num()>=8||!Data()){Done(Context.Token,GameplayError(TEXT("Demo.Gameplay.LoadBusy"),TEXT("资源请求在途/租约上限；先Reset。")));return;}
 if(Resources.IsValid()){RetainedResources.Add(Resources);Resources={};}
 Loading=MoveTemp(Done);const auto E=Epoch;auto Weak=TWeakPtr<FGameplayManualState>(AsShared());FGamePlatformResult Accepted;
 Resources=Data()->AcquireResources(Paths,EGamePlatformDataLifetime::World,World.Get(),[Weak,E,Apply=MoveTemp(Apply)](const auto& H,const auto& R){auto S=Weak.Pin();if(!S||!S->Current(E)||!S->Loading)return;const auto& Own=S->Resources;
  if(Own.ScopeId!=H.ScopeId||Own.LeaseId!=H.LeaseId||Own.Generation!=H.Generation||Own.IssuerProof!=H.IssuerProof||Own.ResourcePaths!=H.ResourcePaths||Own.DefinitionId!=H.DefinitionId||Own.Bundles!=H.Bundles)return;
  auto D=MoveTemp(S->Loading);auto Result=R;if(Result.IsSuccess()){TGuardValue<bool> Busy(S->bMutating,true);Result=Apply();}if(!S->Current(E))Result=FGamePlatformResult::Cancelled(TEXT("资源应用期间来源失活。"));D(S->Context.Token,Result);
 },Accepted);
 if(!Accepted.IsSuccess()){auto D=MoveTemp(Loading);if(D)D(Context.Token,Accepted);}
}
FGamePlatformResult FGameplayManualState::Reset(){
 // Destroy only owned mesh/actors. Services lack per-mesh cancel; do not CancelAll foreign feedback.
 auto R=Server(TEXT("UnPossessAI"),FManualArguments(Context,{}));if(!R.IsSuccess())return R;
 if(GameplayHost.IsValid()){GameplayHost->OnFixtureChanged.Remove(GameplayHostChanged);GameplayHostChanged.Reset();R=GameplayHost->ReleaseFixture(Context.Token);if(!R.IsSuccess())return R;GameplayHost.Reset();}
 DefinitionSelection.Reset();++DefinitionSelectionGeneration;DefinitionObject.Reset();DefinitionResult={};
 if(CharacterIdentity.IsValid()){CharacterIdentity->OnReadinessChanged().Remove(CharacterReadiness);CharacterIdentity->OnIdentityChanged().Remove(CharacterIdentityChanged);}CharacterReadiness.Reset();CharacterIdentityChanged.Reset();CharacterIdentity.Reset();
 for(const auto& Sub:FixtureSubscriptions)if(auto* F=Sub.Actor.Get()){F->ASC->OnAvatarBindingChanged().Remove(Sub.Binding);F->Eligibility->OnEligibilityChanged().Remove(Sub.Eligibility);F->Combat->OnCombatObservationChanged().Remove(Sub.Combat);F->CastFacts->OnCastFactsChanged().Remove(Sub.Cast);F->GetRootComponent()->TransformUpdated.Remove(Sub.Transform);}FixtureSubscriptions.Reset();
 for(const auto& H:AttributeHandlers)if(auto* C=H.Key.Get()){if(H.Value.Num()==2){C->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetHealthAttribute()).Remove(H.Value[0]);C->GetGameplayAttributeValueChangeDelegate(UGamePlatformCombatAttributeSet::GetEnergyAttribute()).Remove(H.Value[1]);}}AttributeHandlers.Reset();
 for(auto* A:{&Projectile})if(A->IsValid()){(*A)->AuthorityCancel();if(!(*A)->Destroy())return GameplayError(TEXT("Demo.Gameplay.ProjectileCleanup"),TEXT("保留真实弹体供重试。"));A->Reset();}
 if(Obstacle.IsValid()){if(!Obstacle->Destroy())return GameplayError(TEXT("Demo.Gameplay.ObstacleCleanup"),TEXT("障碍未销毁。"));Obstacle.Reset();}
 for(auto* F:{&Source,&Target,&Alternate})if(F->IsValid()){
  (*F)->AIState->OnStateChanged.RemoveAll(Observer.Get());(*F)->ASC->ClearAbilityInput();(*F)->ASC->ClearActivationGate(F->Get());
  if(!(*F)->Destroy())return GameplayError(TEXT("Demo.Gameplay.FixtureCleanup"),TEXT("角色未销毁；保留归属。"));F->Reset();
 }
 AttributeHandlers.Reset();InputHandle={};Hero.Reset();AI.Reset();Overlay.Reset();CastId.Invalidate();LastDamageId.Invalidate();HitstopId.Invalidate();FlashId.Invalidate();
 if(bOwnTrack){auto* Q=Player.IsValid()?Player->GetSubsystem<UGamePlatformQuestClientSubsystem>():nullptr;if(!Q)return GameplayError(TEXT("Demo.Gameplay.QuestCleanup"),TEXT("原Quest投影失效，不能确认恢复。"));if(bOriginalTracked)Q->TrackQuest(TouchedQuest);else Q->UntrackQuest(TouchedQuest);if(Q->IsTracked(TouchedQuest)!=bOriginalTracked)return GameplayError(TEXT("Demo.Gameplay.QuestCleanup"),TEXT("追踪初态未恢复。"));bOwnTrack=false;}
 return ReleaseResources();
}
FGamePlatformResult FGameplayManualState::Cleanup(){
 bActive=false;++Epoch;if(bMutating||bCleaning)return GameplayError(TEXT("Demo.Gameplay.InFlight"),TEXT("原生同步栈尚未退回，保留资源及Host；Stop需重试。"));TGuardValue<bool> Guard(bCleaning,true);
 if(Loading){auto D=MoveTemp(Loading);D(Context.Token,FGamePlatformResult::Cancelled(TEXT("本资源准备已取消。")));}
 if(ExperienceSubscription.IsValid()){if(!Experience.IsValid()||!Experience->Unsubscribe(ExperienceSubscription))return GameplayError(TEXT("Demo.Gameplay.ExperienceCleanup"),TEXT("保留完整体验订阅供重试。"));ExperienceSubscription={};}
 if(auto* Q=Player.IsValid()?Player->GetSubsystem<UGamePlatformQuestClientSubsystem>():nullptr)Q->OnChanged.Remove(QuestChanged);QuestChanged.Reset();
 if(auto* H=Player.IsValid()?Player->GetSubsystem<UGamePlatformLocalHitstopSubsystem>():nullptr)H->OnVisualHitstopFinished().Remove(HitstopFinished);HitstopFinished.Reset();
 auto R=Reset();if(!R.IsSuccess())return R;
 FWorldDelegates::OnWorldCleanup.Remove(WorldCleanup);WorldCleanup.Reset();if(Observer.IsValid())Observer->Changed=nullptr;Observer.Reset();
 if(bHost){if(!Host.IsValid())return GameplayError(TEXT("Demo.Gameplay.HostLost"),TEXT("原排他宿主失效。"));R=Host->Release(Context.Token);if(!R.IsSuccess())return R;bHost=false;}return FGamePlatformResult::Success();
}
class FGameplayManualAdapter final:public IDivineBeastsDemoScenarioAdapter {
 TSharedRef<FGameplayManualState> S;
public:
 FGameplayManualAdapter(UWorld& W,FName F,TSharedPtr<FGameplayManualState>* Out=nullptr):S(MakeShared<FGameplayManualState>()){S->World=&W;S->Feature=F;if(Out)*Out=S;}
 ~FGameplayManualAdapter(){auto State=S;auto R=State->Cleanup();if(!R.IsSuccess()&&State->bMutating)AsyncTask(ENamedThreads::GameThread,[State]{State->Cleanup();});}
 void Prepare(const FDivineBeastsDemoScenarioContext& C,FDivineBeastsDemoOperationCompletion D)override{auto State=S;if(State->bBound){D(C.Token,GameplayError(TEXT("Demo.Gameplay.Bound"),TEXT("不可重绑定。")));return;}State->bBound=true;State->Context=C;if(!C.Token.IsValid()||C.World!=State->World||C.Scenario.FeatureId!=State->Feature||!ValidateGameplayManualContract(C.Scenario)){D(C.Token,GameplayError(TEXT("Demo.Gameplay.Source"),TEXT("世界或封闭功能身份不匹配。")));return;}auto R=State->Prepare();D(C.Token,R);}
 void Execute(FName O,const TArray<FDivineBeastsDemoParameterValue>& V,FDivineBeastsDemoOperationCompletion D)override{auto State=S;if(!State->Alive()){D(State->Context.Token,GameplayError(TEXT("Demo.Gameplay.Inactive"),TEXT("来源已失活。")));return;}auto R=ValidateGameplayManualValues(State->Context.Scenario,V);if(!R.IsSuccess()||!State->Context.Scenario.Operations.Contains(O)){D(State->Context.Token,R.IsSuccess()?GameplayError(TEXT("Demo.Gameplay.Operation"),TEXT("操作不在白名单。")):R);return;}if(O==TEXT("Stop")){State->bActive=false;D(State->Context.Token,State->Cleanup());return;}if(State->bMutating||State->Loading){D(State->Context.Token,GameplayError(TEXT("Demo.Gameplay.Busy"),TEXT("原生命令在途，不能同步重入新命令。")));return;}State->Execute(O,FManualArguments(State->Context,V),MoveTemp(D));}
 void Stop(FDivineBeastsDemoOperationCompletion D)override{auto State=S;D(State->Context.Token,State->Cleanup());}
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateGameplayManualScenario(UWorld& W,FName F){if(!GameplayManualFeatures().Contains(F))return {};return MakeUnique<FGameplayManualAdapter>(W,F);}
#if WITH_DEV_AUTOMATION_TESTS
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateGameplayManualScenarioForTesting(UWorld& W,FName F,TSharedPtr<FGameplayManualState>& Out){return MakeUnique<FGameplayManualAdapter>(W,F,&Out);}
#endif
}
