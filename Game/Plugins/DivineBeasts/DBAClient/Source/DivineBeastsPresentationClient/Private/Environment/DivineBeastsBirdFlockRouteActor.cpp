// 新手村及开放世界鸟群样条和批次表现组合；不含项目权威玩法，也不修改服务端地图。
#include "Environment/DivineBeastsBirdFlockRouteActor.h"

#include "Environment/DivineBeastsBirdFlockBatchComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"

ADivineBeastsBirdFlockRouteActor::ADivineBeastsBirdFlockRouteActor()
{
    bReplicates = false;
    SetReplicateMovement(false);
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BirdRouteRoot"));
    RootComponent = SceneRoot;
    FlightSpline = CreateDefaultSubobject<USplineComponent>(TEXT("BirdFlightSpline"));
    FlightSpline->SetupAttachment(SceneRoot);
    FlightSpline->SetMobility(EComponentMobility::Movable);
    FlockBatches = CreateDefaultSubobject<UDivineBeastsBirdFlockBatchComponent>(TEXT("FlockBatches"));
    // 默认路线绑定到自身Spline；场景可以无迁移地切回旧Orbit/Traverse/FigureEight。
    FlockBatches->Flight.bUseSpline = true;
    FlockBatches->Flight.FlightMode = EDivineBeastsBirdFlockFlightMode::SplineLoop;
    FlockBatches->BirdsPerGroup = 32;
    FlockBatches->MaxActiveGroups = 3;
    FlockBatches->MaxBatches = 4;
    // 本地玩家表现Service应在业务世界就绪事件后再调用Start，不能在构造/BeginPlay强行启动。
    FlockBatches->bAutoStart = false;
}

bool ADivineBeastsBirdFlockRouteActor::StartRouteBirds(FString& OutError)
{
    if (!FlockBatches || !FlightSpline)
    {
        OutError = TEXT("鸟群样条/批次组件缺失。");
        return false;
    }
    return FlockBatches->StartBatches(OutError);
}

void ADivineBeastsBirdFlockRouteActor::StopRouteBirds()
{
    if (FlockBatches) FlockBatches->StopBatches();
}
