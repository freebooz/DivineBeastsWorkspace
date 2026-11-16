// 只借用经资格校验的引擎Editor Hub世界；不创建/销毁世界，不改MPC资产默认值。
#include "Scenarios/DivineBeastsDemoPresentationManualState.h"
#include "Engine/World.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#if WITH_EDITOR
#include "API/GamePlatformSurfaceWindPreview.h"
#include "Editor.h"
#endif
namespace DivineBeasts::Demo {
FGamePlatformResult FPresentationManualState::CloseEditorWind(){
#if WITH_EDITOR
 if(bEditorWindOperation)return PresentationError(TEXT("Demo.Surface.EditorWindInFlight"),TEXT("Editor风原生调用尚未返回，保留原世界/状态/租约供Stop重试。"));
 if(bEditorWindCaptured){FString Error;FGamePlatformSurfaceWindTransition Current;
  if(!EditorWindWorld.IsValid()||!GamePlatformSurfaceWindPreview::Read(*EditorWindWorld,Current,Error))return PresentationError(TEXT("Demo.Surface.EditorWindRestore"),*Error);
  if(!Current.IsNearlyEqual(ExpectedEditorWind))return PresentationError(TEXT("Demo.Surface.EditorWindConflict"),TEXT("Editor风已被后继工具改变；保留状态供显式重试，不覆盖外部修改。"));
  if(!GamePlatformSurfaceWindPreview::Apply(*EditorWindWorld,OriginalEditorWind,Error))return PresentationError(TEXT("Demo.Surface.EditorWindRestore"),*Error);
  bEditorWindCaptured=false;
 }EditorWindWorld.Reset();
#endif
 return FGamePlatformResult::Success();
}
FGamePlatformResult FPresentationManualState::ExecuteEditorWind(FName Op,const FPresentationArgs& A){
#if WITH_EDITOR
 if(Op==TEXT("Reset"))return CloseEditorWind();
 if(!Host.IsValid()||!Host->OwnsEditorFixture(Context.Token)||!GEditor)return PresentationError(TEXT("Demo.Surface.EditorLeaseRequired"),TEXT("需要真实Editor PIE和排他Editor租约。"));
 if(bEditorWindOperation)return PresentationError(TEXT("Demo.Surface.EditorWindReentry"),TEXT("拒绝同步重入修改风。"));
 TGuardValue<bool> Calling(bEditorWindOperation,true);
 const auto E=Epoch;
 if(Op==TEXT("CreateEditorWorld")){
  if(EditorWindWorld.IsValid())return PresentationError(TEXT("Demo.Surface.EditorWorldOwned"),TEXT("已有本Run借用；先Reset。"));
  auto* Candidate=GEditor->GetEditorWorldContext().World();
  if(!Candidate||Candidate->WorldType!=EWorldType::Editor||Candidate->GetOutermost()->GetName()!=FDivineBeastsPluginDemoMode::GetHubMapPackage())return PresentationError(TEXT("Demo.Surface.EditorHubRequired"),TEXT("要求真实Editor中已打开开发Hub，拒绝任意用户地图。"));
  FString Error;if(!GamePlatformSurfaceWindPreview::Read(*Candidate,OriginalEditorWind,Error))return PresentationError(TEXT("Demo.Surface.EditorWindRead"),*Error);
  if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("Editor风捕获期间已停止。"));
  EditorWindWorld=Candidate;ExpectedEditorWind=OriginalEditorWind;bEditorWindCaptured=true;return FGamePlatformResult::Success();
 }
 if(!EditorWindWorld.IsValid()||!bEditorWindCaptured)return PresentationError(TEXT("Demo.Surface.EditorWorldMissing"),TEXT("先借用本运行实际Editor Hub风上下文。"));
 FString Error;FGamePlatformSurfaceWindTransition Wind;
 if(!GamePlatformSurfaceWindPreview::Read(*EditorWindWorld,Wind,Error))return PresentationError(TEXT("Demo.Surface.EditorWindRead"),*Error);
 if(Op==TEXT("ApplyWind")){
  if(!Wind.IsNearlyEqual(ExpectedEditorWind))return PresentationError(TEXT("Demo.Surface.EditorWindConflict"),TEXT("外部后继风已改变，拒绝覆盖。"));
  Wind=OriginalEditorWind;Wind.From=FVector4(A.Number(TEXT("WindFromYaw")),A.Number(TEXT("WindFromStrength")),A.Number(TEXT("WindFromGust")),A.Number(TEXT("WindFromHz")));Wind.To=FVector4(A.Number(TEXT("WindToYaw")),A.Number(TEXT("WindToStrength")),A.Number(TEXT("WindToGust")),A.Number(TEXT("WindToHz")));Wind.Timing=FVector4(0,A.Number(TEXT("WindSeconds"),2),0,0);Wind.Seed=A.Int(TEXT("Seed"),2026);
  ExpectedEditorWind=Wind;
  if(!GamePlatformSurfaceWindPreview::Apply(*EditorWindWorld,Wind,Error))return PresentationError(TEXT("Demo.Surface.EditorWindApply"),*Error);
  ExpectedEditorWind=Wind;
 }
 if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("Editor风操作期间已停止。"));
 Observe(FString::Printf(TEXT("真实Editor Hub MPC风 FromYaw=%.2f ToYaw=%.2f Strength=%.3f Seed=%d；实际视口/植被仍需人工观察。"),Wind.From.X,Wind.To.X,Wind.To.Y,Wind.Seed));
 return Current(E)?FGamePlatformResult::Success():FGamePlatformResult::Cancelled(TEXT("观察通知期间已停止。"));
#else
 return FGamePlatformResult::Unsupported(TEXT("Demo.Surface.EditorHostRequired"),TEXT("风预览要求Editor目标和真实PIE。"));
#endif
}
}
