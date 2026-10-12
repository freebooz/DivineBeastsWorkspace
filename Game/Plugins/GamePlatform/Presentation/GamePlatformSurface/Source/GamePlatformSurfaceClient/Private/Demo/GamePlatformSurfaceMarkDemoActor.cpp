// 平台独立演示：用真实射线撞击测试网格，调用与生产同一个SurfaceMark入口。
#include "Demo/GamePlatformSurfaceMarkDemoActor.h"
#include "Marks/GamePlatformSurfaceMarkBlueprintLibrary.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGamePlatformSurfaceMarkDemo, Log, All);

AGamePlatformSurfaceMarkDemoActor::AGamePlatformSurfaceMarkDemoActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ReviewOrigin")));
}

void AGamePlatformSurfaceMarkDemoActor::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoEmitOnBeginPlay && GetNetMode() != NM_DedicatedServer && IsValid(GetWorld()))
    {
        // 延后一秒：让当前世界的GameInstance/Data服务和世界网格完成BeginPlay。
        GetWorld()->GetTimerManager().SetTimer(DemoTimer, this, &ThisClass::EmitFiveMarks, 1.0f, false);
    }
}

void AGamePlatformSurfaceMarkDemoActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(DemoTimer);
    Super::EndPlay(Reason);
}

void AGamePlatformSurfaceMarkDemoActor::EmitFiveMarks()
{
    UWorld* World = GetWorld();
    if (!IsValid(World) || World->GetNetMode() == NM_DedicatedServer) return;
    const EGamePlatformSurfaceMarkKind Kinds[5] = {
        EGamePlatformSurfaceMarkKind::Bullet,
        EGamePlatformSurfaceMarkKind::Scorch,
        EGamePlatformSurfaceMarkKind::Slash,
        EGamePlatformSurfaceMarkKind::Fluid,
        EGamePlatformSurfaceMarkKind::Crack
    };
    const FVector Origin = GetActorLocation();
    for (int32 Index = 0; Index < 5; ++Index)
    {
        const float X = (Index - 2) * PanelSpacingCm;
        // 从测试板朝向前方的观察侧射入；正常的墙面命中法线为-Y。
        const FVector Start = Origin + FVector(X, -250.0f, 170.0f);
        const FVector End = Origin + FVector(X, 600.0f, 170.0f);
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(SurfaceMarkDemo), true, this);
        Query.bReturnPhysicalMaterial = true;
        if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Query))
        {
            UE_LOG(LogGamePlatformSurfaceMarkDemo, Warning, TEXT("第%d类痕迹没有命中测试板，等待人工检查场景。"), Index);
            continue;
        }
        const auto Result = UGamePlatformSurfaceMarkBlueprintLibrary::SubmitSurfaceMarkFromHit(
            World, Kinds[Index], Hit, FVector(1, 0, 0), FGuid::NewGuid(), 1.0f);
        UE_LOG(LogGamePlatformSurfaceMarkDemo, Display, TEXT("痕迹演示%d：状态=%d，Active=%d Pending=%d"),
            Index, int32(Result.Status), Result.ActiveCount, Result.PendingCount);
    }
}
