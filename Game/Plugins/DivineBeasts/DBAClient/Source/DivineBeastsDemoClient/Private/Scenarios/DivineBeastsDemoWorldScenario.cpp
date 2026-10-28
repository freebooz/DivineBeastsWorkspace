#include "Scenarios/DivineBeastsDemoWorldState.h"
#include "Scenarios/DivineBeastsDemoManualWorldHostSubsystem.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Interfaces/IGamePlatformPCGService.h"
#include "Actors/GamePlatformWaterRegion.h"
#include "Subsystems/GamePlatformWaterWorldSubsystem.h"
#include "Subsystems/GamePlatformWeatherClientWorldSubsystem.h"
#include "Components/GamePlatformInteractorComponent.h"
#include "Components/GamePlatformInteractableComponent.h"
#include "PCGComponent.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
namespace DivineBeasts::Demo {
bool FWorldManualState::Alive()const{return bActive&&!bCleaning&&World.IsValid()&&!World->bIsTearingDown&&Player.IsValid()&&Player->GetWorld()==World.Get()&&Host.IsValid()&&Host->Owns(Context.Token);}
UGameInstance* FWorldManualState::Instance()const{return World.IsValid()?World->GetGameInstance():nullptr;}
IGamePlatformDataService* FWorldManualState::Data()const{return Instance()?IGamePlatformDataService::Get(*Instance()):nullptr;}
void FWorldManualState::Observe(FString Text)const {if(Alive()&&Context.PublishObservation)Context.PublishObservation(Context.Token,Text.Left(4096));}
void FWorldManualState::Snapshot(FName Event)const {
 FString Text=FString::Printf(TEXT("本Run=%s；事件=%s；只读真实快照；受理/终态/画面/人工结论分别记录。"),*Context.Token.RunId.ToString(),*Event.ToString());
 if(auto* S=IGamePlatformWorldService::Get(*World)){auto V=S->GetReadiness();Text+=FString::Printf(TEXT(" WorldState=%d Generation=%s Definition=%d Regions=%d Streaming=%d Contributors=%d Code=%s"),int32(V.Context.ReadinessState),*V.Context.ContextGeneration.ToString(),V.bDefinitionLoaded,V.bRequiredRegionsRegistered,V.bRequiredStreamingReady,V.bContributorsReady,*V.Context.Result.Code.ToString());}
 if(auto* S=World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>()){auto V=S->GetCurrentSnapshot();Text+=FString::Printf(TEXT(" WeatherRevision=%d Transition=%.2f"),V.Revision,V.TransitionSeconds);}
 if(Streaming.IsValid())if(auto* S=IGamePlatformWorldService::Get(*World)){auto V=S->GetStreamingState(Streaming);Text+=FString::Printf(TEXT(" StreamingState=%d Code=%s"),int32(V.State),*V.Error.ToString());}
 if(Generation.IsValid())if(auto* S=IGamePlatformPCGService::Get(*World)){auto V=S->GetGenerationSnapshot(Generation);Text+=FString::Printf(TEXT(" PCGPhase=%d Outcome=%d Valid=%d Instances=%d"),int32(V.Phase),int32(V.Outcome),V.bIsResultValid,V.ActualInstanceCount);}
 if(auto* S=World->GetSubsystem<UGamePlatformWaterClientWorldSubsystem>())Text+=FString::Printf(TEXT(" WaterQuality=%d NativeWaterBodies=%d；数量非GPU验收。"),int32(S->GetWaterQualityTier()),S->GetNativeWaterBodyCount());
 if(Feature.ToString().StartsWith(TEXT("GamePlatformTelemetry.")))if(auto* S=Instance()?Instance()->GetSubsystem<UGamePlatformTelemetrySubsystem>():nullptr){auto V=S->GetDiagnostics();Text+=FString::Printf(TEXT(" TelemetryDepth=%d Recorded=%lld Batches=%lld Sink=%s"),V.BufferDepth,V.RecordedTotal,V.SubmittedBatches,*V.SinkHealth.ToString());}
 if(WaterInteractor.IsValid()){const auto V=WaterInteractor->GetWaterState();Text+=FString::Printf(TEXT(" WaterPresence In=%d Deep=%d Swim=%d Region=%s"),V.bInWater,V.bDeepWater,V.bCanSwim,*V.RegionId.ToString());}
 if(Interactor.IsValid()){const auto& V=Interactor->GetCurrentFocus();Text+=FString::Printf(TEXT(" Focus=%d Target=%s Session=%s Hold=%.3f Result=%d"),V.IsValid(),*GetNameSafe(V.TargetActor),*Interactor->GetCurrentSession().SessionId.ToString(),Interactor->GetHoldProgress(),int32(Interactor->GetLastResult().Error));}
 if(Interactable.IsValid())Text+=FString::Printf(TEXT(" TargetRevision=%d Consumed=%d Toggle=%d Charges=%d Occupancy=%d"),Interactable->GetTargetRevision(),Interactable->IsConsumed(),Interactable->GetToggleState(),Interactable->GetRemainingCharges(),Interactable->GetOccupancyCount());
 Observe(MoveTemp(Text));
}
FGamePlatformResult FWorldManualState::Prepare(){
 auto* Widget=Cast<UUserWidget>(Context.SampleWidget.Get());Player=Widget?Widget->GetOwningLocalPlayer():nullptr;
 if(!World.IsValid()||World->GetNetMode()!=NM_Standalone||!World->IsGameWorld()||!World->HasBegunPlay()||!Player.IsValid()||Player->GetWorld()!=World.Get()||!Data())return WorldError(TEXT("Demo.World.Source"),TEXT("需要同GI/LP的Standalone GameWorld、真实SampleWidget与Data；不能初始化替代子系统。"));
 Host=Instance()->GetSubsystem<UDivineBeastsDemoManualHostSubsystem>();if(!Host.IsValid())return WorldError(TEXT("Demo.World.Host"),TEXT("排他ManualHost不可用。"));
 auto R=Host->Acquire(Context.Token,*World);if(!R.IsSuccess())return R;bHost=true;
 if((Feature.ToString().StartsWith(TEXT("GamePlatformWorld."))||Feature.ToString().StartsWith(TEXT("GamePlatformPCG."))))
  if(auto* WorldHost=World->GetSubsystem<UDivineBeastsDemoManualWorldHostSubsystem>()){
   R=WorldHost->PrepareRun(Context.Token,Feature);if(!R.IsSuccess())return R;
  }
 Observer.Reset(NewObject<UDivineBeastsDemoWorldObserver>(Instance()));
 bActive=true;const TWeakPtr<FWorldManualState> Weak=AsShared();const auto E=Epoch;
 if(auto* Service=IGamePlatformWorldService::Get(*World)){
  FGamePlatformResult Accepted;
  ReadinessSubscription=Service->SubscribeReadiness(World.Get(),[Weak,E](const FGamePlatformWorldReadinessSnapshot& V){
   if(auto P=Weak.Pin())if(P->Current(E))P->Observe(FString::Printf(TEXT("真实World状态通知 State=%d Definition=%d RegionDefinitions=%d Providers=%d RegionsReady=%d Code=%s"),int32(V.Context.ReadinessState),V.bDefinitionLoaded,V.bRegionDefinitionsLoaded,V.RegisteredRegionProviders,V.bRequiredRegionsRegistered,*V.Context.Result.Code.ToString()));
  },Accepted);
  if(!Accepted.IsSuccess())return Accepted;
 }
 Observer->Changed=[Weak,E](FName Event){if(auto S=Weak.Pin())if(S->Current(E))S->Snapshot(Event);};
 WorldCleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Weak](UWorld* W,bool,bool){if(auto S=Weak.Pin())if(S->World.Get()==W){S->bActive=false;++S->Epoch;S->Cleanup();}});
 if(auto* S=World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>())WeatherChanged=S->AddSnapshotHandler(FGamePlatformWeatherSnapshotChanged::FDelegate::CreateLambda([Weak,E](const auto&){if(auto P=Weak.Pin())if(P->Current(E))P->Snapshot(TEXT("WeatherSnapshotChanged"));}));
 if(auto* S=World->GetSubsystem<UGamePlatformWeatherClientWorldSubsystem>())VisualChanged=S->AddVisualChangedHandler(FGamePlatformWeatherVisualChanged::FDelegate::CreateLambda([Weak,E](const auto& V){if(auto P=Weak.Pin())if(P->Current(E))P->Observe(FString::Printf(TEXT("真实WeatherVisualChanged：Type=%d Rain=%.3f Snow=%.3f；GPU显示未人工签收。"),int32(V.Type),V.RainIntensity,V.SnowIntensity));}));
 if(Feature.ToString().StartsWith(TEXT("GamePlatformTelemetry."))){auto* S=Instance()->GetSubsystem<UGamePlatformTelemetrySubsystem>();if(!S||!S->BeginDevelopmentCapture(*World,Context.Token.RunId,TelemetryScope))return WorldError(TEXT("Demo.Telemetry.Ownership"),TEXT("需要空缓冲/空会话和无在途sink的开发遥测作用域；拒绝接管外部状态。"));}
 Snapshot(TEXT("InitialSnapshot"));return Alive()?FGamePlatformResult::Success():FGamePlatformResult::Cancelled(TEXT("准备通知期间已停止。"));
}
AActor* FWorldManualState::SpawnAnchor(const FVector& P){
 if(!World.IsValid()||World->bIsTearingDown)return nullptr;
 const auto E=Epoch;auto KeepSelf=AsShared();
 auto* Actor=World->SpawnActor<AActor>(AActor::StaticClass(),P,FRotator::ZeroRotator);
 if(!Actor)return nullptr;if(!Current(E)){Actor->Destroy();return nullptr;}Actor->SetFlags(RF_Transient);auto* Root=NewObject<USceneComponent>(Actor);Actor->SetRootComponent(Root);Actor->AddInstanceComponent(Root);Root->RegisterComponent();Actor->SetActorLocation(P);if(!Current(E)){Actor->Destroy();return nullptr;}return Actor;
}
FGamePlatformResult FWorldManualState::ReleaseAsset(){
 if(DefinitionLease.IsValid()){auto* S=Data();auto R=S?S->ReleaseDefinition(DefinitionLease):WorldError(TEXT("Demo.World.DefinitionDataLost"),TEXT("原Data失效，保留定义完整租约。"));if(!R.IsSuccess())return R;DefinitionLease={};}
 if(!AssetLease.IsValid())return FGamePlatformResult::Success();auto* S=Data();auto R=S?S->ReleaseResources(AssetLease):WorldError(TEXT("Demo.World.DataLost"),TEXT("Data服务失效，保留完整租约供Stop重试。"));if(R.IsSuccess())AssetLease={};return R;
}
void FWorldManualState::Load(const FSoftObjectPath& P,TFunction<FGamePlatformResult(UObject*)> Apply,FDivineBeastsDemoOperationCompletion Done){
 if(AssetLease.IsValid()&&!LoadingCompletion&&AssetLease.ResourcePaths.Num()==1&&AssetLease.ResourcePaths[0]==P&&Data()&&Data()->GetLeaseState(AssetLease)==EGamePlatformDataRequestState::Succeeded){
  TStrongObjectPtr<UObject> Asset(P.ResolveObject());
  if(auto* Definition=Cast<UGamePlatformDefinitionBase>(Asset.Get())){
   const TArray<FName> RequiredBundles=Definition->IsA<UGamePlatformPCGProfileDefinition>()?TArray<FName>{TEXT("PCGGeneration")}:TArray<FName>{};
   // 普通对象驻留（包括他源保活）不代替本运行成功的定义/递归依赖/资源束所有权。
   if(!DefinitionLease.IsValid()||DefinitionLease.DefinitionId!=Definition->GetPrimaryAssetId()||DefinitionLease.Bundles!=RequiredBundles||Data()->GetLeaseState(DefinitionLease)!=EGamePlatformDataRequestState::Succeeded||Data()->GetLoadedDefinition(DefinitionLease)!=Definition){Done(Context.Token,WorldError(TEXT("Demo.World.DefinitionNotReady"),TEXT("本运行的定义/依赖束未成功；同路径驻留不可绕过。先Reset后重新申请完整租约。")));return;}
  }
  if(!Asset.IsValid()){Done(Context.Token,WorldError(TEXT("Demo.World.ResourceNotReady"),TEXT("本资源租约无可读对象。")));return;}
  auto R=Apply(Asset.Get());Done(Context.Token,Alive()?R:FGamePlatformResult::Cancelled(TEXT("资源复用期间已停止。")));return;
 }
 if(AssetLease.IsValid()||LoadingCompletion||!P.IsValid()||!Data()){Done(Context.Token,WorldError(TEXT("Demo.World.AssetBusy"),TEXT("资源无效或仍持有上一份租约；先Reset。")));return;}
 LoadingCompletion=MoveTemp(Done);auto E=Epoch;const TWeakPtr<FWorldManualState> Weak=AsShared();FGamePlatformResult R;
 AssetLease=Data()->AcquireResources({P},EGamePlatformDataLifetime::World,World.Get(),[Weak,E,P,Apply=MoveTemp(Apply)](const auto& Lease,const auto& Result) mutable {
  auto S=Weak.Pin();if(!S||!S->Current(E)||!S->LoadingCompletion)return;const auto H=S->AssetLease;
  if(H.ScopeId!=Lease.ScopeId||H.LeaseId!=Lease.LeaseId||H.Generation!=Lease.Generation||H.IssuerProof!=Lease.IssuerProof||H.DefinitionId!=Lease.DefinitionId||H.Bundles!=Lease.Bundles||H.ResourcePaths!=Lease.ResourcePaths)return;
  TStrongObjectPtr<UObject> Asset(P.ResolveObject());
  if(Result.IsSuccess())if(auto* Definition=Cast<UGamePlatformDefinitionBase>(Asset.Get())){
   const bool PCG=Definition->IsA<UGamePlatformPCGProfileDefinition>();FGamePlatformResult Accepted;
   S->DefinitionLease=S->Data()->AcquireDefinition(Definition->GetPrimaryAssetId(),Definition->GetClass(),PCG?TArray<FName>{TEXT("PCGGeneration")}:TArray<FName>{},EGamePlatformDataLifetime::World,S->World.Get(),[Weak,E,P,Apply=MoveTemp(Apply)](const auto& Lease,const auto& Outcome){
    auto Self=Weak.Pin();if(!Self||!Self->Current(E)||!Self->LoadingCompletion)return;const auto& H=Self->DefinitionLease;
    if(H.ScopeId!=Lease.ScopeId||H.LeaseId!=Lease.LeaseId||H.Generation!=Lease.Generation||H.IssuerProof!=Lease.IssuerProof||H.DefinitionId!=Lease.DefinitionId||H.Bundles!=Lease.Bundles||H.ResourcePaths!=Lease.ResourcePaths)return;
    auto Done=MoveTemp(Self->LoadingCompletion);auto R=Outcome;TStrongObjectPtr<UObject> Object(P.ResolveObject());if(R.IsSuccess())R=Apply(Object.Get());if(!Self->Current(E))R=FGamePlatformResult::Cancelled(TEXT("定义应用期间已停止。"));Done(Self->Context.Token,R);
   },Accepted);
   if(!Accepted.IsSuccess()){auto Done=MoveTemp(S->LoadingCompletion);if(Done)Done(S->Context.Token,Accepted);}return;
  }
  auto Done=MoveTemp(S->LoadingCompletion);auto R=Result;if(R.IsSuccess())R=Apply(Asset.Get());
  if(!S->Current(E))R=FGamePlatformResult::Cancelled(TEXT("资源应用期间来源失活。"));Done(S->Context.Token,R);
 },R);
 if(!R.IsSuccess()){auto Callback=MoveTemp(LoadingCompletion);if(Callback)Callback(Context.Token,R);}
}
FGamePlatformResult FWorldManualState::ResetObjects(){
 const auto E=Epoch;
 if(Interactor.IsValid()){TStrongObjectPtr<UGamePlatformInteractorComponent> C(Interactor.Get());C->CancelCurrentInteraction();if(Epoch!=E)return WorldError(TEXT("Demo.Interaction.ResetReentered"),TEXT("取消通知期间清理重入；保留归属供Stop重试。"));C->SetLocalFocusPolicy(false,false);C->OnFocusChanged.RemoveAll(Observer.Get());C->OnSessionChanged.RemoveAll(Observer.Get());C->OnResultChanged.RemoveAll(Observer.Get());C->OnInteractionEvent.RemoveAll(Observer.Get());C->DestroyComponent();Interactor.Reset();}
 if(Interactable.IsValid()){if(Reservation.IsValid()&&!Interactable->CancelExternalOutcomeReservation(Reservation))return WorldError(TEXT("Demo.Interaction.ReservationCleanup"),TEXT("本保留未确认撤销；保留目标和身份。"));Reservation.Invalidate();Interactable->OnStateChanged.RemoveAll(Observer.Get());}
 if(WaterInteractor.IsValid()){WaterInteractor->OnWaterStateChanged.RemoveAll(Observer.Get());WaterInteractor->DestroyComponent();WaterInteractor.Reset();}
 if(PCGComponent.IsValid()){
  TStrongObjectPtr<UPCGComponent> Owned(PCGComponent.Get());TWeakObjectPtr<AActor> Owner=Owned->GetOwner();Owned->SetGraphLocal(nullptr);
  if(Epoch!=E)return WorldError(TEXT("Demo.PCG.ComponentCleanupReentered"),TEXT("本组件撤图通知期间清理重入，保留归属供Stop重试。"));
  Owned->DestroyComponent();
  TArray<UPCGComponent*> Remaining;if(Owner.IsValid())Owner->GetComponents<UPCGComponent>(Remaining);
  if(Owned->IsRegistered()||Remaining.Contains(Owned.Get()))return WorldError(TEXT("Demo.PCG.ComponentCleanupPending"),TEXT("本PCG组件仍登记在Owner，保留组件/图/资源租约供Stop重试。"));
  PCGComponent.Reset();
 }
 // 保持真实空间体积直至PCG组件及其图完全退役，避免清理/取消仍访问无界Owner。
 if(PCGBounds.IsValid()){
  TStrongObjectPtr<UBoxComponent> Owned(PCGBounds.Get());TWeakObjectPtr<AActor> Owner=Owned->GetOwner();Owned->DestroyComponent();
  TArray<UBoxComponent*> Remaining;if(Owner.IsValid())Owner->GetComponents<UBoxComponent>(Remaining);
  if(Epoch!=E||Owned->IsRegistered()||Remaining.Contains(Owned.Get()))return WorldError(TEXT("Demo.PCG.BoundsCleanupPending"),TEXT("本采样体积尚未确认退役，保留归属及资源租约供Stop重试。"));
  PCGBounds.Reset();
 }
 if(WaterRegion.IsValid()){auto* S=World->GetSubsystem<UGamePlatformWaterWorldSubsystem>();if(S)S->UnregisterRegion(WaterRegion.Get());if(!WaterRegion->Destroy())return WorldError(TEXT("Demo.Water.Destroy"),TEXT("本水域未确认销毁。"));WaterRegion.Reset();}
 if(Target.IsValid()){if(!Target->Destroy())return WorldError(TEXT("Demo.Interaction.Destroy"),TEXT("本目标未确认销毁。"));Target.Reset();Interactable.Reset();}
 // Anchor may still own registered regions/contributors/streaming or PCG owner; cleanup those first.
 return FGamePlatformResult::Success();
}
FGamePlatformResult FWorldManualState::Cleanup(){
#if WITH_EDITOR
 if(bAuthoringGraph)return WorldError(TEXT("Demo.PCG.AuthoringInFlight"),TEXT("原生作者/保存回调尚未返回；完整Data租约和宿主保留到Stop重试；已保存图不会删除。"));
 if(bSpawningLink)return WorldError(TEXT("Demo.Nav.SpawnInFlight"),TEXT("原生SmartLink出生栈尚未核对候选，不能提前释放宿主；候选收敛后Stop重试。"));
#endif
 if(bCleaning)return WorldError(TEXT("Demo.World.CleanupReentry"),TEXT("正在清理；根Stop可重试。"));TGuardValue<bool> Guard(bCleaning,true);bActive=false;++Epoch;
 if(LoadingCompletion){auto Done=MoveTemp(LoadingCompletion);Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("停止取消本次资源准备。")));}
 if(World.IsValid()){
  if(Generation.IsValid()){auto* S=IGamePlatformPCGService::Get(*World);if(!S)return WorldError(TEXT("Demo.PCG.CleanupService"),TEXT("PCG服务失效，保留句柄。"));auto Snap=S->GetGenerationSnapshot(Generation);auto R=Snap.Outcome==EGamePlatformPCGOutcome::Pending?S->CancelGeneration(Generation):S->ReleaseGeneration(Generation);if(!R.IsSuccess())return R;Snap=S->GetGenerationSnapshot(Generation);if(Snap.Phase!=EGamePlatformPCGPhase::Cleaned)return WorldError(TEXT("Demo.PCG.Cleaning"),TEXT("原生PCG仍Cleaning；保留句柄/订阅/资源，Stop稍后重试。"));S->UnsubscribeGeneration(GenerationSubscription);Generation={};GenerationSubscription={};}
  if(auto* S=IGamePlatformWorldService::Get(*World)){
   if(ReadinessSubscription.IsValid()){if(!S->UnsubscribeReadiness(ReadinessSubscription))return WorldError(TEXT("Demo.World.ReadinessCleanup"),TEXT("状态订阅完整世界身份不符；保留句柄。"));ReadinessSubscription={};}
   if(Streaming.IsValid()){auto R=S->CancelStreaming(Streaming);if(!R.IsSuccess())return R;Streaming={};}
   if(RegionRegistration.IsValid()){if(!S->UnregisterRegionProvider(RegionRegistration))return WorldError(TEXT("Demo.World.RegionCleanup"),TEXT("本Provider未确认撤销。"));RegionRegistration={};}
   if(ContributorRegistration.IsValid()){if(!S->Unregister(ContributorRegistration))return WorldError(TEXT("Demo.World.ContributorCleanup"),TEXT("本贡献者未确认撤销。"));ContributorRegistration={};Contributor.Reset();}
   if(RegionSubscription.IsValid()){if(!S->Unregister(RegionSubscription))return WorldError(TEXT("Demo.World.SubscriptionCleanup"),TEXT("本区域订阅未确认撤销。"));RegionSubscription={};}
  }else if(Streaming.IsValid()||RegionRegistration.IsValid()||ContributorRegistration.IsValid()||RegionSubscription.IsValid()||ReadinessSubscription.IsValid())return WorldError(TEXT("Demo.World.CleanupService"),TEXT("World服务失效，保留所有完整句柄。"));
  if(AtmosphereScope.IsValid()){auto* C=World->GetSubsystem<UGamePlatformWeatherClientWorldSubsystem>();if(!C||!C->EndDevelopmentAtmosphere(AtmosphereScope))return WorldError(TEXT("Demo.Weather.AtmosphereCleanup"),TEXT("光雾未确认恢复。"));AtmosphereScope.Invalidate();}
  if(WeatherScope.IsValid()){auto* S=World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();if(!S||!S->EndDevelopmentWeather(WeatherScope))return WorldError(TEXT("Demo.Weather.Cleanup"),TEXT("天气归属变更或清理失败；不清除外部状态。"));WeatherScope.Invalidate();}
  if(ProjectionScope.IsValid()){auto* C=World->GetSubsystem<UGamePlatformWeatherClientWorldSubsystem>();if(!C||!C->EndDevelopmentProjection(ProjectionScope))return WorldError(TEXT("Demo.Weather.ProjectionCleanup"),TEXT("天气客户端投影未归还；保留作用域供重试。"));ProjectionScope.Invalidate();}
  if(WaterScope.IsValid()){auto* S=World->GetSubsystem<UGamePlatformWaterClientWorldSubsystem>();if(!S||!S->EndDevelopmentPresentation(WaterScope))return WorldError(TEXT("Demo.Water.Cleanup"),TEXT("水面表现归属变更或清理失败；保留租约。"));WaterScope.Invalidate();}
  if(TelemetryScope.IsValid()){auto* S=Instance()?Instance()->GetSubsystem<UGamePlatformTelemetrySubsystem>():nullptr;if(!S||!S->EndDevelopmentCapture(TelemetryScope))return WorldError(TEXT("Demo.Telemetry.Cleanup"),TEXT("遥测sink/上下文归属未确认恢复。"));TelemetryScope.Invalidate();}
  if(auto* S=World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>())S->RemoveSnapshotHandler(WeatherChanged);
  if(auto* S=World->GetSubsystem<UGamePlatformWeatherClientWorldSubsystem>())S->RemoveVisualChangedHandler(VisualChanged);
 }
 auto R=CleanupEditor();if(!R.IsSuccess())return R;
 R=ResetObjects();if(!R.IsSuccess())return R;
 if(Anchor.IsValid()){if(!Anchor->Destroy())return WorldError(TEXT("Demo.World.AnchorCleanup"),TEXT("自有Anchor未确认销毁。"));Anchor.Reset();}
 R=ReleaseAsset();if(!R.IsSuccess())return R;
 if(Observer.IsValid())Observer->Changed=nullptr;Observer.Reset();FWorldDelegates::OnWorldCleanup.Remove(WorldCleanup);WorldCleanup.Reset();
 // Editor cleanup uses its own exact actor/query/provider ownership before releasing the host.
 if(bHost){if(!Host.IsValid())return WorldError(TEXT("Demo.World.HostLost"),TEXT("原ManualHost失效，未确认释放。"));R=Host->Release(Context.Token);if(!R.IsSuccess())return R;bHost=false;}
 return FGamePlatformResult::Success();
}
void FWorldManualState::Execute(FName O,const FManualArguments& A,FDivineBeastsDemoOperationCompletion D){
 if(O==TEXT("Snapshot")){Snapshot(TEXT("ExplicitSnapshot"));D(Context.Token,FGamePlatformResult::Success());return;}
 if(Feature.ToString().StartsWith(TEXT("GamePlatformWorld.")))return WorldOperation(O,A,MoveTemp(D));
 if(Feature.ToString().StartsWith(TEXT("GamePlatformWeather.")))return WeatherOperation(O,A,MoveTemp(D));
 if(Feature.ToString().StartsWith(TEXT("GamePlatformWater.")))return WaterOperation(O,A,MoveTemp(D));
 if(Feature.ToString().StartsWith(TEXT("GamePlatformPCG."))&&Feature!=TEXT("GamePlatformPCG.authoring")&&Feature!=TEXT("GamePlatformPCG.spatial_bake"))return PCGOperation(O,A,MoveTemp(D));
 if(Feature.ToString().StartsWith(TEXT("GamePlatformInteraction.")))return InteractionOperation(O,A,MoveTemp(D));
 if(Feature.ToString().StartsWith(TEXT("GamePlatformTelemetry.")))return TelemetryOperation(O,A,MoveTemp(D));
 return EditorOperation(O,A,MoveTemp(D));
}
class FWorldManualAdapter final:public IDivineBeastsDemoScenarioAdapter {
 TSharedRef<FWorldManualState> S;
public:
 FWorldManualAdapter(UWorld& W,FName F,TSharedPtr<FWorldManualState>* Out=nullptr):S(MakeShared<FWorldManualState>()){S->World=&W;S->Feature=F;if(Out)*Out=S;}
 ~FWorldManualAdapter(){S->Cleanup();}
 void Prepare(const FDivineBeastsDemoScenarioContext& C,FDivineBeastsDemoOperationCompletion D)override{auto State=S;
  if(State->bBound){D(C.Token,WorldError(TEXT("Demo.World.AlreadyBound"),TEXT("适配不可重绑定。")));return;}State->bBound=true;State->Context=C;
  if(!C.Token.IsValid()||C.World!=State->World||C.Scenario.FeatureId!=State->Feature||!ValidateWorldManualContract(C.Scenario)){D(C.Token,WorldError(TEXT("Demo.World.Source"),TEXT("固定身份/世界/参数合同错误。")));return;}
  auto R=State->Prepare();if(!R.IsSuccess())State->bActive=false;D(C.Token,R);
 }
 void Execute(FName O,const TArray<FDivineBeastsDemoParameterValue>& V,FDivineBeastsDemoOperationCompletion D)override{auto State=S;
  if(!State->Alive()){D(State->Context.Token,WorldError(TEXT("Demo.World.Inactive"),TEXT("本来源已停止。")));return;}
  auto R=ValidateWorldManualValues(State->Context.Scenario,V);if(!R.IsSuccess()||!State->Context.Scenario.Operations.Contains(O)){D(State->Context.Token,R.IsSuccess()?WorldError(TEXT("Demo.World.Operation"),TEXT("操作不在封闭白名单。")):R);return;}
  if(O==TEXT("Stop")){Stop(MoveTemp(D));return;}State->Execute(O,FManualArguments(State->Context,V),MoveTemp(D));
 }
 void Stop(FDivineBeastsDemoOperationCompletion D)override{auto State=S;State->bActive=false;++State->Epoch;D(State->Context.Token,State->Cleanup());}
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateWorldManualScenario(UWorld& W,FName F){return WorldManualFeatures().Contains(F)?MakeUnique<FWorldManualAdapter>(W,F):nullptr;}
#if WITH_DEV_AUTOMATION_TESTS
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateWorldManualScenarioForTesting(UWorld& W,FName F,TSharedPtr<FWorldManualState>& Out){Out.Reset();return WorldManualFeatures().Contains(F)?MakeUnique<FWorldManualAdapter>(W,F,&Out):nullptr;}
#endif
}
