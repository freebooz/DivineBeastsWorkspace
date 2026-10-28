#pragma once
#include "Scenarios/DivineBeastsDemoGameplayScenarios.h"
#include "Scenarios/DivineBeastsDemoGameplayFixture.h"
#include "Scenarios/DivineBeastsDemoManualHostSubsystem.h"
#include "Scenarios/DivineBeastsDemoPlatformManualBase.h"
#include "Types/GamePlatformDataLease.h"
#include "Interfaces/IGamePlatformGameplayService.h"
#include "GameplayAbilitySpecHandle.h"
#include "UObject/StrongObjectPtr.h"
class AGamePlatformCombatProjectile;
class UGamePlatformExperienceComponent;
class UGamePlatformQuestClientSubsystem;
class UGamePlatformHeroDefinition;
class UGamePlatformAIDefinition;
class UMaterialInterface;
class USkeletalMesh;
class UAnimInstance;
class IGamePlatformDataService;
class ULocalPlayer;
class ADivineBeastsDemoGameplayHostMode;
class UDivineBeastsCharacterComponent;
#if WITH_EDITOR
class AGamePlatformAIController;
#endif
namespace DivineBeasts::Demo {
struct FGameplayManualState:TSharedFromThis<FGameplayManualState>{
 FDivineBeastsDemoScenarioContext Context;FName Feature;
 TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ULocalPlayer> Player;
 TWeakObjectPtr<UDivineBeastsDemoManualHostSubsystem> Host;
 TStrongObjectPtr<UDivineBeastsDemoGameplayObserver> Observer;
 TWeakObjectPtr<ADivineBeastsDemoGameplayFixture> Source,Target,Alternate;
 TWeakObjectPtr<AGamePlatformCombatProjectile> Projectile;
 TWeakObjectPtr<AActor> Obstacle;
 TWeakObjectPtr<UGamePlatformExperienceComponent> Experience;
 FGamePlatformGameplayRegistration ExperienceSubscription;
 TWeakObjectPtr<UGamePlatformHeroDefinition> Hero;
 FSoftObjectPath DefinitionSelection;uint64 DefinitionSelectionGeneration=0;TWeakObjectPtr<UObject> DefinitionObject;FGamePlatformResult DefinitionResult;
 TWeakObjectPtr<ADivineBeastsDemoGameplayHostMode> GameplayHost;FDelegateHandle GameplayHostChanged;
 TWeakObjectPtr<UDivineBeastsCharacterComponent> CharacterIdentity;
 FDelegateHandle CharacterReadiness,CharacterIdentityChanged;
 TWeakObjectPtr<UGamePlatformAIDefinition> AI;
 TWeakObjectPtr<UMaterialInterface> Overlay;
#if WITH_EDITOR
 TWeakObjectPtr<AGamePlatformAIController> AIController;
#endif
 FGameplayAbilitySpecHandle InputHandle;FGuid CastId,LastDamageId,HitstopId,FlashId;
 FGamePlatformDataLease Resources;TArray<FGamePlatformDataLease> RetainedResources;FDivineBeastsDemoOperationCompletion Loading;
 FDelegateHandle WorldCleanup,QuestChanged,HitstopFinished;
 struct FFixtureSubscriptions {TWeakObjectPtr<ADivineBeastsDemoGameplayFixture> Actor;FDelegateHandle Binding,Eligibility,Combat,Cast,Transform;};
 TArray<FFixtureSubscriptions> FixtureSubscriptions;
 TArray<TPair<TWeakObjectPtr<UGamePlatformAbilitySystemComponent>,TArray<FDelegateHandle>>> AttributeHandlers;
 FName TouchedQuest;bool bOriginalTracked=false,bOwnTrack=false;
 bool bBound=false,bActive=false,bHost=false,bCleaning=false,bMutating=false;
 uint64 Epoch=1;
 bool Alive()const;bool Current(uint64 E)const{return Alive()&&Epoch==E;}
 IGamePlatformDataService* Data()const;
 FGamePlatformResult Prepare();FGamePlatformResult Cleanup();FGamePlatformResult Reset();
 void Observe(const FString&)const;void Snapshot()const;
 FGamePlatformResult CreateFixture();FGamePlatformResult ActivateFixture(bool);FGamePlatformResult BindFixtureAvatar();
 FGamePlatformResult GrantInput(FGameplayTag);FGamePlatformResult RevokeInput();
 FGamePlatformResult ReleaseResources();
 void Load(const TArray<FSoftObjectPath>&,TFunction<FGamePlatformResult()>,FDivineBeastsDemoOperationCompletion);
 void Execute(FName,const FManualArguments&,FDivineBeastsDemoOperationCompletion);
 FGamePlatformResult Gameplay(FName,const FManualArguments&);
 FGamePlatformResult Combat(FName,const FManualArguments&);
 FGamePlatformResult Animation(FName,const FManualArguments&);
 FGamePlatformResult Server(FName,const FManualArguments&);
};
inline FGamePlatformResult GameplayError(const TCHAR* C,const TCHAR* M){return FGamePlatformResult::Failure(C,M);}
#if WITH_DEV_AUTOMATION_TESTS
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateGameplayManualScenarioForTesting(UWorld&,FName,TSharedPtr<FGameplayManualState>&);
#endif
inline FGamePlatformResult GameplayBool(bool B,const TCHAR* API){auto R=B?FGamePlatformResult::Success():GameplayError(TEXT("Demo.Gameplay.Rejected"),TEXT("真实接口拒绝；见当前快照。"));R.Message=FString::Printf(TEXT("%s accepted=%d；受理与终态/人工验收分别记录。"),API,B);return R;}
}
