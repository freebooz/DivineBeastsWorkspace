#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Scenarios/DivineBeastsDemoGameplayState.h"
#include "Scenarios/DivineBeastsDemoGameplayHostMode.h"
#include "Tests/DivineBeastsDemoPresentationReentryFixture.h"
#include "Actors/GamePlatformCombatProjectile.h"
#include "Components/GamePlatformExperienceComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Framework/GamePlatformPlayerControllerBase.h"
#include "Framework/GamePlatformPlayerStateBase.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/UserWidget.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IGamePlatformDataService.h"
using namespace DivineBeasts::Demo;
namespace {
struct FFix1Live:TSharedFromThis<FFix1Live>{
 FAutomationTestBase* T=nullptr;TUniquePtr<IDivineBeastsDemoScenarioAdapter> Adapter;TSharedPtr<FGameplayManualState> State;FDivineBeastsDemoScenarioContext C;TStrongObjectPtr<UDivineBeastsDemoPresentationTestScreen> Widget;
 TArray<FDivineBeastsDemoParameterValue> Values;FGamePlatformResult Last;bool Ready=true,ArmStop=false,DidStop=false;int32 Step=0,Variant=0;double Deadline=0;FName Operation;int32 TypedAction=0,TypedLoadCallbacks=0,TypedCleanupCallbacks=0;bool TypedReentered=false;FGamePlatformResult TypedCleanupResult;FGamePlatformDataLease TypedOriginalLease;TFunction<bool(FFix1Live&)> Drive;
 bool Init(FAutomationTestBase& Test,FName F){T=&Test;UWorld* W=nullptr;if(GEngine)for(auto& V:GEngine->GetWorldContexts())if(auto* P=V.World())if(P->WorldType==EWorldType::Game&&P->GetNetMode()==NM_Standalone&&P->HasBegunPlay())W=P;
  if(!FParse::Param(FCommandLine::Get(),TEXT("GameplayManualNative"))||!W||!W->GetGameInstance()||!W->GetGameInstance()->GetFirstGamePlayer()){T->AddError(TEXT("HostRequired: real Standalone Hub, GameplayManualNative/DBAPluginDemo/GamePlatformManualFixture"));return false;}
  Widget.Reset(CreateWidget<UDivineBeastsDemoPresentationTestScreen>(W->GetGameInstance()->GetFirstGamePlayer()->GetPlayerController(W)));if(!Widget.IsValid())return false;
  C.World=W;C.SampleWidget=Widget.Get();C.Token.RunId=FGuid::NewGuid();C.Token.WorldId=FGuid::NewGuid();C.Token.Generation=1;C.Scenario=GameplayManualDefinition(F);for(auto P:C.Scenario.Parameters)Values.Add(P.DefaultValue);
  auto Weak=TWeakPtr<FFix1Live>(AsShared());C.PublishObservation=[Weak](const auto&,const FString&){
   if(auto R=Weak.Pin())if(R->TypedAction&&!R->TypedReentered&&R->State->DefinitionResult.IsSuccess()&&R->State->DefinitionObject.IsValid()){
    R->TypedReentered=true;R->TypedOriginalLease=R->State->Resources;auto Complete=[Weak](const auto& Token,const auto& Result){if(auto S=Weak.Pin()){S->T->TestTrue(TEXT("reentrant cleanup uses full original token"),Token==S->C.Token);++S->TypedCleanupCallbacks;S->TypedCleanupResult=Result;}};
    if(R->TypedAction==1)R->Adapter->Stop(MoveTemp(Complete));else R->Adapter->Execute(TEXT("Reset"),R->Values,MoveTemp(Complete));return;
   }
   if(auto R=Weak.Pin())if(R->ArmStop&&!R->DidStop&&R->State->GameplayHost.IsValid())if(auto* PC=R->State->GameplayHost->GetFixtureController())if(auto* PS=PC->GetPlayerState<AGamePlatformPlayerStateBase>())if(PS->GetLifecycleSnapshot().IsRegistered()){R->DidStop=true;R->Adapter->Stop([Weak](const auto&,const auto& Result){if(auto S=Weak.Pin())S->T->TestFalse(TEXT("reentrant Stop retains in-flight host"),Result.IsSuccess());});}};
  Adapter=CreateGameplayManualScenarioForTesting(*W,F,State);bool B=false;Adapter->Prepare(C,[&](const auto&,const auto& R){B=R.IsSuccess();if(!B)T->AddError(R.Message);});Deadline=FPlatformTime::Seconds()+60;return B;
 }
 void Issue(FName O){Ready=false;Operation=O;auto Weak=TWeakPtr<FFix1Live>(AsShared());Adapter->Execute(O,Values,[Weak,O](const auto& Token,const auto& R){if(auto S=Weak.Pin()){S->T->TestTrue(TEXT("full run identity on every callback"),Token==S->C.Token);if(O==TEXT("LoadDefinition"))++S->TypedLoadCallbacks;S->Last=R;S->Ready=true;}});}
 void Stop(){Ready=false;auto Weak=TWeakPtr<FFix1Live>(AsShared());Adapter->Stop([Weak](const auto& Token,const auto& R){if(auto S=Weak.Pin()){S->T->TestTrue(TEXT("full cleanup identity"),Token==S->C.Token);S->Last=R;S->Ready=true;}});}
 void SetPath(const TCHAR* P){Values[0].AssetPathValue=FSoftObjectPath(P);}
 bool Tick(){if(FPlatformTime::Seconds()>Deadline){T->AddError(TEXT("Timeout waiting for actual service state; no forced success."));Stop();return true;}if(!Ready)return false;return Drive?Drive(*this):true;}
 bool Finish(){T->TestTrue(TEXT("Stop cleanup succeeded"),Last.IsSuccess());T->TestFalse(TEXT("exclusive host released"),State->Host.IsValid()&&State->Host->Owns(C.Token));return true;}
};
class FFix1Wait final:public IAutomationLatentCommand{TSharedRef<FFix1Live> R;public:explicit FFix1Wait(TSharedRef<FFix1Live> V):R(V){}bool Update()override{return R->Tick();}};
bool DefinitionRegression(FAutomationTestBase& T,int32 Variant){auto R=MakeShared<FFix1Live>();FName F=Variant==0?TEXT("GamePlatformCharacter.definition"):Variant==1?TEXT("GamePlatformAbilitySystem.abilityset_definition"):TEXT("GamePlatformAI.brain");if(!R->Init(T,F))return false;R->Variant=Variant;
 R->Drive=[](FFix1Live& S){FName Load=S.Variant==2?TEXT("LoadAIDefinition"):TEXT("LoadDefinition");FName Validate=S.Variant==2?TEXT("PossessAI"):TEXT("ValidateDefinition");
  if(S.Step==0){if(S.Variant==2){S.Step=1;S.Issue(TEXT("CreateFixture"));}else{S.Step=2;S.Issue(Load);}return false;}
  if(S.Step==1){S.T->TestTrue(TEXT("fixture resources loaded"),S.Last.IsSuccess());++S.Step;S.Issue(Load);return false;}
  if(S.Step==2){S.T->TestTrue(TEXT("valid typed selection accepted"),S.Last.IsSuccess());S.T->TestTrue(TEXT("current typed object recorded"),S.State->DefinitionObject.IsValid());S.T->TestTrue(TEXT("actual current resource lease succeeded"),S.State->Resources.IsValid()&&S.State->Data()&&S.State->Data()->GetLeaseState(S.State->Resources)==EGamePlatformDataRequestState::Succeeded);if(S.Variant!=2){S.T->TestFalse(TEXT("pure definition has no fabricated Character prerequisite"),S.State->Source.IsValid());S.T->TestTrue(TEXT("own lease names exactly current selected path"),S.State->Resources.ResourcePaths.Num()==1&&S.State->Resources.ResourcePaths[0]==S.Values[0].AssetPathValue);}++S.Step;if(S.Variant==2){S.SetPath(TEXT("/DBAGameplay/Definitions/DA_Hero_Zodiac_Rat.DA_Hero_Zodiac_Rat"));S.Issue(Load);}else S.Issue(Validate);return false;}
  if(S.Step==3){if(S.Variant!=2){S.T->TestTrue(TEXT("valid current definition validates"),S.Last.IsSuccess());S.SetPath(S.Variant==0?TEXT("/DBAGameplay/Abilities/Placeholders/DA_DBA_Rat_PlaceholderAbilitySet.DA_DBA_Rat_PlaceholderAbilitySet"):TEXT("/DBAGameplay/Definitions/DA_Hero_Zodiac_Rat.DA_Hero_Zodiac_Rat"));S.Issue(Load);++S.Step;return false;}S.Step=4;}
  if(S.Step==4){S.T->TestFalse(TEXT("wrong domain fails on fixed feature"),S.Last.IsSuccess());S.T->TestFalse(TEXT("old typed object was cleared"),S.State->DefinitionObject.IsValid());S.T->TestFalse(TEXT("old Hero cannot be reused"),S.State->Hero.IsValid());S.T->TestFalse(TEXT("old AI cannot be reused"),S.State->AI.IsValid());++S.Step;S.Issue(Validate);return false;}
  if(S.Step==5){S.T->TestFalse(TEXT("Validate/Possess after wrong domain cannot recover old result"),S.Last.IsSuccess());S.T->AddExpectedError(TEXT("MissingGameplayDefinition"),EAutomationExpectedErrorFlags::Contains,0);S.SetPath(TEXT("/DBAUIPack_Core/Development/PluginDemos/Fixtures/Gameplay/MissingGameplayDefinition.MissingGameplayDefinition"));++S.Step;S.Issue(Load);return false;}
  if(S.Step==6){S.T->TestFalse(TEXT("missing resource actual load fails"),S.Last.IsSuccess());++S.Step;S.Issue(Validate);return false;}
  if(S.Step==7){S.T->TestFalse(TEXT("Validate/Possess after failure cannot return old success"),S.Last.IsSuccess());++S.Step;S.Stop();return false;}return S.Finish();
 };ADD_LATENT_AUTOMATION_COMMAND(FFix1Wait(R));return true;}
bool TypedCompletionReentry(FAutomationTestBase& T,bool Reset){auto R=MakeShared<FFix1Live>();if(!R->Init(T,TEXT("GamePlatformCharacter.definition")))return false;R->TypedAction=Reset?2:1;
 R->Drive=[](FFix1Live& S){
  if(S.Step==0){++S.Step;S.Issue(TEXT("LoadDefinition"));return false;}
  if(S.Step==1){S.T->TestTrue(TEXT("actual successful typed observation invoked synchronous cleanup"),S.TypedReentered);S.T->TestEqual(TEXT("original load completes exactly once"),S.TypedLoadCallbacks,1);S.T->TestEqual(TEXT("nested cleanup completes exactly once"),S.TypedCleanupCallbacks,1);S.T->TestTrue(TEXT("actual nested cleanup accepted"),S.TypedCleanupResult.IsSuccess());S.T->TestTrue(TEXT("old completed load is Cancelled after observation cleanup"),S.Last.Status==FGamePlatformResult::Cancelled(TEXT("comparison")).Status);
   S.T->TestTrue(TEXT("captured genuine original lease was valid"),S.TypedOriginalLease.IsValid());S.T->TestTrue(TEXT("same Data issuer reports exact original lease Released"),S.State->Data()&&S.State->Data()->GetLeaseState(S.TypedOriginalLease)==EGamePlatformDataRequestState::Released);S.T->TestFalse(TEXT("original Data resource lease released"),S.State->Resources.IsValid());S.T->TestTrue(TEXT("no retained leases created by pure definition case"),S.State->RetainedResources.IsEmpty());S.T->TestFalse(TEXT("no stale typed object survives cleanup"),S.State->DefinitionObject.IsValid());S.T->TestFalse(TEXT("no stale Hero survives cleanup"),S.State->Hero.IsValid());S.T->TestFalse(TEXT("current selection cleared by Reset/Stop"),S.State->DefinitionSelection.IsValid());
   if(S.TypedAction==2){S.T->TestTrue(TEXT("Reset retains this run and its exact ManualHost"),S.State->Alive());++S.Step;S.Issue(TEXT("ValidateDefinition"));return false;}
   S.T->TestFalse(TEXT("Stop released exact ManualHost"),S.State->Host.IsValid()&&S.State->Host->Owns(S.C.Token));S.Step=3;S.Stop();return false;
  }
  if(S.Step==2){S.T->TestFalse(TEXT("Validate after synchronous Reset cannot fall back"),S.Last.IsSuccess());++S.Step;S.Stop();return false;}
  S.T->TestEqual(TEXT("later Stop never re-completes original load"),S.TypedLoadCallbacks,1);return S.Finish();
 };ADD_LATENT_AUTOMATION_COMMAND(FFix1Wait(R));return true;}
bool HostRegression(FAutomationTestBase& T,bool Reenter){auto R=MakeShared<FFix1Live>();if(!R->Init(T,TEXT("GamePlatformGameplay.admission_spawn")))return false;R->Variant=Reenter?1:0;
 if(!Cast<ADivineBeastsDemoGameplayHostMode>(R->C.World->GetAuthGameMode())){T.AddError(TEXT("HostRequired: fresh Hub?game=/Script/DivineBeastsDemoClient.DivineBeastsDemoGameplayHostMode + DBAManualGameplayHost/FoundationGameplayOffline/FoundationWorld/DBAManualWorldHost. Each Host test needs its own process/world."));return false;}
 R->Drive=[](FFix1Live& S){
  if(S.Step==0){++S.Step;S.Issue(TEXT("PrepareGameplayHost"));return false;}
  if(S.Step==1){S.T->TestTrue(TEXT("real mode acquired exact run"),S.Last.IsSuccess());++S.Step;S.Issue(TEXT("BeginExperience"));return false;}
  if(S.Step==2){S.T->TestTrue(TEXT("real BeginExperience accepted"),S.Last.IsSuccess());if(!S.State->Experience.IsValid()||!S.State->Experience->GetExperienceSnapshot().IsServerActive())return false;++S.Step;S.Issue(TEXT("StaleFixtureAdmission"));return false;}
  if(S.Step==3){S.T->TestFalse(TEXT("mismatched local admission denied by actual authority"),S.Last.IsSuccess());S.ArmStop=S.Variant==1;++S.Step;S.Issue(TEXT("SubmitFixtureAdmission"));return false;}
  if(S.Step==4){if(S.Variant==1){S.T->TestTrue(TEXT("real player lifecycle event triggered Stop"),S.DidStop);S.T->TestFalse(TEXT("old submit cannot return success after Stop"),S.Last.IsSuccess());S.Step=10;S.Stop();return false;}S.T->TestTrue(TEXT("actual local admission submitted"),S.Last.IsSuccess());auto* C=S.State->GameplayHost->GetFixtureController();auto* PS=C?C->GetPlayerState<AGamePlatformPlayerStateBase>():nullptr;if(!PS||PS->GetLifecycleSnapshot().Stage!=EGamePlatformPlayerStage::AwaitingClient)return false;S.T->TestTrue(TEXT("native platform spawned and possessed real Pawn"),C->GetPawn()!=nullptr);++S.Step;S.Issue(TEXT("StaleFixturePreparation"));return false;}
  if(S.Step==5){S.T->TestFalse(TEXT("wrong preparation token rejected"),S.Last.IsSuccess());++S.Step;S.Issue(TEXT("ConfirmFixturePreparation"));return false;}
  if(S.Step==6){S.T->TestTrue(TEXT("actual native current preparation accepted"),S.Last.IsSuccess());auto* H=S.State->GameplayHost.Get();S.T->TestTrue(TEXT("real isolated platform lifecycle Active"),H->IsPlayerGameplayActive(*H->GetFixtureController()));++S.Step;S.Issue(TEXT("RevokeFixtureAdmission"));return false;}
  if(S.Step==7){S.T->TestTrue(TEXT("actual admission revoke"),S.Last.IsSuccess());auto* C=S.State->GameplayHost->GetFixtureController();S.T->TestTrue(TEXT("platform revoke removed controlled Pawn"),C&&!C->GetPawn());++S.Step;S.Issue(TEXT("DrainExperience"));return false;}
  if(S.Step==8){S.T->TestTrue(TEXT("real drain accepted"),S.Last.IsSuccess());S.T->TestTrue(TEXT("actual Experience Released"),S.State->Experience->GetExperienceSnapshot().Stage==EGamePlatformExperienceStage::Released);S.Step=10;S.Stop();return false;}
  S.T->TestFalse(TEXT("per-run mode fixture released"),S.State->GameplayHost.IsValid());return S.Finish();
 };ADD_LATENT_AUTOMATION_COMMAND(FFix1Wait(R));return true;}
bool ProjectileRegression(FAutomationTestBase& T){auto R=MakeShared<FFix1Live>();if(!R->Init(T,TEXT("GamePlatformCombat.projectile")))return false;
 R->Drive=[](FFix1Live& S){static const FName Cases[]={TEXT("LaunchMovingTarget"),TEXT("LaunchThenSwitch"),TEXT("LaunchThenInvalidate"),TEXT("LaunchStraight"),TEXT("LaunchBlocked")};
  if(S.Step==0){++S.Step;S.Issue(TEXT("CreateFixture"));return false;}
  if(S.Step==1){S.T->TestTrue(TEXT("independent case fixture created"),S.Last.IsSuccess());++S.Step;S.Issue(TEXT("ActivateFixture"));return false;}
  if(S.Step==2){++S.Step;S.Issue(Cases[S.Variant]);return false;}
  if(S.Step==3){S.T->TestTrue(TEXT("case actual native launch accepted"),S.Last.IsSuccess());if(!S.State->Projectile.IsValid()){S.T->AddError(TEXT("actual projectile missing"));S.Stop();return true;}if(!S.State->Projectile->IsFlightFinished())return false;const auto F=S.State->Projectile->GetFlightFacts();
   if(S.Variant==0||S.Variant==1)S.T->TestTrue(TEXT("moving/original target collision occurred"),F.bImpacted&&S.State->Target.IsValid()&&S.State->Target->Combat->GetCombatHealth()<100);
   if(S.Variant==2)S.T->TestFalse(TEXT("target invalidation terminates without fabricated impact"),F.bImpacted);
   if(S.Variant==3)S.T->TestTrue(TEXT("no-target flight stays straight"),FMath::Abs(S.State->Projectile->GetActorLocation().Y-100)<.1);
   if(S.Variant==4)S.T->TestTrue(TEXT("preplaced real obstacle blocks before target"),F.bImpacted&&S.State->Target.IsValid()&&S.State->Target->Combat->GetCombatHealth()==100);
   ++S.Step;S.Issue(TEXT("Reset"));return false;
  }
  if(S.Step==4){S.T->TestTrue(TEXT("owned case reset clean"),S.Last.IsSuccess());if(++S.Variant<5){S.Step=0;return false;}++S.Step;S.Stop();return false;}return S.Finish();
 };ADD_LATENT_AUTOMATION_COMMAND(FFix1Wait(R));return true;}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayHeroSelectionLive,"DivineBeasts.PluginDemos.Gameplay.Fix1.Live.HeroSelection",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FGameplayHeroSelectionLive::RunTest(const FString&){return DefinitionRegression(*this,0);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayAbilitySelectionLive,"DivineBeasts.PluginDemos.Gameplay.Fix1.Live.AbilitySetSelection",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FGameplayAbilitySelectionLive::RunTest(const FString&){return DefinitionRegression(*this,1);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayAISelectionLive,"DivineBeasts.PluginDemos.Gameplay.Fix1.Live.AISelection",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FGameplayAISelectionLive::RunTest(const FString&){return DefinitionRegression(*this,2);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayHostLifecycleLive,"DivineBeasts.PluginDemos.Gameplay.Fix1.Live.HostLifecycle",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FGameplayHostLifecycleLive::RunTest(const FString&){return HostRegression(*this,false);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayHostStopLive,"DivineBeasts.PluginDemos.Gameplay.Fix1.Live.HostStopReentry",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FGameplayHostStopLive::RunTest(const FString&){return HostRegression(*this,true);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayProjectileCasesLive,"DivineBeasts.PluginDemos.Gameplay.Fix1.Live.ProjectileCases",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FGameplayProjectileCasesLive::RunTest(const FString&){return ProjectileRegression(*this);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayTypedStopLive,"DivineBeasts.PluginDemos.Gameplay.Fix2.Live.TypedCompletionStop",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FGameplayTypedStopLive::RunTest(const FString&){return TypedCompletionReentry(*this,false);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayTypedResetLive,"DivineBeasts.PluginDemos.Gameplay.Fix2.Live.TypedCompletionReset",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FGameplayTypedResetLive::RunTest(const FString&){return TypedCompletionReentry(*this,true);}
#endif
