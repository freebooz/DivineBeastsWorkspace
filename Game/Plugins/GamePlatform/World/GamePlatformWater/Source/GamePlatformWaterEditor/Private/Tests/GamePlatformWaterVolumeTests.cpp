// 真实引擎Brush与CharacterMovement回归：凹岸缺口不能游泳，离开水域必须退出泳态。
#include "Misc/AutomationTest.h"
#include "GamePlatformWaterEditorLibrary.h"
#include "Actors/GamePlatformWaterRegion.h"
#include "Subsystems/GamePlatformWaterWorldSubsystem.h"
#include "WaterBodyLakeActor.h"
#include "WaterBodyComponent.h"
#include "WaterSplineComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformWaterVolumeTests,
    "GamePlatform.Water.Editor.ConcavePhysicsVolume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformWaterVolumeTests::RunTest(const FString&)
{
    // 避免WaterBody的Editor构造通过GEditor接触用户当前地图；只有烘焙调用同步栈标记Editor。
    UWorld* W = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("独立测试世界"), W)) return false;
    FActorSpawnParameters Spawn;
    Spawn.bDeferConstruction = true;
    AWaterBodyLake* Lake = W->SpawnActor<AWaterBodyLake>(AWaterBodyLake::StaticClass(), FTransform::Identity, Spawn);
    Lake->GetWaterBodyComponent()->bAffectsLandscape = false;
    Lake->FinishSpawning(FTransform::Identity);
    UWaterSplineComponent* Spline = Lake->GetWaterBodyComponent()->GetWaterSpline();
    Spline->ClearSplinePoints(false);
    for (const FVector P : {FVector(0,0,0), FVector(400,0,0), FVector(400,100,0),
                           FVector(100,100,0), FVector(100,400,0), FVector(0,400,0)})
        Spline->AddSplinePoint(P, ESplineCoordinateSpace::World, false);
    for (int32 I = 0; I < 6; ++I) Spline->SetSplinePointType(I, ESplinePointType::Linear, false);
    Spline->SetClosedLoop(true, true);
    AGamePlatformWaterRegion* Region = W->SpawnActor<AGamePlatformWaterRegion>();
    Region->Region.RegionId = TEXT("Test.ConcaveLake");
    FString Error;
    W->WorldType = EWorldType::Editor;
    APhysicsVolume* Volume = UGamePlatformWaterEditorLibrary::BakeLakePhysicsVolume(Lake, Region, 200.f, Error);
    W->WorldType = EWorldType::Game;
    if (TestNotNull(*Error, Volume))
    {
        FGamePlatformWaterSample Sample;
        TestTrue(TEXT("凹湖内部精确采样"), Region->SampleRegion(FVector(50,50,-50), Sample));
        TestFalse(TEXT("包围盒内凹岸干地不得浸水"), Region->SampleRegion(FVector(200,200,-50), Sample));
        TestFalse(TEXT("水面上方保持干态"), Region->SampleRegion(FVector(50,50,1), Sample));
        TestFalse(TEXT("体积底部以外不属于水域"), Region->SampleRegion(FVector(50,50,-201), Sample));
        TestFalse(TEXT("非有限查询先失败，不进入原生物理查询"), Region->SampleRegion(FVector(std::numeric_limits<double>::quiet_NaN(),0,0), Sample));
        TestTrue(TEXT("水物理体积与区域共用边界"), Volume->EncompassesPoint(FVector(50,50,-50)));
        TestFalse(TEXT("物理体积不能覆盖凹岸缺口"), Volume->EncompassesPoint(FVector(200,200,-50)));
        ACharacter* Character = W->SpawnActor<ACharacter>();
        auto* Movement = Character->GetCharacterMovement();
        Character->SetActorLocation(FVector(-1000,-1000,1000));
        Movement->SetUpdatedComponent(Character->GetCapsuleComponent());
        Movement->GetNavAgentPropertiesRef().bCanSwim = true;
        Character->SetActorLocation(FVector(50,50,-50));
        TestFalse(TEXT("未发布水域规则时体积不能自行授予游泳"), Volume->IsOverlapInVolume(*Character->GetCapsuleComponent()));
        auto* Water = W->GetSubsystem<UGamePlatformWaterWorldSubsystem>();
        TestTrue(TEXT("发布本世界唯一水域索引"), Water->RegisterRegion(Region));
        W->AddPhysicsVolume(Volume);
        Character->GetCapsuleComponent()->UpdatePhysicsVolume(true);
        TestTrue(TEXT("原生物理水域通知进入Swimming"), Movement->IsSwimming());
        auto* NoSwim = W->SpawnActor<AGamePlatformWaterRegion>();
        NoSwim->Region.RegionId = TEXT("Test.NoSwim");
        NoSwim->Region.Priority = 100;
        NoSwim->Region.bSwimmable = false;
        TestTrue(TEXT("高优先级禁止游泳规则发布"), Water->RegisterRegion(NoSwim));
        Character->GetCapsuleComponent()->UpdatePhysicsVolume(true);
        TestFalse(TEXT("原生运动必须服从同一水域索引的禁止游泳规则"), Movement->IsSwimming());
        Water->UnregisterRegion(NoSwim);
        Character->GetCapsuleComponent()->UpdatePhysicsVolume(true);
        TestTrue(TEXT("撤销覆盖后原生游泳恢复"), Movement->IsSwimming());
        Region->WaterPhysicsVolume = nullptr;
        Character->GetCapsuleComponent()->UpdatePhysicsVolume(true);
        TestFalse(TEXT("区域失效时孤立体积不能继续授予游泳"), Movement->IsSwimming());
        TestFalse(TEXT("要求精确水域时缺体积必须失败"), Region->IsWaterRegionValid());
        TestFalse(TEXT("缺体积不能退回大Box"), Region->SampleRegion(FVector(0,0,-50), Sample));
        TestFalse(TEXT("失败必须清空旧采样"), Sample.bInWater);
        TestEqual(TEXT("湖岸原控制点保持不变"), Spline->GetNumberOfSplinePoints(), 6);
    }
    W->DestroyWorld(false);
    return true;
}
#endif
