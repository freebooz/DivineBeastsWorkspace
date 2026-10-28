// Requires the actual engine-owned Standalone Hub. Never creates a second GI/World/LP service.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Scenarios/DivineBeastsDemoGameplayState.h"
#include "Tests/DivineBeastsDemoPresentationReentryFixture.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
using namespace DivineBeasts::Demo;
namespace {
struct FGameplayLiveRun:TSharedFromThis<FGameplayLiveRun>{
 FAutomationTestBase* Test=nullptr;TUniquePtr<IDivineBeastsDemoScenarioAdapter> Adapter;TSharedPtr<FGameplayManualState> State;
 TStrongObjectPtr<UDivineBeastsDemoPresentationTestScreen> Widget;FDivineBeastsDemoScenarioContext Context;
 TArray<FDivineBeastsDemoParameterValue> Values;FGamePlatformResult Last;bool Ready=true,bReenter=false,bReentered=false;int32 Stage=0,Kind=0,Completions=0;double Deadline=0;float Health=0;
 bool Init(FAutomationTestBase& T,int32 K){Test=&T;Kind=K;UWorld* W=nullptr;if(GEngine)for(const auto& C:GEngine->GetWorldContexts())if(auto* Candidate=C.World())if(Candidate->WorldType==EWorldType::Game&&Candidate->GetNetMode()==NM_Standalone&&Candidate->HasBegunPlay()&&Candidate->GetGameInstance())W=Candidate;
  if(!FParse::Param(FCommandLine::Get(),TEXT("GameplayManualNative"))||!W||!W->GetGameInstance()->GetFirstGamePlayer()){T.AddError(TEXT("HostRequired: -game real Hub -DBAPluginDemo -GamePlatformManualFixture -GameplayManualNative; no Editor-world substitute."));return false;}
  auto* PC=W->GetGameInstance()->GetFirstGamePlayer()->GetPlayerController(W);Widget.Reset(CreateWidget<UDivineBeastsDemoPresentationTestScreen>(PC));if(!Widget.IsValid())return false;
  FName F=K==2?FName(TEXT("GamePlatformCharacter.initialize")):FName(TEXT("GamePlatformCombat.damage_heal"));Context.World=W;Context.SampleWidget=Widget.Get();Context.Token.RunId=FGuid::NewGuid();Context.Token.WorldId=FGuid::NewGuid();Context.Token.Generation=1;Context.Scenario=GameplayManualDefinition(F);for(auto P:Context.Scenario.Parameters)Values.Add(P.DefaultValue);
  auto Weak=TWeakPtr<FGameplayLiveRun>(AsShared());Context.PublishObservation=[Weak](const auto& Token,const FString&){auto S=Weak.Pin();if(!S||!S->bReenter||S->bReentered||!S->State||!S->State->Target.IsValid()||S->State->Target->Combat->GetCombatHealth()>=100)return;S->bReentered=true;S->Adapter->Stop([Weak,Token](const auto& T,const auto& R){if(auto P=Weak.Pin()){P->Test->TestTrue(TEXT("full token during reentry"),T==Token);P->Test->TestFalse(TEXT("in-flight native stack cannot release owned resources"),R.IsSuccess());}});};
  Adapter=CreateGameplayManualScenarioForTesting(*W,F,State);bool Prepared=false;Adapter->Prepare(Context,[&](const auto& Token,const auto& R){Prepared=R.IsSuccess();T.TestTrue(TEXT("original run token"),Token==Context.Token);if(!Prepared)T.AddError(R.Message);});Deadline=FPlatformTime::Seconds()+45;return Prepared;
 }
 void Issue(FName O){Ready=false;auto Weak=TWeakPtr<FGameplayLiveRun>(AsShared());Adapter->Execute(O,Values,[Weak](const auto& T,const auto& R){if(auto S=Weak.Pin()){S->Test->TestTrue(TEXT("exact token callback"),T==S->Context.Token);S->Last=R;S->Ready=true;++S->Completions;}});}
 void Stop(){Ready=false;auto Weak=TWeakPtr<FGameplayLiveRun>(AsShared());Adapter->Stop([Weak](const auto& T,const auto& R){if(auto S=Weak.Pin()){S->Test->TestTrue(TEXT("exact stop token"),T==S->Context.Token);S->Last=R;S->Ready=true;++S->Completions;}});}
 bool Update(){
  if(FPlatformTime::Seconds()>Deadline){Test->AddError(TEXT("TimedOut waiting for actual Data/character state; no forced success."));Stop();return true;}
  if(!Ready)return false;
  if(Stage==0){++Stage;Issue(TEXT("CreateFixture"));return false;}
  if(Stage==1){if(!Test->TestTrue(TEXT("owned fixture resources genuinely loaded"),Last.IsSuccess())){Stop();return true;}Test->TestTrue(TEXT("authority character/ASC/Combat host"),State->Source.IsValid()&&State->Source->HasAuthority()&&State->Source->Combat->GetCombatAttributeSet());++Stage;Issue(Kind==2?TEXT("InitializeCharacter"):TEXT("ActivateFixture"));return false;}
  if(Kind==2){
   if(Stage==2){if(!Test->TestTrue(TEXT("unique initializer accepted real Hero.Zodiac.Rat"),Last.IsSuccess())){Stop();return true;}if(!State->CharacterIdentity.IsValid()||!State->CharacterIdentity->IsCharacterReady())return false;
    auto* C=State->CharacterIdentity.Get();Test->TestEqual(TEXT("real identity"),C->GetHeroDefinitionId(),FName(TEXT("Hero.Zodiac.Rat")));Test->TestTrue(TEXT("actual asset version confirmed"),C->GetDefinitionVersion()>0&&C->GetLoadedDefinition()!=nullptr);if(auto* D=C->GetLoadedDefinition())Test->TestEqual(TEXT("actual initializer applied native capsule envelope"),State->Source->GetCapsuleComponent()->GetUnscaledCapsuleRadius(),D->SpawnEnvelope.CapsuleRadius);++Stage;Stop();return false;
   }
  }else{
   if(Stage==2){if(!Test->TestTrue(TEXT("fixture eligibility active"),Last.IsSuccess())){Stop();return true;}Health=State->Target->Combat->GetCombatHealth();bReenter=Kind==1;++Stage;Issue(TEXT("Damage"));return false;}
   if(Stage==3){
    if(Kind==1){Test->TestTrue(TEXT("attribute event reentered Stop"),bReentered);Test->TestFalse(TEXT("old command cannot complete success after Stop"),Last.IsSuccess());Test->TestTrue(TEXT("failed Stop retains owned target until retry"),State->Target.IsValid());Stage=6;Stop();return false;}
    Test->TestTrue(TEXT("real GAS damage completed"),Last.IsSuccess());Test->TestTrue(TEXT("authority health reduced by actual result"),State->Target->Combat->GetCombatHealth()<Health);Health=State->Target->Combat->GetCombatHealth();++Stage;Issue(TEXT("DuplicateDamage"));return false;
   }
   if(Stage==4){Test->TestEqual(TEXT("same EventId cannot settle twice"),State->Target->Combat->GetCombatHealth(),Health);++Stage;Issue(TEXT("StaleDamage"));return false;}
   if(Stage==5){Test->TestFalse(TEXT("stale avatar denied"),Last.IsSuccess());Test->TestEqual(TEXT("denied request does not alter health"),State->Target->Combat->GetCombatHealth(),Health);++Stage;Stop();return false;}
  }
  Test->TestTrue(TEXT("explicit Stop confirmed cleanup"),Last.IsSuccess());Test->TestFalse(TEXT("exclusive host released for full token"),State->Host.IsValid()&&State->Host->Owns(Context.Token));Test->TestFalse(TEXT("own resource lease released"),State->Resources.IsValid());Test->TestTrue(TEXT("own retained resources released, no whole-GI zero assertion"),State->RetainedResources.IsEmpty());Test->TestFalse(TEXT("owned fixture released"),State->Source.IsValid());return true;
 }
};
class FGameplayLiveWait final:public IAutomationLatentCommand {TSharedRef<FGameplayLiveRun> R;public:explicit FGameplayLiveWait(TSharedRef<FGameplayLiveRun> V):R(V){}bool Update()override{return R->Update();}};
bool Start(FAutomationTestBase& T,int32 Kind){auto R=MakeShared<FGameplayLiveRun>();if(!R->Init(T,Kind))return false;ADD_LATENT_AUTOMATION_COMMAND(FGameplayLiveWait(R));return true;}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBAGameplayCombatLiveTest,"DivineBeasts.PluginDemos.Gameplay.Live.CombatLifecycle",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FDBAGameplayCombatLiveTest::RunTest(const FString&){return Start(*this,0);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBAGameplayReentryLiveTest,"DivineBeasts.PluginDemos.Gameplay.Live.StopDuringDamage",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FDBAGameplayReentryLiveTest::RunTest(const FString&){return Start(*this,1);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBAGameplayInitializeLiveTest,"DivineBeasts.PluginDemos.Gameplay.Live.UniqueCharacterInitialization",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FDBAGameplayInitializeLiveTest::RunTest(const FString&){return Start(*this,2);}
#endif
