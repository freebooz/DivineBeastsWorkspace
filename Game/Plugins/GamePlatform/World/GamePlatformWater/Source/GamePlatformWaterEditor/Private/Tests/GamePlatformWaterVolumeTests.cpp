// 真实引擎Brush与CharacterMovement回归：凹岸缺口不能游泳，离开水域必须退出泳态。
#include "Misc/AutomationTest.h"
#include "GamePlatformWaterEditorLibrary.h"
#include "Actors/GamePlatformWaterRegion.h"
#include "Actors/GamePlatformWaterPhysicsVolume.h"
#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"
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
        // 资格单元回归发送真实引擎通知；完整物理场景的Broadphase由独立运行验证，
        // 不把此未开始Play的瞬态World当作已初始化的Chaos场景。
        const auto UpdateQualifiedVolume = [&]
        {
            Character->GetCapsuleComponent()->SetPhysicsVolume(
                Volume->IsOverlapInVolume(*Character->GetCapsuleComponent()) ? Volume : nullptr, true);
        };
        auto* Water = W->GetSubsystem<UGamePlatformWaterWorldSubsystem>();
        TestTrue(TEXT("发布本世界唯一水域索引"), Water->RegisterRegion(Region));
        W->AddPhysicsVolume(Volume);
        UpdateQualifiedVolume();
        TestTrue(TEXT("原生物理水域通知进入Swimming"), Movement->IsSwimming());
        auto* NoSwim = W->SpawnActor<AGamePlatformWaterRegion>();
        NoSwim->Region.RegionId = TEXT("Test.NoSwim");
        NoSwim->Region.Priority = 100;
        NoSwim->Region.bSwimmable = false;
        TestTrue(TEXT("高优先级禁止游泳规则发布"), Water->RegisterRegion(NoSwim));
        TestFalse(TEXT("高优先级禁泳覆盖拒绝物理体积资格"), Volume->IsOverlapInVolume(*Character->GetCapsuleComponent()));
        UpdateQualifiedVolume();
        TestFalse(TEXT("原生运动必须服从同一水域索引的禁止游泳规则"), Movement->IsSwimming());
        Water->UnregisterRegion(NoSwim);
        UpdateQualifiedVolume();
        TestTrue(TEXT("撤销覆盖后原生游泳恢复"), Movement->IsSwimming());
        auto* Duplicate = W->SpawnActor<AGamePlatformWaterRegion>();
        Duplicate->Region = Region->Region;
        W->WorldType = EWorldType::Editor;
        auto* DuplicateVolume = UGamePlatformWaterEditorLibrary::BakeLakePhysicsVolume(Lake, Duplicate, 200.f, Error);
        W->WorldType = EWorldType::Game;
        if (TestNotNull(TEXT("同合同体积允许重叠烘焙"), DuplicateVolume))
        {
            AddExpectedError(TEXT("重复水域ID"), EAutomationExpectedErrorFlags::Contains, 1);
            TestFalse(TEXT("不同Actor的重复ID必须拒绝注册"), Water->RegisterRegion(Duplicate));
            TestFalse(TEXT("被拒绝注册的同ID体积不能借另一Actor授予资格"), DuplicateVolume->IsOverlapInVolume(*Character->GetCapsuleComponent()));
            Water->UnregisterRegion(Region);
            TestFalse(TEXT("撤销注册立即撤销物理泳态资格"), Volume->IsOverlapInVolume(*Character->GetCapsuleComponent()));
            TestTrue(TEXT("原区域重新注册"), Water->RegisterRegion(Region));
            DuplicateVolume->Destroy();
        }
        Region->WaterPhysicsVolume = nullptr;
        UpdateQualifiedVolume();
        TestFalse(TEXT("区域失效时孤立体积不能继续授予游泳"), Movement->IsSwimming());
        TestFalse(TEXT("要求精确水域时缺体积必须失败"), Region->IsWaterRegionValid());
        TestFalse(TEXT("缺体积不能退回大Box"), Region->SampleRegion(FVector(0,0,-50), Sample));
        TestFalse(TEXT("失败必须清空旧采样"), Sample.bInWater);
        TestEqual(TEXT("湖岸原控制点保持不变"), Spline->GetNumberOfSplinePoints(), 6);
    }
    if (Volume) Volume->Destroy();
    // 未受索引管理的原生体积能绕过资格；必须在创建任何Actor/绑定前拒绝。
    auto* Conflict = W->SpawnActor<APhysicsVolume>();
    auto* Cube = NewObject<UCubeBuilder>(); Cube->X=200; Cube->Y=200; Cube->Z=200;
    W->WorldType = EWorldType::Editor;
    UActorFactory::CreateBrushForVolumeActor(Conflict, Cube);
    Conflict->SetActorLocation(FVector(50,50,-50)); Conflict->bWaterVolume=true; Conflict->Priority=100;
    auto* Target = W->SpawnActor<AGamePlatformWaterRegion>(); Target->Region.RegionId=TEXT("Test.Conflict");
    auto* Rejected = UGamePlatformWaterEditorLibrary::BakeLakePhysicsVolume(Lake, Target, 200.f, Error);
    TestNull(TEXT("未知原生物理水域重叠时拒绝作者"), Rejected);
    TestNull(TEXT("拒绝后目标没有新增绑定"), Target->WaterPhysicsVolume.Get());
    if (Rejected) { Target->WaterPhysicsVolume=nullptr; Rejected->Destroy(); }
    Conflict->SetActorLocation(FVector(10000,10000,-50));
    TestNotNull(TEXT("不重叠的普通体积不影响烘焙"), UGamePlatformWaterEditorLibrary::BakeLakePhysicsVolume(Lake, Target, 200.f, Error));
    W->WorldType = EWorldType::Game;
    W->DestroyWorld(false);
    return true;
}
#endif
