// 仅操作本适配创建的隔离Editor World；不读取GEditor当前地图，不修改资产默认值。
#include "Scenarios/DivineBeastsDemoPresentationManualState.h"
#include "Engine/World.h"
#if WITH_EDITOR
#include "API/GamePlatformSurfaceWindPreview.h"
#endif
namespace DivineBeasts::Demo
{
FGamePlatformResult FPresentationManualState::CloseEditorWind()
{
#if WITH_EDITOR
    if(EditorWindWorld.IsValid())
    {
        if(bEditorWindCaptured){FString Error;if(!GamePlatformSurfaceWindPreview::Apply(*EditorWindWorld,OriginalEditorWind,Error))return PresentationError(TEXT("Demo.Surface.EditorWindRestore"),*Error);bEditorWindCaptured=false;}
        EditorWindWorld->DestroyWorld(false);EditorWindWorld.Reset();
    }
#endif
    return FGamePlatformResult::Success();
}
FGamePlatformResult FPresentationManualState::ExecuteEditorWind(FName Op,const FPresentationArgs& A)
{
#if WITH_EDITOR
    if(Op==TEXT("Reset"))return CloseEditorWind();
    if(Op==TEXT("CreateEditorWorld"))
    {
        if(EditorWindWorld.IsValid())return PresentationError(TEXT("Demo.Surface.EditorWorldOwned"),TEXT("已有本运行Editor世界；先Reset。"));
        // UE5.8 CreateWorld内部已经InitializeNewWorld，禁止第二次初始化或默认AddToRoot造成泄漏。
        const auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
        EditorWindWorld.Reset(UWorld::CreateWorld(EWorldType::Editor,false,MakeUniqueObjectName(GetTransientPackage(),UWorld::StaticClass(),TEXT("DBAManualSurfaceWind")),GetTransientPackage(),false,ERHIFeatureLevel::Num,&Values));
        if(!EditorWindWorld.IsValid())return PresentationError(TEXT("Demo.Surface.EditorWorldCreate"),TEXT("隔离Editor世界创建失败。"));
        FString Error;bEditorWindCaptured=GamePlatformSurfaceWindPreview::Read(*EditorWindWorld,OriginalEditorWind,Error);
        return bEditorWindCaptured?FGamePlatformResult::Success():PresentationError(TEXT("Demo.Surface.EditorWindRead"),*Error);
    }
    if(!EditorWindWorld.IsValid()||!bEditorWindCaptured)return PresentationError(TEXT("Demo.Surface.EditorWorldMissing"),TEXT("先创建本运行隔离Editor世界并读取合法MPC。"));
    FGamePlatformSurfaceWindTransition Wind;FString Error;
    if(Op==TEXT("ApplyWind"))
    {Wind=OriginalEditorWind;Wind.From=FVector4(A.Number(TEXT("WindFromYaw")),A.Number(TEXT("WindFromStrength")),A.Number(TEXT("WindFromGust")),A.Number(TEXT("WindFromHz")));Wind.To=FVector4(A.Number(TEXT("WindToYaw")),A.Number(TEXT("WindToStrength")),A.Number(TEXT("WindToGust")),A.Number(TEXT("WindToHz")));Wind.Timing=FVector4(0,A.Number(TEXT("WindSeconds"),2),0,0);Wind.Seed=A.Int(TEXT("Seed"),2026);
        if(!GamePlatformSurfaceWindPreview::Apply(*EditorWindWorld,Wind,Error))return PresentationError(TEXT("Demo.Surface.EditorWindApply"),*Error);}
    if(!GamePlatformSurfaceWindPreview::Read(*EditorWindWorld,Wind,Error))return PresentationError(TEXT("Demo.Surface.EditorWindRead"),*Error);
    Observe(FString::Printf(TEXT("隔离Editor World MPC风：FromYaw=%.2f ToYaw=%.2f Strength=%.3f Seed=%d；雨雪八项未改。当前无可见Editor视口，植被画面仍未验收。"),Wind.From.X,Wind.To.X,Wind.To.Y,Wind.Seed));
    return FGamePlatformResult::Success();
#else
    return FGamePlatformResult::Unsupported(TEXT("Demo.Surface.EditorHostRequired"),TEXT("普通Client不链接SurfaceEditor，风预览需要Editor目标和隔离Editor可视宿主。"));
#endif
}
}
