#include "Scenarios/DivineBeastsDemoWorldState.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Subsystems/GamePlatformWaterWorldSubsystem.h"
#include "Actors/GamePlatformWaterRegion.h"
#include "Components/BoxComponent.h"
#include "Definitions/GamePlatformWaterPresentationPreset.h"
namespace DivineBeasts::Demo {
void FWorldManualState::WorldOperation(FName O,const FManualArguments& A,FDivineBeastsDemoOperationCompletion D){
 auto* S=IGamePlatformWorldService::Get(*World);if(!S){D(Context.Token,WorldError(TEXT("Demo.World.ServiceMissing"),TEXT("真实World服务不可用。")));return;}
 const auto V=S->GetReadiness();if(!V.Context.ContextGeneration.IsValid()||!V.bDefinitionLoaded){D(Context.Token,WorldError(TEXT("Demo.World.DefinitionHostRequired"),TEXT("先由可信宿主通过真实WorldDefinition/Data初始化；Hub无生产Experience。不能在页内伪造Session或永久接管世界。")));return;}
 if(!Anchor.IsValid())Anchor=SpawnAnchor(FVector::ZeroVector);if(!Anchor.IsValid()){D(Context.Token,WorldError(TEXT("Demo.World.Anchor"),TEXT("本Run观察者Actor创建失败。")));return;}
 FGamePlatformResult R=FGamePlatformResult::Success();const auto E=Epoch;
 if(O==TEXT("AddContributor")){
  if(ContributorRegistration.IsValid())R=WorldError(TEXT("Demo.World.Busy"),TEXT("本贡献者已登记，先移除。"));
  else{Contributor=MakeShared<FWorldManualContributor>();auto H=S->RegisterReadinessContributor(Anchor.Get(),Contributor.ToSharedRef(),R);ContributorRegistration=H;if(!Current(E)&&H.IsValid()){S->Unregister(H);ContributorRegistration={};}}
 }else if(O==TEXT("SetContributor")){if(!Contributor.IsValid())R=WorldError(TEXT("Demo.World.NoContributor"),TEXT("先添加本贡献者。"));else{Contributor->Mode=static_cast<int32>(A.Int(TEXT("ContributorResult")));S->GetReadiness();}}
 else if(O==TEXT("RemoveContributor")){if(ContributorRegistration.IsValid()){R=WorldBool(S->Unregister(ContributorRegistration),TEXT("UnregisterContributor"));if(R.IsSuccess()){ContributorRegistration={};Contributor.Reset();}}}
 else if(O==TEXT("RegisterRegion")){
  if(RegionRegistration.IsValid())R=WorldError(TEXT("Demo.World.Busy"),TEXT("本Provider已登记，先移除。"));
  else{FGamePlatformRegionProvider P;FGamePlatformId::TryParse(A.Id(TEXT("RegionId")).ToString()+TEXT("@1"),P.RegionId);P.ContextGeneration=V.Context.ContextGeneration;P.Provider=Anchor.Get();P.Bounds=FBox(FVector(-400,-400,-100),FVector(400,400,300));
   const FString DefinitionText=A.Id(TEXT("DefinitionId")).ToString()+TEXT("@")+FString::FromInt(static_cast<int32>(A.Int(TEXT("DefinitionVersion"))));
   P.DefinitionId=FPrimaryAssetId(TEXT("GamePlatformDefinition"),FName(*DefinitionText));auto H=S->RegisterRegionProvider(P,R);RegionRegistration=H;
   if(!Current(E)&&H.IsValid()){S->UnregisterRegionProvider(H);RegionRegistration={};}
   if(Current(E)&&R.IsSuccess()&&!RegionSubscription.IsValid()){const TWeakPtr<FWorldManualState> Weak=AsShared();RegionSubscription=S->SubscribeRegions(Anchor.Get(),[Weak,E](const auto& Event){if(auto P=Weak.Pin())if(P->Current(E)&&Event.Observer==P->Anchor)P->Observe(FString::Printf(TEXT("Region事件：%s Entered=%d Generation=%s"),*Event.RegionId.ToString(),Event.bEntered,*Event.ContextGeneration.ToString()));});}
  }
 }else if(O==TEXT("QueryRegion")){FGamePlatformId Region;R=S->QueryRegion(FVector(A.Number(TEXT("X")),A.Number(TEXT("Y")),0),Region);if(Current(E))Observe(FString::Printf(TEXT("真实QueryRegion=%s Code=%s"),*Region.ToString(),*R.Code.ToString()));}
 else if(O==TEXT("MoveObserver"))R=S->UpdateObserver(Anchor.Get(),FVector(A.Number(TEXT("X")),A.Number(TEXT("Y")),0),V.Context.ContextGeneration);
 else if(O==TEXT("RemoveRegion")){if(RegionRegistration.IsValid()){R=WorldBool(S->UnregisterRegionProvider(RegionRegistration),TEXT("UnregisterRegionProvider"));if(R.IsSuccess())RegionRegistration={};}}
 else if(O==TEXT("RequestStreaming")){
  if(Streaming.IsValid())R=WorldError(TEXT("Demo.World.StreamingBusy"),TEXT("先取消本流送需求。"));
  else{FGamePlatformWorldStreamingRequest Q;Q.ContextGeneration=V.Context.ContextGeneration;Q.Owner=Anchor.Get();Q.LevelPackage=FName(*A.Asset(TEXT("Level")).GetLongPackageName());Q.TimeoutSeconds=A.Number(TEXT("Timeout"));Q.RequestedState=A.Bool(TEXT("Activated"))?EGamePlatformWorldStreamingRequestedState::Activated:EGamePlatformWorldStreamingRequestedState::Loaded;auto H=S->RequestStreaming(Q,R);Streaming=H;if(!Current(E)&&H.IsValid()){S->CancelStreaming(H);Streaming={};}if(Current(E)&&H.IsValid()){auto State=S->GetStreamingState(H);Observe(FString::Printf(TEXT("真实流送 State=%d Code=%s；Request返回受理，不代表关卡已显示。"),int32(State.State),*State.Error.ToString()));}}
 }else if(O==TEXT("CancelStreaming")){if(Streaming.IsValid()){R=S->CancelStreaming(Streaming);if(R.IsSuccess())Streaming={};}}
 if(Current(E))Snapshot(O);D(Context.Token,Current(E)?R:FGamePlatformResult::Cancelled(TEXT("世界操作通知期间已停止；只撤本次句柄。")));
}
FGamePlatformResult FWorldManualState::CreateWater(const FManualArguments& A){
 if(WaterRegion.IsValid())return WorldError(TEXT("Demo.Water.Busy"),TEXT("本水域已创建；先移除或Reset。"));
 auto* S=World->GetSubsystem<UGamePlatformWaterWorldSubsystem>();if(!S)return WorldError(TEXT("Demo.Water.Service"),TEXT("真实水域服务不可用。"));
 auto* Region=World->SpawnActorDeferred<AGamePlatformWaterRegion>(AGamePlatformWaterRegion::StaticClass(),FTransform(FRotator(0,A.Number(TEXT("Yaw")),0),FVector::ZeroVector));
 if(!Region)return WorldError(TEXT("Demo.Water.Spawn"),TEXT("真实水域Actor创建失败。"));
 WaterRegion=Region;Region->SetFlags(RF_Transient);Region->Region.RegionId=FName(*(TEXT("Manual.Water.")+Context.Token.RunId.ToString(EGuidFormats::Digits)));Region->Region.SurfaceOffsetCm=100;Region->Region.ShallowDepthCm=60;Region->Region.FlowSpeedCmPerSecond=20;Region->WaterBounds->SetBoxExtent(FVector(300,300,FMath::Max(200.,A.Number(TEXT("Depth")))));
 Region->FinishSpawning(Region->GetActorTransform());if(!Alive())return FGamePlatformResult::Cancelled(TEXT("水域出生通知期间停止。"));
 return WorldBool(Region->IsWaterRegionValid()&&S->RegisterRegion(Region),TEXT("RegisterRegion"));
}
void FWorldManualState::WaterOperation(FName O,const FManualArguments& A,FDivineBeastsDemoOperationCompletion D){
 FGamePlatformResult R=FGamePlatformResult::Success();const auto E=Epoch;
 if(O==TEXT("CreateRegion"))R=CreateWater(A);
 else if(O==TEXT("RemoveRegion")||O==TEXT("Reset")){
  if(WaterScope.IsValid()){auto* C=World->GetSubsystem<UGamePlatformWaterClientWorldSubsystem>();if(!C||!C->EndDevelopmentPresentation(WaterScope)){D(Context.Token,WorldError(TEXT("Demo.Water.ScopeCleanup"),TEXT("本表现清理失败，保留租约。")));return;}WaterScope.Invalidate();}
  R=ResetObjects();if(R.IsSuccess())R=ReleaseAsset();
 }else if(O==TEXT("Sample")){
  auto* S=World->GetSubsystem<UGamePlatformWaterWorldSubsystem>();FGamePlatformWaterSample V;const bool Found=S&&S->QueryWaterAtLocation(FVector(A.Number(TEXT("X")),A.Number(TEXT("Y")),A.Number(TEXT("Z"))),V);
  Observe(FString::Printf(TEXT("真实水深查询：Found=%d Region=%s Surface=%.2f Depth=%.2f Shallow=%d Flow=%s"),Found,*V.RegionId.ToString(),V.SurfaceZCm,V.DepthCm,V.bShallow,*V.FlowVelocityCmPerSecond.ToString()));R=WorldBool(Found,TEXT("QueryWaterAtLocation"));
 }else if(O==TEXT("AttachInteractor")){
  if(WaterInteractor.IsValid())R=WorldError(TEXT("Demo.Water.Busy"),TEXT("本涉水组件已存在。"));
  else{if(!Anchor.IsValid())Anchor=SpawnAnchor(FVector(A.Number(TEXT("X")),A.Number(TEXT("Y")),A.Number(TEXT("Z"))));if(!Anchor.IsValid())R=WorldError(TEXT("Demo.Water.Owner"),TEXT("真实权威涉水Owner创建失败。"));else{auto* C=NewObject<UGamePlatformWaterInteractorComponent>(Anchor.Get());WaterInteractor=C;C->QueryIntervalSeconds=A.Number(TEXT("Interval"));C->FootSampleOffsetCm=A.Number(TEXT("FootOffset"));C->OnWaterStateChanged.AddDynamic(Observer.Get(),&UDivineBeastsDemoWorldObserver::Water);Anchor->AddInstanceComponent(C);C->RegisterComponent();}}
 }else if(O==TEXT("MoveInteractor")){R=WorldBool(Anchor.IsValid()&&Anchor->SetActorLocation(FVector(A.Number(TEXT("X")),A.Number(TEXT("Y")),A.Number(TEXT("Z")))),TEXT("MoveOwnedWaterInteractor"));}
 else if(O==TEXT("Configure")){
  auto* S=World->GetSubsystem<UGamePlatformWaterClientWorldSubsystem>();if(!S||WaterScope.IsValid()||!S->BeginDevelopmentPresentation(Context.Token.RunId,WaterScope)){D(Context.Token,WorldError(TEXT("Demo.Water.ScopeBusy"),TEXT("只接管原先空配置且无在途波纹的开发表现；不覆盖外部水体。")));return;}
  Load(A.Asset(TEXT("Preset")),[Weak=TWeakPtr<FWorldManualState>(AsShared())](UObject* Object){auto P=Weak.Pin();auto* Preset=Cast<UGamePlatformWaterPresentationPreset>(Object);if(!P||!P->Alive()||!Preset)return WorldError(TEXT("Demo.Water.Preset"),TEXT("租约资源不是水面预设。"));auto* C=P->World->GetSubsystem<UGamePlatformWaterClientWorldSubsystem>();return WorldBool(C&&C->ConfigureDevelopmentPresentation(P->WaterScope,Preset),TEXT("ConfigurePresentation"));},MoveTemp(D));return;
 }else if(O==TEXT("Ripple")||O==TEXT("SetQuality")){
  auto* S=World->GetSubsystem<UGamePlatformWaterClientWorldSubsystem>();if(!S||!WaterScope.IsValid())R=WorldError(TEXT("Demo.Water.ScopeRequired"),TEXT("先配置本次真实水面预设。"));
  else if(O==TEXT("Ripple"))R=WorldBool(S->RequestLocalRipple(FVector(0,0,100),A.Number(TEXT("Strength")),A.Bool(TEXT("Splash"))),TEXT("RequestLocalRipple"));
  else R=WorldBool(S->SetDevelopmentWaterQuality(WaterScope,static_cast<EGamePlatformWaterQualityTier>(A.Int(TEXT("Quality")))),TEXT("SetWaterQualityTier"));
 }
 if(Current(E))Snapshot(O);D(Context.Token,Current(E)?R:FGamePlatformResult::Cancelled(TEXT("水域操作通知期间已停止。")));
}
}
