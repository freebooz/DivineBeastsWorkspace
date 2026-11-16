#include "Scenarios/DivineBeastsDemoWorldState.h"
#include "Interfaces/IGamePlatformPCGService.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "Services/GamePlatformPCGInspection.h"
#include "PCGComponent.h"
#include "Components/GamePlatformInteractorComponent.h"
#include "Components/GamePlatformInteractableComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
namespace DivineBeasts::Demo {
FWorldInteractionFixture DecodeWorldInteractionFixture(const FManualArguments& A){
 FWorldInteractionFixture C;
 if(auto* V=A.Find(TEXT("Distance"),EDivineBeastsDemoParameterType::Number))C.Distance=V->NumberValue;
 if(auto* V=A.Find(TEXT("HoldSeconds"),EDivineBeastsDemoParameterType::Number))C.HoldSeconds=V->NumberValue;
 if(auto* V=A.Find(TEXT("OptionId"),EDivineBeastsDemoParameterType::Identifier))C.OptionId=V->IdentifierValue;
 if(auto* V=A.Find(TEXT("CommitKind"),EDivineBeastsDemoParameterType::Integer))C.CommitKind=static_cast<int32>(V->IntegerValue);
 if(auto* V=A.Find(TEXT("Charges"),EDivineBeastsDemoParameterType::Integer))C.Charges=static_cast<int32>(V->IntegerValue);
 if(auto* V=A.Find(TEXT("LineOfSight"),EDivineBeastsDemoParameterType::Boolean))C.bLineOfSight=V->bBooleanValue;
 return C;
}
void FWorldManualState::PCGOperation(FName O,const FManualArguments& A,FDivineBeastsDemoOperationCompletion D){
 if(O==TEXT("ConfigureComponent")&&(PCGComponent.IsValid()||PCGBounds.IsValid())){D(Context.Token,WorldError(TEXT("Demo.PCG.ComponentAlreadyOwned"),TEXT("本运行已持有一个PCG组件；先Reset，不覆盖旧Owner。")));return;}
 auto* S=IGamePlatformPCGService::Get(*World);if(!S){D(Context.Token,WorldError(TEXT("Demo.PCG.ServiceMissing"),TEXT("真实PCG服务不可用。")));return;}
 if(O==TEXT("Generate")){
  if(Generation.IsValid()){D(Context.Token,WorldError(TEXT("Demo.PCG.Busy"),TEXT("先释放/取消本次生成并等待真实Cleaned。")));return;}
  auto* W=IGamePlatformWorldService::Get(*World);const auto V=W?W->GetReadiness():FGamePlatformWorldReadinessSnapshot{};
  if(!V.bDefinitionLoaded||!V.Context.ContextGeneration.IsValid()){D(Context.Token,WorldError(TEXT("Demo.PCG.WorldContextRequired"),TEXT("缺真实WorldDefinition及区域上下文，不自建PCG世界目录。")));return;}
  if(!Anchor.IsValid())Anchor=SpawnAnchor(FVector::ZeroVector);FGamePlatformPCGRequest Q;
  UGamePlatformPrimaryDataAsset::BuildDefinitionAssetId(FName(*(A.Id(TEXT("ProfileId")).ToString()+TEXT("@")+FString::FromInt(static_cast<int32>(A.Int(TEXT("ProfileVersion")))))),Q.ProfileId);
  FGamePlatformId::TryParse(A.Id(TEXT("RegionId")).ToString()+TEXT("@1"),Q.RegionId);Q.ContextGeneration=V.Context.ContextGeneration;Q.CenterCm=FVector(A.Number(TEXT("X")),A.Number(TEXT("Y")),0);Q.Owner=Anchor.Get();FGamePlatformResult R;const auto E=Epoch;
  auto H=S->RequestGeneration(Q,R);Generation=H;
  if(!Current(E)){if(H.IsValid())S->CancelGeneration(H);D(Context.Token,FGamePlatformResult::Cancelled(TEXT("请求同步通知期间已停止。")));return;}
  if(H.IsValid()){const TWeakPtr<FWorldManualState> Weak=AsShared();GenerationSubscription=S->SubscribeGeneration(H,Anchor.Get(),[Weak,E,H](const auto& V){auto P=Weak.Pin();if(!P||!P->Current(E)||!(P->Generation==H)||!(V.Handle==H))return;P->Observe(FString::Printf(TEXT("真实PCG通知：Phase=%d Outcome=%d Valid=%d Count=%d Fingerprint=%s Code=%s；清理完成独立于生成成功。"),int32(V.Phase),int32(V.Outcome),V.bIsResultValid,V.ActualInstanceCount,*V.OutputFingerprint,*V.Result.Code.ToString()));});}
  D(Context.Token,R);return;
 }
 if(O==TEXT("CancelGeneration")||O==TEXT("ReleaseGeneration")){
  if(!Generation.IsValid()){D(Context.Token,WorldError(TEXT("Demo.PCG.NoRequest"),TEXT("没有本Run生成句柄。")));return;}
  auto R=O==TEXT("CancelGeneration")?S->CancelGeneration(Generation):S->ReleaseGeneration(Generation);auto V=S->GetGenerationSnapshot(Generation);
  Observe(FString::Printf(TEXT("真实PCG清理：Phase=%d Outcome=%d；Cleaning保留所有权。"),int32(V.Phase),int32(V.Outcome)));
  if(V.Phase==EGamePlatformPCGPhase::Cleaned){S->UnsubscribeGeneration(GenerationSubscription);Generation={};GenerationSubscription={};}
  D(Context.Token,R);return;
 }
 if(O==TEXT("Reset")){auto R=ResetObjects();if(R.IsSuccess())R=ReleaseAsset();D(Context.Token,R);return;}
 Load(A.Asset(TEXT("Profile")),[Weak=TWeakPtr<FWorldManualState>(AsShared()),O,Seed=static_cast<int32>(A.Int(TEXT("Seed")))](UObject* Object){
  auto P=Weak.Pin();auto* Profile=Cast<UGamePlatformPCGProfileDefinition>(Object);if(!P||!P->Alive()||!Profile)return WorldError(TEXT("Demo.PCG.ProfileClass"),TEXT("租约资源不是PCG Profile。"));
  auto* Service=IGamePlatformPCGService::Get(*P->World);if(O==TEXT("ValidateProfile"))return Service?Service->ValidateProfile(*Profile):WorldError(TEXT("Demo.PCG.Missing"),TEXT("PCG服务失效。"));
  if(O==TEXT("ValidateGraph"))return GamePlatformPCGInspection::ValidateApprovedGraph(*Profile);
  if(O==TEXT("ConfigureComponent")){
   if(P->PCGComponent.IsValid()||P->PCGBounds.IsValid())return WorldError(TEXT("Demo.PCG.ComponentAlreadyOwned"),TEXT("本运行已有PCG组件或尚未退役的采样体积；先Reset。"));
   const FVector Half=Profile->HalfExtentCm;
   if(!FMath::IsFinite(Half.X)||!FMath::IsFinite(Half.Y)||!FMath::IsFinite(Half.Z)||Half.GetMin()<=0||Half.GetMax()>5000)return WorldError(TEXT("Demo.PCG.Bounds"),TEXT("Profile采样体积必须为有限正半径且各轴不超过5000cm。"));
   if(!P->Anchor.IsValid())P->Anchor=P->SpawnAnchor(FVector::ZeroVector);
   if(!P->Anchor.IsValid())return WorldError(TEXT("Demo.PCG.Owner"),TEXT("本PCG Owner创建失败。"));
   const auto E=P->Epoch;
   // PCG原生边界来自Owner的Primitive；SceneRoot不提供空间体积。先建立真实有界夹具。
   TStrongObjectPtr<UBoxComponent> Bounds(NewObject<UBoxComponent>(P->Anchor.Get()));P->PCGBounds=Bounds.Get();
   Bounds->SetupAttachment(P->Anchor->GetRootComponent());P->Anchor->AddInstanceComponent(Bounds.Get());
   Bounds->SetBoxExtent(Half,false);Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);Bounds->SetGenerateOverlapEvents(false);Bounds->SetCanEverAffectNavigation(false);Bounds->RegisterComponent();
   if(!P->Current(E)||P->PCGBounds.Get()!=Bounds.Get())return FGamePlatformResult::Cancelled(TEXT("PCG采样体积登记期间来源已停止。"));
   if(!Bounds->IsRegistered()||!Bounds->Bounds.GetBox().IsValid)return WorldError(TEXT("Demo.PCG.BoundsRegistration"),TEXT("真实PCG采样体积尚未登记，保留归属供Reset/Stop。"));
   TStrongObjectPtr<UPCGComponent> C(NewObject<UPCGComponent>(P->Anchor.Get()));P->PCGComponent=C.Get();P->Anchor->AddInstanceComponent(C.Get());
   // RegisterComponent会在已BeginPlay世界同步进入PCG BeginPlay；必须先完成OnDemand/图/Seed配置。
   const auto Configured=GamePlatformPCGInspection::ConfigureOwnedComponent(*C,*Profile,Seed);
   if(!P->Current(E)||P->PCGComponent.Get()!=C.Get()||P->PCGBounds.Get()!=Bounds.Get())return FGamePlatformResult::Cancelled(TEXT("PCG组件配置期间来源已停止。"));
   if(!Configured.IsSuccess())return Configured; // 保留未登记组件和真实Bounds，Stop精确退役。
   if(!C->GetExecutionState().GetBounds().IsValid)return WorldError(TEXT("Demo.PCG.ExecutionBounds"),TEXT("原生执行源没有有效Owner体积；不得登记或生成。"));
   C->RegisterComponent();
   if(!P->Current(E)||P->PCGComponent.Get()!=C.Get()||P->PCGBounds.Get()!=Bounds.Get())return FGamePlatformResult::Cancelled(TEXT("PCG组件登记期间来源已停止。"));
   return WorldBool(C->IsRegistered(),TEXT("RegisterConfiguredPCGComponent"));
  }
  if(O==TEXT("InspectOutput")){
   if(!P->PCGComponent.IsValid())return WorldError(TEXT("Demo.PCG.NoComponent"),TEXT("先配置本次PCG组件；未Generate不得把零输出当生成完成。"));
   auto V=GamePlatformPCGInspection::InspectOwnedOutput(*P->PCGComponent,*Profile,FVector::ZeroVector);P->Observe(FString::Printf(TEXT("本组件真实审查 Count=%d Fingerprint=%s；仅审查，不冒充完成委托。"),V.InstanceCount,*V.Fingerprint));return V.Result;
  }
  return WorldError(TEXT("Demo.PCG.Operation"),TEXT("未知PCG操作。"));
 },MoveTemp(D));
}
FGamePlatformResult FWorldManualState::CreateInteraction(const FWorldInteractionFixture& A,UStaticMesh* VisualMesh){
 const auto E=Epoch;
 if(A.Charges<=0)return WorldError(TEXT("Demo.Interaction.Charges"),TEXT("本可用目标夹具Charges须为正；Outcome显式0为负向案例，不遗留目标。"));
 if(Target.IsValid())return WorldError(TEXT("Demo.Interaction.TargetBusy"),TEXT("先Reset本目标。"));auto* PC=Player->GetPlayerController(World.Get());if(!PC||!PC->HasAuthority())return WorldError(TEXT("Demo.Interaction.Authority"),TEXT("缺真实Standalone PlayerController权威。"));
 FVector Origin;FRotator View;PC->GetPlayerViewPoint(Origin,View);const double Distance=A.Distance;
 Target=SpawnAnchor(Origin+View.Vector()*(Distance>0?Distance:150));if(!Target.IsValid())return WorldError(TEXT("Demo.Interaction.TargetSpawn"),TEXT("本目标Actor创建失败。"));
 auto* Sphere=NewObject<USphereComponent>(Target.Get());Sphere->InitSphereRadius(20);Sphere->SetupAttachment(Target->GetRootComponent());Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);Sphere->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);Target->AddInstanceComponent(Sphere);Sphere->RegisterComponent();
 if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("目标碰撞创建期间已停止。"));
 if(!VisualMesh)return WorldError(TEXT("Demo.Interaction.VisualMissing"),TEXT("真实可见网格资源未取得Data租约。"));
 auto* Mesh=NewObject<UStaticMeshComponent>(Target.Get());Mesh->SetupAttachment(Target->GetRootComponent());Mesh->SetStaticMesh(VisualMesh);Mesh->SetRelativeScale3D(FVector(.4));Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Target->AddInstanceComponent(Mesh);Mesh->RegisterComponent();
 if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("目标网格创建期间已停止。"));
 auto* C=NewObject<UGamePlatformInteractableComponent>(Target.Get());Interactable=C;C->SetupAttachment(Target->GetRootComponent());Target->AddInstanceComponent(C);C->RegisterComponent();if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("目标组件注册期间停止。"));C->OnStateChanged.AddDynamic(Observer.Get(),&UDivineBeastsDemoWorldObserver::Target);
 if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("目标组件创建期间已停止。"));
 FGamePlatformInteractionOption Option;Option.OptionId=A.OptionId;if(Option.OptionId.IsNone())Option.OptionId=TEXT("Manual.Toggle");Option.CommitKind=static_cast<EGamePlatformInteractionCommitKind>(A.CommitKind);if(Feature!=TEXT("GamePlatformInteraction.outcome"))Option.CommitKind=EGamePlatformInteractionCommitKind::Toggle;Option.bRequireLineOfSight=Feature==TEXT("GamePlatformInteraction.session")?A.bLineOfSight:true;Option.MaxDistance=250;
 if(Feature==TEXT("GamePlatformInteraction.session")){Option.Mode=EGamePlatformInteractionMode::Hold;Option.HoldDuration=A.HoldSeconds;}
 if(!C->SetOptions({Option}))return WorldError(TEXT("Demo.Interaction.Options"),TEXT("真实权威SetOptions拒绝。"));if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("设置目标事件期间停止。"));
 C->SetRemainingCharges(A.Charges);
 if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("目标Charges通知期间停止。"));
 return WorldBool(C->GetRemainingCharges()==A.Charges&&C->IsInteractionEnabled()&&C->GetOptions().Num()==1&&C->IsOptionAvailable(C->GetOptions()[0]),TEXT("CreateUsableTarget"));
}
void FWorldManualState::InteractionOperation(FName O,const FManualArguments& A,FDivineBeastsDemoOperationCompletion D){
 FGamePlatformResult R=FGamePlatformResult::Success();const auto E=Epoch;
 if(O==TEXT("CreateTarget")){const auto Config=DecodeWorldInteractionFixture(A);Load(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")),[Weak=TWeakPtr<FWorldManualState>(AsShared()),Config](UObject* Object){auto P=Weak.Pin();if(!P||!P->Alive())return FGamePlatformResult::Cancelled(TEXT("目标资源完成时来源失活。"));return P->CreateInteraction(Config,Cast<UStaticMesh>(Object));},MoveTemp(D));return;}
 else if(O==TEXT("AttachInteractor")){
  auto* PC=Player->GetPlayerController(World.Get());if(!PC||Interactor.IsValid()||PC->FindComponentByClass<UGamePlatformInteractorComponent>())R=WorldError(TEXT("Demo.Interaction.OwnerBusy"),TEXT("只添加本Run新组件；拒绝接管现有玩家交互器。"));
  else{auto* C=NewObject<UGamePlatformInteractorComponent>(PC);Interactor=C;C->OnFocusChanged.AddDynamic(Observer.Get(),&UDivineBeastsDemoWorldObserver::Focus);C->OnSessionChanged.AddDynamic(Observer.Get(),&UDivineBeastsDemoWorldObserver::Session);C->OnResultChanged.AddDynamic(Observer.Get(),&UDivineBeastsDemoWorldObserver::Result);C->OnInteractionEvent.AddDynamic(Observer.Get(),&UDivineBeastsDemoWorldObserver::Interaction);PC->AddInstanceComponent(C);C->RegisterComponent();}
 }else if(O==TEXT("RefreshFocus")){if(!Interactor.IsValid())R=WorldError(TEXT("Demo.Interaction.NoInteractor"),TEXT("先附加本交互器。"));else Interactor->RefreshLocalFocus();}
 else if(O==TEXT("SetFocusPolicy")){if(!Interactor.IsValid())R=WorldError(TEXT("Demo.Interaction.NoInteractor"),TEXT("先附加本交互器。"));else Interactor->SetLocalFocusPolicy(A.Bool(TEXT("Enabled")),A.Bool(TEXT("Pointer")));}
 else if(O==TEXT("BeginInteraction")){if(!Interactor.IsValid())R=WorldError(TEXT("Demo.Interaction.NoInteractor"),TEXT("先附加本交互器。"));else{auto Request=Interactor->BeginFocusedInteraction();R=WorldBool(Request.IsValid(),TEXT("BeginFocusedInteraction"));if(Current(E))Observe(FString::Printf(TEXT("真实Begin Request=%s；最终资格/LOS/距离和提交由平台验证。Hub没有ServerActive时应拒绝；不设置假资格。"),*Request.ToString()));}}
 else if(O==TEXT("CancelInteraction")){if(Interactor.IsValid())Interactor->CancelCurrentInteraction();}
 else if(O==TEXT("ReserveExternal")){if(!Interactable.IsValid()||Reservation.IsValid())R=WorldError(TEXT("Demo.Interaction.ReservationBusy"),TEXT("需要本目标及空保留。"));else{const FGuid H=FGuid::NewGuid();Reservation=H;R=WorldBool(Interactable->BeginExternalOutcomeReservation(H),TEXT("BeginExternalOutcomeReservation"));if(!R.IsSuccess()&&Reservation==H)Reservation.Invalidate();}}
 else if(O==TEXT("CancelReservation")||O==TEXT("ConsumeReservation")){
  if(!Interactable.IsValid()||!Reservation.IsValid())R=WorldError(TEXT("Demo.Interaction.NoReservation"),TEXT("先保留本外部结果。"));else{auto H=Reservation;const bool B=O==TEXT("CancelReservation")?Interactable->CancelExternalOutcomeReservation(H):Interactable->FinalizeExternalConsume(H);R=WorldBool(B,*O.ToString());if(B&&Reservation==H)Reservation.Invalidate();}
 }else if(O==TEXT("Reset")){R=ResetObjects();if(R.IsSuccess())R=ReleaseAsset();}
 if(Current(E))Snapshot(O);D(Context.Token,Current(E)?R:FGamePlatformResult::Cancelled(TEXT("交互同步通知期间来源已停止。")));
}
}
