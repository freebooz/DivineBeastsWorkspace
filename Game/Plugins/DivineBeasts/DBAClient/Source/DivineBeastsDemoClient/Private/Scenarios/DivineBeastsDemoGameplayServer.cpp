// Server-module references are restricted to this Editor-only private bridge.
#include "Scenarios/DivineBeastsDemoGameplayState.h"
#include "Components/GamePlatformAIStateComponent.h"
#include "Data/GamePlatformAIDefinition.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Services/GamePlatformQuestClientSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#if WITH_EDITOR
#include "Controllers/GamePlatformAIController.h"
#include "Subsystems/GamePlatformQuestServerSubsystem.h"
#endif
namespace DivineBeasts::Demo {
FGamePlatformResult FGameplayManualState::Server(FName O,const FManualArguments& A){
 if(O==TEXT("ReadQuests")||O==TEXT("TrackQuest")||O==TEXT("UntrackQuest")){
  auto* Q=Player.IsValid()?Player->GetSubsystem<UGamePlatformQuestClientSubsystem>():nullptr;if(!Q)return GameplayError(TEXT("Demo.Gameplay.QuestClient"),TEXT("真实LP Quest服务未创建。"));
  if(O==TEXT("ReadQuests")){Observe(FString::Printf(TEXT("真实权威Quest缓存=%d；空Hub无任务，不写合成快照。"),Q->GetSortedSnapshotsView().Num()));return FGamePlatformResult::Success();}
  FName Id=A.Id(TEXT("QuestId"));if(!Q->FindQuest(Id))return GameplayError(TEXT("Demo.Gameplay.QuestSnapshotMissing"),TEXT("没有该任务的真实权威快照；不能伪造任务成功。"));
  if(bOwnTrack&&TouchedQuest!=Id)return GameplayError(TEXT("Demo.Gameplay.QuestTrackBusy"),TEXT("先Reset归还上个任务追踪。"));if(!bOwnTrack){TouchedQuest=Id;bOriginalTracked=Q->IsTracked(Id);bOwnTrack=true;}
  return GameplayBool(O==TEXT("TrackQuest")?Q->TrackQuest(Id):Q->UntrackQuest(Id),TEXT("QuestClient tracking only; no progress write"));
 }
 if(O==TEXT("InspectQuestHost")||O==TEXT("InspectPersistenceHost")){
#if WITH_EDITOR
  auto* Q=World.IsValid()?World->GetSubsystem<UGamePlatformQuestServerSubsystem>():nullptr;Observe(FString::Printf(TEXT("真实QuestServer=%d；当前无本Run可信PlayerRuntime、生产Quest端点或可撤销Definition注册。"),Q!=nullptr));
#endif
  return GameplayError(TEXT("Demo.Gameplay.QuestAuthorityFixtureMissing"),TEXT("任务权威/持久化正常案例缺少隔离可信Port和可清理Definition宿主；不读真实账号、不造内存落库成功，不把Client追踪当进度验收。"));
 }
#if WITH_EDITOR
 if(O==TEXT("UnPossessAI")){if(!AIController.IsValid())return FGamePlatformResult::Success();auto* C=AIController.Get();C->UnPossess();if(!C->Destroy())return GameplayError(TEXT("Demo.Gameplay.AICleanup"),TEXT("AI控制器未销毁；保留对象。"));AIController.Reset();return FGamePlatformResult::Success();}
 if(O==TEXT("PossessAI")){
  if(!Source.IsValid()||!AI.IsValid()||AIController.IsValid())return GameplayError(TEXT("Demo.Gameplay.AIFixture"),TEXT("需已加载AIDefinition、自有真实Pawn且没有在途AI。"));
  const auto E=Epoch;Source->AIState->SetDefinitionAsset(AI.Get());if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("AI状态通知期间停止。"));Source->ASC->ClearActivationGate(Source.Get());
  FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;AIController=World->SpawnActor<AGamePlatformAIController>(AGamePlatformAIController::StaticClass(),FVector::ZeroVector,FRotator::ZeroRotator,P);if(!Current(E)||!AIController.IsValid())return GameplayError(TEXT("Demo.Gameplay.AISpawn"),TEXT("原生控制器出生失败/来源停止。"));AIController->Possess(Source.Get());return GameplayBool(AIController->GetPawn()==Source.Get(),TEXT("native Possess accepted; brain/data readiness observed separately"));
 }
 if(O==TEXT("ForceDecision")){if(!AIController.IsValid())return GameplayError(TEXT("Demo.Gameplay.AIHost"),TEXT("先PossessAI。"));AIController->ForceDecisionUpdate();Observe(FString::Printf(TEXT("原生AI最后错误=%d；决策调用不等于路径/攻击成功。"),int32(AIController->GetLastAIError())));return GameplayBool(AIController->GetLastAIError()==EGamePlatformAIError::None,TEXT("ForceDecisionUpdate"));}
 if(O==TEXT("TryAttack")){if(!AIController.IsValid())return GameplayError(TEXT("Demo.Gameplay.AIHost"),TEXT("先PossessAI。"));return GameplayBool(AIController->TryAttackCurrentTarget(),TEXT("native TryAttackCurrentTarget; base native-attack defaults false, no fabricated attack"));}
#else
 if(O==TEXT("UnPossessAI"))return FGamePlatformResult::Success();
#endif
 return GameplayError(TEXT("Demo.Gameplay.EditorServerHost"),TEXT("该操作需要Editor私有Server桥和真实Standalone Authority；普通Client不链接Server模块。"));
}
}
