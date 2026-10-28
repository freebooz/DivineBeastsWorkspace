// 世界人工页私有运行所有权；真实平台服务由引擎GI/World集合拥有。
#pragma once
#include "Scenarios/DivineBeastsDemoWorldScenarios.h"
#include "Scenarios/DivineBeastsDemoWorldObserver.h"
#include "Scenarios/DivineBeastsDemoManualHostSubsystem.h"
#include "Scenarios/DivineBeastsDemoPlatformManualBase.h"
#include "Types/GamePlatformDataLease.h"
#include "Types/GamePlatformRegion.h"
#include "Types/GamePlatformWorldStreaming.h"
#include "Types/GamePlatformPCGTypes.h"
#include "Interfaces/IGamePlatformWorldReadinessContributor.h"
#include "Subsystems/GamePlatformWeatherWorldSubsystem.h"
#include "Subsystems/GamePlatformWaterClientWorldSubsystem.h"
#include "Subsystems/GamePlatformTelemetrySubsystem.h"
#include "UObject/StrongObjectPtr.h"
#if WITH_EDITOR
#include "Types/GamePlatformNavigationTypes.h"
#include "Performance/GamePlatformPerformanceTestRunner.h"
#include "Audit/GamePlatformAssetAudit.h"
#include "Review/GamePlatformReviewTypes.h"
#endif
class AGamePlatformWaterRegion;
class UGamePlatformInteractorComponent;
class UGamePlatformInteractableComponent;
class UGamePlatformWeatherPresetDefinition;
class UGamePlatformPCGProfileDefinition;
class UPCGComponent;
class UBoxComponent;
class IGamePlatformDataService;
class UStaticMesh;
class ULocalPlayer;
#if WITH_EDITOR
class AGamePlatformNavigationSmartLinkProxy;
class UGamePlatformReviewReport;
class UGamePlatformPerformanceTestRunner;
class UGamePlatformNavigationAgentProfile;
class AGamePlatformPCGWorldDirector;
class AGamePlatformPCGExclusionActor;
class UPCGGraph;
#endif
namespace DivineBeasts::Demo {
struct FWorldInteractionFixture {double Distance=150,HoldSeconds=2;FName OptionId;int32 CommitKind=1,Charges=3;bool bLineOfSight=true;};
FWorldInteractionFixture DecodeWorldInteractionFixture(const FManualArguments& A);
struct FWorldManualContributor final:IGamePlatformWorldReadinessContributor {
 int32 Mode=0;
 FGamePlatformResult Evaluate(const FGamePlatformWorldContext&)const override {
  if(Mode==1)return FGamePlatformResult::Success();
  if(Mode==2)return FGamePlatformResult::Failure(TEXT("Demo.World.ContributorFailure"),TEXT("本开发贡献者显式报告失败，不改基础事实。"));
  return FGamePlatformResult();
 }
};
struct FWorldManualState:TSharedFromThis<FWorldManualState> {
 FDivineBeastsDemoScenarioContext Context;
 TWeakObjectPtr<UWorld> World;
 TWeakObjectPtr<ULocalPlayer> Player;
 TWeakObjectPtr<UDivineBeastsDemoManualHostSubsystem> Host;
 TStrongObjectPtr<UDivineBeastsDemoWorldObserver> Observer;
 FName Feature;
 bool bBound=false,bActive=false,bCleaning=false,bHost=false;
 uint64 Epoch=1;
 FDelegateHandle WorldCleanup,WeatherChanged,VisualChanged;
 TWeakObjectPtr<AActor> Anchor,Target;
 TWeakObjectPtr<AGamePlatformWaterRegion> WaterRegion;
 TWeakObjectPtr<UGamePlatformWaterInteractorComponent> WaterInteractor;
 TWeakObjectPtr<UGamePlatformInteractorComponent> Interactor;
 TWeakObjectPtr<UGamePlatformInteractableComponent> Interactable;
 TWeakObjectPtr<UPCGComponent> PCGComponent;
 // 仅本Run PCG夹具的真实采样体积；组件完全退役后才撤销，不接管外部Primitive。
 TWeakObjectPtr<UBoxComponent> PCGBounds;
 FGamePlatformWorldRegistration RegionRegistration,RegionSubscription,ContributorRegistration,ReadinessSubscription;
 FGamePlatformWorldStreamingHandle Streaming;
 TSharedPtr<FWorldManualContributor> Contributor;
 FGamePlatformPCGHandle Generation;
 FGamePlatformPCGSubscription GenerationSubscription;
 FGuid WeatherScope,AtmosphereScope,ProjectionScope,WaterScope,TelemetryScope,Reservation;
 FGamePlatformDataLease AssetLease;
 FGamePlatformDataLease DefinitionLease;
 FDivineBeastsDemoOperationCompletion LoadingCompletion;
#if WITH_EDITOR
 TWeakObjectPtr<AActor> Invoker;
 TWeakObjectPtr<AGamePlatformNavigationSmartLinkProxy> Link;
 TArray<TWeakObjectPtr<AGamePlatformNavigationSmartLinkProxy>> RetiringLinks;
 bool bSpawningLink=false;
 bool bAuthoringGraph=false;
 FString SavedRealizedGraph; // 持久产物不是Run临时资源；Stop不删除。
 FGamePlatformResult CreateOwnedSmartLink(double Distance,bool bEnabled,FName TraversalType);
 TWeakObjectPtr<UGamePlatformNavigationAgentProfile> NavProfile;
 bool bOwnNavProfile=false;
 TWeakObjectPtr<AGamePlatformPCGWorldDirector> Director;
 TWeakObjectPtr<AGamePlatformPCGExclusionActor> Exclusion;
 bool bExclusionRegistered=false;
 TStrongObjectPtr<UPCGGraph> SpatialGraph;
 FGamePlatformNavigationRequestHandle PathRequest;
 TStrongObjectPtr<UGamePlatformReviewReport> Review;
 TStrongObjectPtr<UGamePlatformPerformanceTestRunner> Runner;
 FName RuleProvider;
 FGamePlatformPerformanceRunResult Performance;
 bool bHasPerformance=false;
 TArray<FGamePlatformAssetAuditRow> AssetRows;
#endif
 bool Alive()const;
 bool Current(uint64 E)const{return Alive()&&Epoch==E;}
 UGameInstance* Instance()const;
 IGamePlatformDataService* Data()const;
 void Observe(FString Text)const;
 void Snapshot(FName Event=NAME_None)const;
 FGamePlatformResult Prepare();
 FGamePlatformResult Cleanup();
 FGamePlatformResult CleanupEditor();
 FGamePlatformResult ResetObjects();
 AActor* SpawnAnchor(const FVector& Position);
 FGamePlatformResult CreateWater(const FManualArguments& A);
 FGamePlatformResult CreateInteraction(const FWorldInteractionFixture& A,UStaticMesh* VisualMesh);
 void Execute(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done);
 void WorldOperation(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done);
 void WeatherOperation(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done);
 void WaterOperation(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done);
 void PCGOperation(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done);
 void InteractionOperation(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done);
 void TelemetryOperation(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done);
 void EditorOperation(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done);
 void Load(const FSoftObjectPath& P,TFunction<FGamePlatformResult(UObject*)> Apply,FDivineBeastsDemoOperationCompletion Done);
 FGamePlatformResult ReleaseAsset();
};
#if WITH_DEV_AUTOMATION_TESTS
/** 同一个真实适配器/执行路径的测试观察口；不初始化服务，不提供玩法写入旁路。 */
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateWorldManualScenarioForTesting(UWorld& World,FName Feature,TSharedPtr<FWorldManualState>& OutState);
#endif
inline FGamePlatformResult WorldError(const TCHAR* C,const TCHAR* M){return FGamePlatformResult::Failure(FName(C),M);}
inline FGamePlatformResult WorldBool(bool B,const TCHAR* Operation){return B?FGamePlatformResult::Success():FGamePlatformResult::Failure(TEXT("Demo.World.Rejected"),FString::Printf(TEXT("真实接口拒绝：%s；见观察快照，不伪造成功。"),Operation));}
}
