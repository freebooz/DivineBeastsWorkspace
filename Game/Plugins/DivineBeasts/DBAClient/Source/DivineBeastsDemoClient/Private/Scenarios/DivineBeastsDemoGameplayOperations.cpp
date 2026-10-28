#include "Scenarios/DivineBeastsDemoGameplayState.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Components/GamePlatformAITargetComponent.h"
#include "Components/GamePlatformExperienceComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Framework/GamePlatformGameModeBase.h"
#include "Creation/GamePlatformCharacterCreationProvider.h"
#include "Initialization/GamePlatformCharacterInitializationExecutor.h"
#include "Definitions/GamePlatformHeroDefinition.h"
#include "Definitions/GamePlatformAbilitySetDefinition.h"
#include "Data/GamePlatformAIDefinition.h"
#include "Features/IModularFeatures.h"
#include "Services/GamePlatformQuestClientSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Locomotion/GamePlatformLocomotionAnimInstance.h"
#include "Materials/MaterialInterface.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Scenarios/DivineBeastsDemoGameplayHostMode.h"
namespace DivineBeasts::Demo {
FGamePlatformResult FGameplayManualState::GrantInput(FGameplayTag Tag){
 if(!Source.IsValid()||InputHandle.IsValid()||!Tag.IsValid()||!Tag.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Platform.Ability.Input"),false)))return GameplayError(TEXT("Demo.Gameplay.InputGrant"),TEXT("需唯一夹具授权与已注册输入子标签。"));
 FGameplayAbilitySpec S(UDivineBeastsDemoGameplayInput::StaticClass(),1);S.GetDynamicSpecSourceTags().AddTag(Tag);InputHandle=Source->ASC->GiveAbility(S);return GameplayBool(InputHandle.IsValid(),TEXT("GAS GiveAbility (owned fixture only)"));
}
FGamePlatformResult FGameplayManualState::RevokeInput(){if(!InputHandle.IsValid())return FGamePlatformResult::Success();if(!Source.IsValid())return GameplayError(TEXT("Demo.Gameplay.InputLost"),TEXT("原ASC失效。"));auto* C=Source->ASC.Get();const auto H=InputHandle;C->CancelAbilityHandle(H);C->ClearAbility(H);if(C->FindAbilitySpecFromHandle(H))return GameplayError(TEXT("Demo.Gameplay.InputCleanup"),TEXT("原Spec仍存在，保留完整句柄。"));InputHandle={};return FGamePlatformResult::Success();}
FGamePlatformResult FGameplayManualState::Gameplay(FName O,const FManualArguments& A){
 if(O==TEXT("SubscribeExperience")||O==TEXT("UnsubscribeExperience")){
  auto* GS=World->GetGameState();if(!ExperienceSubscription.IsValid())Experience=GS?GS->FindComponentByClass<UGamePlatformExperienceComponent>():nullptr;if(!Experience.IsValid())return GameplayError(TEXT("Demo.Gameplay.ExperienceHost"),TEXT("Hub不是平台Experience GameState；需隔离真实Experience宿主，禁止手动初始化第二份服务。"));
  if(O==TEXT("UnsubscribeExperience")){bool B=ExperienceSubscription.IsValid()&&Experience->Unsubscribe(ExperienceSubscription);if(B)ExperienceSubscription={};return GameplayBool(B,TEXT("UnsubscribeExperience"));}
  if(ExperienceSubscription.IsValid()){Snapshot();return FGamePlatformResult::Success();}FGamePlatformResult R;auto Weak=TWeakPtr<FGameplayManualState>(AsShared());const auto E=Epoch;
  ExperienceSubscription=Experience->SubscribeExperienceChanged(static_cast<UObject*>(GS),[Weak,E](const auto&){if(auto S=Weak.Pin())if(S->Current(E))S->Snapshot();},R);return R;
 }
 if(O==TEXT("PrepareGameplayHost")){GameplayHost=Cast<ADivineBeastsDemoGameplayHostMode>(World->GetAuthGameMode());if(!GameplayHost.IsValid())return GameplayError(TEXT("Demo.Gameplay.HostURL"),TEXT("需重启独立Hub ?game=/Script/DivineBeastsDemoClient.DivineBeastsDemoGameplayHostMode 及完整离线标志；不替换当前Mode。"));auto R=GameplayHost->AcquireFixture(Context.Token);if(!R.IsSuccess())return R;auto Weak=TWeakPtr<FGameplayManualState>(AsShared());const auto E=Epoch;GameplayHostChanged=GameplayHost->OnFixtureChanged.AddLambda([Weak,E]{if(auto S=Weak.Pin())if(S->Current(E))S->Snapshot();});Experience=GameplayHost->GetExperienceComponent();return Gameplay(TEXT("SubscribeExperience"),A);}
 if(O==TEXT("BeginExperience")||O==TEXT("DrainExperience")||O==TEXT("SubmitFixtureAdmission")||O==TEXT("StaleFixtureAdmission")||O==TEXT("ConfirmFixturePreparation")||O==TEXT("StaleFixturePreparation")||O==TEXT("RevokeFixtureAdmission")||O==TEXT("EvaluateSpawn")||O==TEXT("RequestRestart")){
  if(!GameplayHost.IsValid()||!GameplayHost->OwnsFixture(Context.Token))return GameplayError(TEXT("Demo.Gameplay.OwnedHost"),TEXT("先准备本Run最小离线宿主。"));auto* H=GameplayHost.Get();auto* C=H->GetFixtureController();
  if(O==TEXT("BeginExperience"))return H->BeginFixtureExperience(Context.Token);if(O==TEXT("DrainExperience"))return H->DrainFixtureExperience(Context.Token);
  if(O==TEXT("SubmitFixtureAdmission")||O==TEXT("StaleFixtureAdmission"))return H->SubmitFixtureAdmission(Context.Token,O==TEXT("StaleFixtureAdmission"));
  if(O==TEXT("ConfirmFixturePreparation")||O==TEXT("StaleFixturePreparation"))return H->ConfirmFixturePreparation(Context.Token,O==TEXT("StaleFixturePreparation"));
  if(O==TEXT("RevokeFixtureAdmission"))return H->RevokeFixtureAdmission(Context.Token);
  if(!C)return GameplayError(TEXT("Demo.Gameplay.Controller"),TEXT("原夹具Controller失效。"));if(O==TEXT("RequestRestart"))return H->RequestServerRestart(*C);auto Result=H->EvaluatePlayerStartEligibility(*C);Observe(FString::Printf(TEXT("真实出生资格=%d Reason=%s"),int32(Result.State),*Result.ReasonCode.ToString()));return GameplayBool(Result.State==EGamePlatformSpawnEligibility::Allowed,TEXT("real offline fixture EvaluatePlayerStartEligibility"));
 }
 if(O==TEXT("ReadCatalog")||O==TEXT("ValidateDraft")){
  auto P=IModularFeatures::Get().GetModularFeatureImplementations<IGamePlatformCharacterCreationProvider>(IGamePlatformCharacterCreationProvider::GetModularFeatureName());if(P.Num()!=1||!P[0])return GameplayError(TEXT("Demo.Gameplay.CreationProvider"),TEXT("真实角色创建提供者须恰好一份；不注册替身。"));
  TArray<FGamePlatformCharacterCreationHeroDescriptor> H;P[0]->GetCreateableHeroes(H);if(O==TEXT("ReadCatalog")){FString Text=FString::Printf(TEXT("本地Catalog Revision=%d Count=%d；仅本地结构，不是创建后端资格。"),P[0]->GetCatalogRevision(),H.Num());for(int32 I=0;I<FMath::Min(H.Num(),12);++I)Text+=TEXT(" ")+H[I].HeroDefinitionId.ToString();Observe(Text);return FGamePlatformResult::Success();}
  FString Error;bool B=P[0]->ValidateCreationDraft(A.Id(TEXT("HeroId")),{},Error);Observe(TEXT("真实草稿结构校验：")+Error);return GameplayBool(B,TEXT("ValidateCreationDraft; no backend create"));
 }
 if(O==TEXT("ValidateDefinition")){
  if(A.Asset(TEXT("Definition"))!=DefinitionSelection||!DefinitionResult.IsSuccess()||!DefinitionObject.IsValid())return GameplayError(TEXT("Demo.Gameplay.CurrentDefinitionMissing"),TEXT("当前选择尚未typed加载成功；失败/改选后不回退旧定义。"));
  if(Feature==TEXT("GamePlatformCharacter.definition")){auto* H=Cast<UGamePlatformHeroDefinition>(DefinitionObject.Get());if(!H)return GameplayError(TEXT("Demo.Gameplay.DefinitionType"),TEXT("本页只接受Hero定义。"));FString Error;bool B=H->IsDefinitionValid(Error);Observe(Error);return GameplayBool(B,TEXT("current HeroDefinition validation"));}
  if(Feature==TEXT("GamePlatformAbilitySystem.abilityset_definition")){auto* B=Cast<UGamePlatformAbilitySetDefinition>(DefinitionObject.Get());return B?B->ValidateDefinition():GameplayError(TEXT("Demo.Gameplay.DefinitionType"),TEXT("本页只接受AbilitySet定义。"));}
  return GameplayError(TEXT("Demo.Gameplay.DefinitionFeature"),TEXT("固定功能不提供本定义校验。"));
 }
 if(!Source.IsValid())return GameplayError(TEXT("Demo.Gameplay.Fixture"),TEXT("先创建真实夹具。"));
 auto* S=Source.Get();const auto E=Epoch;
 if(O==TEXT("SetActive")){S->Eligibility->SetServerPlayerActive(A.Bool(TEXT("Active")));return FGamePlatformResult::Success();}
 if(O==TEXT("AdvanceAvatar")){S->Eligibility->SetServerPlayerActive(false);if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("资格通知期间失活。"));S->Eligibility->AdvanceAvatarGeneration();if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("换代通知期间失活。"));return GameplayBool(S->Combat->ResetForNewAvatar(S->Eligibility->GetGameplayAvatarGeneration()),TEXT("owned ResetForNewAvatar"));}
 if(O==TEXT("InitializeCharacter")){
  // The actual initializer accepts any registered Authority ACharacter, and adds this same component.
  // Register before calling the unique executor so Ready/Identity events cannot be missed.
  if(!CharacterIdentity.IsValid()){
   S->ASC->ClearActivationGate(S);
   auto* Identity=NewObject<UDivineBeastsCharacterComponent>(S);S->AddInstanceComponent(Identity);CharacterIdentity=Identity;
   const auto Weak=TWeakPtr<FGameplayManualState>(AsShared());CharacterReadiness=Identity->OnReadinessChanged().AddLambda([Weak,E](bool){if(auto Self=Weak.Pin())if(Self->Current(E))Self->Snapshot();});CharacterIdentityChanged=Identity->OnIdentityChanged().AddLambda([Weak,E]{if(auto Self=Weak.Pin())if(Self->Current(E))Self->Snapshot();});
   Identity->RegisterComponent();if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("角色组件注册期间停止。"));
  }
  FGamePlatformCharacterInitializationContext C;C.HeroDefinitionId=A.Id(TEXT("HeroId"));C.SpawnGeneration=1;C.AvatarGeneration=int32(A.Int(TEXT("AvatarGeneration")));C.bPersistentCharacterIdRequired=false;
  FString Error;bool B=FGamePlatformCharacterInitializationExecutor::InitializeCharacter(*S,C,Error);Observe(TEXT("唯一项目初始化器实际受理结果：")+Error);return GameplayBool(B,TEXT("InitializeCharacter accepted; actual Definition/Ready via native events; no production admission"));
 }
 if(O==TEXT("BindAvatar"))return BindFixtureAvatar();
 if(O==TEXT("ClearAvatar")){S->ASC->ClearAbilityAvatar();return FGamePlatformResult::Success();}
 if(O==TEXT("EvaluateGate"))return S->ASC->EvaluateActivationEligibility();
 if(O==TEXT("GrantInput"))return GrantInput(FGameplayTag::RequestGameplayTag(A.Id(TEXT("InputTag")),false));
 if(O==TEXT("RevokeInput"))return RevokeInput();
 if(O==TEXT("Press"))return S->ASC->AbilityInputPressed(FGameplayTag::RequestGameplayTag(A.Id(TEXT("InputTag")),false),S->ASC->GetInputToken());
 if(O==TEXT("Release"))return S->ASC->AbilityInputReleased(FGameplayTag::RequestGameplayTag(A.Id(TEXT("InputTag")),false),S->ASC->GetInputToken());
 if(O==TEXT("ProcessInput")){int32 Before=S->CommittedInputs;S->ASC->ProcessAbilityInput();return GameplayBool(S->CommittedInputs>Before,TEXT("ProcessAbilityInput -> real GAS Commit"));}
 if(O==TEXT("ClearInput")){S->ASC->ClearAbilityInput();return FGamePlatformResult::Success();}
 if(O==TEXT("CommitInput")){if(!InputHandle.IsValid()){auto R=GrantInput(FGameplayTag::RequestGameplayTag(TEXT("Platform.Ability.Input.DemoManual"),false));if(!R.IsSuccess())return R;}int32 Before=S->CommittedInputs;bool B=S->ASC->TryActivateAbility(InputHandle);return GameplayBool(B&&S->CommittedInputs>Before,TEXT("GAS Commit test ability: fixture cost 25; project defaults unchanged"));}
 if(O==TEXT("PublishCast")||O==TEXT("DuplicateCast")){if(O==TEXT("PublishCast"))CastId=FGuid::NewGuid();return GameplayBool(S->CastFacts->PublishConfirmedCast(CastId,A.Id(TEXT("AbilityId")),TEXT("Demo.Manual.NoAppearance"),S->GetCombatAvatarGeneration(),A.Number(TEXT("Duration"))),TEXT("PublishConfirmedCast; isolated fact injection, not skill qualification"));}
 if(O==TEXT("CancelCast"))return GameplayBool(S->CastFacts->CancelConfirmedCast(CastId),TEXT("CancelConfirmedCast"));
 if(O==TEXT("SetTargetEnabled")){if(!Target.IsValid())return GameplayError(TEXT("Demo.Gameplay.Target"),TEXT("目标失效。"));Target->AITarget->SetAITargetEnabled(A.Bool(TEXT("Enabled")));return FGamePlatformResult::Success();}
 if(O==TEXT("AdvanceTarget")){if(!Target.IsValid())return GameplayError(TEXT("Demo.Gameplay.Target"),TEXT("目标失效。"));Target->AITarget->AdvanceTargetGeneration();return FGamePlatformResult::Success();}
 return GameplayError(TEXT("Demo.Gameplay.Unsupported"),TEXT("本分支未提供该操作。"));
}
void FGameplayManualState::Execute(FName O,const FManualArguments& A,FDivineBeastsDemoOperationCompletion D){
 auto Keep=AsShared();const auto E=Epoch;FGamePlatformResult R;
 if(O==TEXT("Snapshot")){auto* GS=World->GetGameState();if(!ExperienceSubscription.IsValid())Experience=GS?GS->FindComponentByClass<UGamePlatformExperienceComponent>():nullptr;Snapshot();D(Context.Token,Current(E)?FGamePlatformResult::Success():FGamePlatformResult::Cancelled(TEXT("观察期间停止。")));return;}
 if(O==TEXT("CreateFixture")){
  {TGuardValue<bool> Busy(bMutating,true);R=CreateFixture();}if(!Current(E)){D(Context.Token,FGamePlatformResult::Cancelled(TEXT("出生期间已停止。")));return;}if(!R.IsSuccess()){D(Context.Token,R);return;}
  FSoftObjectPath P(TEXT("/Engine/BasicShapes/Cube.Cube"));auto Weak=TWeakPtr<FGameplayManualState>(Keep);Load({P},[Weak,P]{auto S=Weak.Pin();auto* M=Cast<UStaticMesh>(P.ResolveObject());if(!S||!M)return GameplayError(TEXT("Demo.Gameplay.Marker"),TEXT("真实夹具标记Mesh缺失。"));const auto E=S->Epoch;for(auto F:{S->Source,S->Target,S->Alternate}){if(!F.IsValid())return GameplayError(TEXT("Demo.Gameplay.Fixture"),TEXT("夹具失效。"));F->Marker->SetStaticMesh(M);if(!S->Current(E))return FGamePlatformResult::Cancelled(TEXT("Mesh通知期间已停止。"));}S->Snapshot();return FGamePlatformResult::Success();},MoveTemp(D));return;
 }
 if(O==TEXT("ValidateDraft")){
  const FName HeroId=A.Id(TEXT("HeroId"));const auto Path=FDivineBeastsHeroCatalog::GetDefinitionAssetPath(HeroId);
  auto Weak=TWeakPtr<FGameplayManualState>(Keep);Load({Path},[Weak,HeroId]{auto S=Weak.Pin();if(!S)return FGamePlatformResult::Cancelled(TEXT("来源失效。"));auto P=IModularFeatures::Get().GetModularFeatureImplementations<IGamePlatformCharacterCreationProvider>(IGamePlatformCharacterCreationProvider::GetModularFeatureName());if(P.Num()!=1||!P[0])return GameplayError(TEXT("Demo.Gameplay.CreationProvider"),TEXT("真实提供者不唯一。"));FString Error;bool B=P[0]->ValidateCreationDraft(HeroId,{},Error);S->Observe(TEXT("真实已加载定义草稿校验：")+Error);return GameplayBool(B,TEXT("ValidateCreationDraft: local structure, backend permission separate"));},MoveTemp(D));return;
 }
 if(O==TEXT("LoadDefinition")||O==TEXT("LoadAIDefinition")){
  const auto P=A.Asset(TEXT("Definition"));DefinitionSelection=P;const auto Selection=++DefinitionSelectionGeneration;DefinitionObject.Reset();Hero.Reset();AI.Reset();DefinitionResult={};auto Weak=TWeakPtr<FGameplayManualState>(Keep);
  if(O==TEXT("LoadAIDefinition")){FGamePlatformResult Cleared;{TGuardValue<bool> Busy(bMutating,true);Cleared=Server(TEXT("UnPossessAI"),A);}if(!Cleared.IsSuccess()||!Current(E)){D(Context.Token,Current(E)?Cleared:FGamePlatformResult::Cancelled(TEXT("旧AI清理期间来源失活。")));return;}}
  Load({P},[Weak,P,Selection]{auto S=Weak.Pin();if(!S||S->DefinitionSelection!=P||S->DefinitionSelectionGeneration!=Selection)return FGamePlatformResult::Cancelled(TEXT("定义选择已换代。"));auto* O=P.ResolveObject();FGamePlatformResult R;
   if(S->Feature==TEXT("GamePlatformCharacter.definition")){auto* H=Cast<UGamePlatformHeroDefinition>(O);if(!H)return GameplayError(TEXT("Demo.Gameplay.DefinitionType"),TEXT("角色定义页拒绝非Hero资源。"));FString Error;R=GameplayBool(H->IsDefinitionValid(Error),TEXT("current Hero typed validation"));if(R.IsSuccess())S->Hero=H;}
   else if(S->Feature==TEXT("GamePlatformAbilitySystem.abilityset_definition")){auto* B=Cast<UGamePlatformAbilitySetDefinition>(O);if(!B)return GameplayError(TEXT("Demo.Gameplay.DefinitionType"),TEXT("能力集定义页拒绝非AbilitySet资源。"));R=B->ValidateDefinition();}
   else if(S->Feature==TEXT("GamePlatformAI.brain")||S->Feature==TEXT("GamePlatformAI.attack")){auto* AI=Cast<UGamePlatformAIDefinition>(O);if(!AI)return GameplayError(TEXT("Demo.Gameplay.DefinitionType"),TEXT("AI页拒绝非AIDefinition资源。"));FText Error;R=GameplayBool(AI->ValidateDefinition(Error),TEXT("current AI typed validation"));if(R.IsSuccess())S->AI=AI;}
   else return GameplayError(TEXT("Demo.Gameplay.DefinitionFeature"),TEXT("固定功能不接受定义加载。"));if(R.IsSuccess())S->DefinitionObject=O;return R;
  },[Weak,P,Selection,E,OriginalToken=Context.Token,OriginalWorld=World,Done=MoveTemp(D)](const auto& T,const auto& R){
   auto S=Weak.Pin();const auto IsOriginal=[&]{return S&&T==OriginalToken&&S->Context.Token==OriginalToken&&S->World==OriginalWorld&&S->Current(E)&&S->DefinitionSelection==P&&S->DefinitionSelectionGeneration==Selection;};
   if(!IsOriginal()){Done(OriginalToken,FGamePlatformResult::Cancelled(TEXT("定义完成来源或选择已失效。")));return;}
   S->DefinitionResult=R;if(!R.IsSuccess()){S->DefinitionObject.Reset();S->Hero.Reset();S->AI.Reset();}
   S->Snapshot(); // 用户观察者可以同步Stop、Reset或改选；返回后不能沿用先前成功。
   if(!IsOriginal()){Done(OriginalToken,FGamePlatformResult::Cancelled(TEXT("定义观察通知已停止或接管原操作。")));return;}
   Done(OriginalToken,R);
  });return;
 }
 if(O==TEXT("LoadAnimation")){
  auto Mesh=A.Asset(TEXT("Mesh")),Anim=A.Asset(TEXT("AnimClass"));auto Weak=TWeakPtr<FGameplayManualState>(Keep);Load({Mesh,Anim},[Weak,Mesh,Anim]{auto S=Weak.Pin();auto* M=Cast<USkeletalMesh>(Mesh.ResolveObject());auto* C=Cast<UClass>(Anim.ResolveObject());if(!S||!S->Source.IsValid()||!M||!C||!C->IsChildOf(UGamePlatformLocomotionAnimInstance::StaticClass()))return GameplayError(TEXT("Demo.Gameplay.AnimationAssets"),TEXT("需真实Manny Mesh及平台Locomotion派生AnimBlueprint。"));const auto E=S->Epoch;auto* F=S->Source.Get();F->GetMesh()->SetSkeletalMeshAsset(M);if(!S->Current(E))return FGamePlatformResult::Cancelled(TEXT("外观通知期间停止。"));F->GetMesh()->SetRelativeLocation(FVector(0,0,-90));F->GetMesh()->SetRelativeRotation(FRotator(0,-90,0));F->GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);F->GetMesh()->SetAnimInstanceClass(C);if(!S->Current(E))return FGamePlatformResult::Cancelled(TEXT("动画初始化期间停止。"));F->Marker->SetVisibility(false);return GameplayBool(F->GetMesh()->GetAnimInstance()!=nullptr,TEXT("native animation instance"));},MoveTemp(D));return;
 }
 if(O==TEXT("LoadOverlay")){auto P=A.Asset(TEXT("Overlay"));auto Weak=TWeakPtr<FGameplayManualState>(Keep);Load({P},[Weak,P]{auto S=Weak.Pin();auto* M=Cast<UMaterialInterface>(P.ResolveObject());if(S)S->Overlay=M;return GameplayBool(M!=nullptr,TEXT("preloaded Overlay material"));},MoveTemp(D));return;}
 {TGuardValue<bool> Busy(bMutating,true);
  if(O==TEXT("Reset"))R=Reset();else if(O==TEXT("ActivateFixture"))R=ActivateFixture(true);
  else if(Feature.ToString().StartsWith(TEXT("GamePlatformCombat.")))R=Combat(O,A);
  else if(Feature.ToString().StartsWith(TEXT("GamePlatformAnimation.")))R=Animation(O,A);
  else if(Feature.ToString().StartsWith(TEXT("GamePlatformQuest."))||O==TEXT("PossessAI")||O==TEXT("UnPossessAI")||O==TEXT("TryAttack")||O==TEXT("ForceDecision"))R=Server(O,A);
  else R=Gameplay(O,A);
 }
 if(Current(E))Snapshot();if(!Current(E))R=FGamePlatformResult::Cancelled(TEXT("同步平台事件期间本来源失活；已发生的权威结果不伪造回滚。"));D(Context.Token,R);
}
}
