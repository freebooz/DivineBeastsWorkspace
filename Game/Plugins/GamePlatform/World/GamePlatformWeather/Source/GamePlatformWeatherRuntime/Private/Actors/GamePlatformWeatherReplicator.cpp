// 动态天气网络Actor只承担事实投影，不承担天气选择、玩家请求或客户端特效。
#include "Actors/GamePlatformWeatherReplicator.h"
#include "Subsystems/GamePlatformWeatherWorldSubsystem.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AGamePlatformWeatherReplicator::AGamePlatformWeatherReplicator()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true; // 一个世界只有一个很小的状态；不按玩家位置重复生成天气对象。
    SetReplicateMovement(false); // UE5.8通过公开API控制移动复制，不访问AActor私有字段。
    NetUpdateFrequency = 2.f;
    MinNetUpdateFrequency = 0.1f;
    SetReplicates(true);
}

void AGamePlatformWeatherReplicator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGamePlatformWeatherReplicator, Snapshot);
}

void AGamePlatformWeatherReplicator::BeginPlay()
{
    Super::BeginPlay();
    NotifyWorldService(); // 覆盖客户端先OnRep、后BeginPlay和反序执行情况。
}

void AGamePlatformWeatherReplicator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
        if (UGamePlatformWeatherWorldSubsystem* Service = World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>())
            Service->ForgetReplicator(this);
    Super::EndPlay(EndPlayReason);
}

void AGamePlatformWeatherReplicator::OnRep_WeatherSnapshot()
{
    NotifyWorldService();
}

bool AGamePlatformWeatherReplicator::PublishAuthoritativeSnapshot(const FGamePlatformWeatherSnapshot& InSnapshot)
{
    if (!HasAuthority() || InSnapshot.Revision <= Snapshot.Revision) return false;
    Snapshot = InSnapshot;
    NotifyWorldService();
    ForceNetUpdate();
    return true;
}

void AGamePlatformWeatherReplicator::NotifyWorldService() const
{
    if (Snapshot.Revision <= 0) return;
    if (UWorld* World = GetWorld())
        if (UGamePlatformWeatherWorldSubsystem* Service = World->GetSubsystem<UGamePlatformWeatherWorldSubsystem>())
            Service->ObserveReplicator(this, Snapshot);
}
