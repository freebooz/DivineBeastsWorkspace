// Only explicit Standalone Hub URL override selects this real platform GameMode.
#pragma once
#include "Framework/GamePlatformGameModeBase.h"
#include "Framework/GamePlatformPlayerControllerBase.h"
#include "Contracts/DivineBeastsDemoRunTypes.h"
#include "DivineBeastsDemoGameplayHostMode.generated.h"
class AGamePlatformPlayerControllerBase;
UCLASS(NotBlueprintable,Transient)
class ADivineBeastsDemoGameplayHostMode final:public AGamePlatformGameModeBase {
 GENERATED_BODY()
public:
 ADivineBeastsDemoGameplayHostMode();
 bool IsExplicitOfflineHost()const;
 bool OwnsFixture(const FDivineBeastsDemoRunToken&)const;
 FGamePlatformResult AcquireFixture(const FDivineBeastsDemoRunToken&);
 FGamePlatformResult BeginFixtureExperience(const FDivineBeastsDemoRunToken&);
 FGamePlatformResult DrainFixtureExperience(const FDivineBeastsDemoRunToken&);
 FGamePlatformResult SubmitFixtureAdmission(const FDivineBeastsDemoRunToken&,bool bStale=false);
 FGamePlatformResult ConfirmFixturePreparation(const FDivineBeastsDemoRunToken&,bool bStale=false);
 FGamePlatformResult RevokeFixtureAdmission(const FDivineBeastsDemoRunToken&);
 FGamePlatformResult ReleaseFixture(const FDivineBeastsDemoRunToken&);
 FGamePlatformResult ValidateFixtureAdmission(const APlayerController&,const FGamePlatformVerifiedPlayerContext&)const;
 AGamePlatformPlayerControllerBase* GetFixtureController()const{return FixtureController.Get();}
 FString FixtureObservation()const;
 FSimpleMulticastDelegate OnFixtureChanged;
protected:void EndPlay(const EEndPlayReason::Type)override;
private:
 FDivineBeastsDemoRunToken Owner;
 FGamePlatformVerifiedPlayerContext Admission;
 FGuid WorldGeneration;
 FGamePlatformGameplayRegistration AuthorityRegistration,SpawnRegistration;
 TWeakObjectPtr<AGamePlatformPlayerControllerBase> FixtureController;
 TWeakObjectPtr<AActor> SpawnAnchor;
 FDelegateHandle LifecycleChanged;
 bool bOpening=false,bClosingFixture=false,bExperience=false,bAdmitted=false;
};
