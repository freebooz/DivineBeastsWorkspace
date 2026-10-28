// 低频水域查询和离散网络同步：Owner状态的玩法计算只由Dedicated Server/Listen Server决定。
// 角色视觉表现由项目客户端订阅OnWaterStateChanged再调用GamePlatformWaterClient，禁止反向依赖。
#include "Components/GamePlatformWaterInteractorComponent.h"

#include "Subsystems/GamePlatformWaterWorldSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UGamePlatformWaterInteractorComponent::UGamePlatformWaterInteractorComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UGamePlatformWaterInteractorComponent::BeginPlay()
{
    Super::BeginPlay();
    const AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!IsValid(Owner) || !World || !Owner->HasAuthority())
        return;
    QueryIntervalSeconds = FMath::Clamp(QueryIntervalSeconds, 0.1f, 2.f);
    FootSampleOffsetCm = FMath::Clamp(FootSampleOffsetCm, -300.f, 300.f);
    PollAuthorityWater(); // 首次立即完成，避免角色刚出生时错误认为不在水中。
    // 首次真实状态通知可以同步停止/销毁本组件；EndPlay后旧BeginPlay栈不能重新挂计时器。
    if(!IsRegistered()||!IsValid(GetOwner())||GetOwner()!=Owner||World->bIsTearingDown)return;
    World->GetTimerManager().SetTimer(QueryTimer, this,
        &UGamePlatformWaterInteractorComponent::PollAuthorityWater,
        QueryIntervalSeconds, true);
}

void UGamePlatformWaterInteractorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(QueryTimer);
    Super::EndPlay(EndPlayReason);
}

void UGamePlatformWaterInteractorComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    // 只复制离散的浅水/深水变化，完全不复制每帧水纹、Niagara或GPU参数。
    DOREPLIFETIME(UGamePlatformWaterInteractorComponent, WaterState);
}

void UGamePlatformWaterInteractorComponent::OnRep_WaterState()
{
    OnWaterStateChanged.Broadcast(WaterState);
    OnWaterStateChangedNative.Broadcast(GetOwner(), WaterState);
}

void UGamePlatformWaterInteractorComponent::ResetAuthorityWater()
{
    const FGamePlatformWaterPresence Clear;
    if (!WaterState.HasSameState(Clear))
    {
        WaterState = Clear;
        OnWaterStateChanged.Broadcast(WaterState);
        if(!IsRegistered()||!IsValid(GetOwner())||!GetWorld()||GetWorld()->bIsTearingDown)return;
        OnWaterStateChangedNative.Broadcast(GetOwner(), WaterState);
        if (AActor* Owner = GetOwner(); IsRegistered()&&IsValid(Owner))
            if(IsRegistered()&&IsValid(Owner)&&GetOwner()==Owner)Owner->ForceNetUpdate();
    }
}

void UGamePlatformWaterInteractorComponent::PollAuthorityWater()
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!IsValid(Owner) || !World || !Owner->HasAuthority())
        return;
    const UGamePlatformWaterWorldSubsystem* Water =
        World->GetSubsystem<UGamePlatformWaterWorldSubsystem>();
    if (!Water)
    {
        ResetAuthorityWater();
        return;
    }
    FGamePlatformWaterSample Sample;
    const FVector FootPosition = Owner->GetActorLocation() +
        FVector(0., 0., static_cast<double>(FootSampleOffsetCm));
    FGamePlatformWaterPresence Next;
    if (Water->QueryWaterAtLocation(FootPosition, Sample))
    {
        Next.bInWater = true;
        Next.bDeepWater = !Sample.bShallow;
        Next.bCanSwim = Sample.bSwimmable;
        Next.RegionId = Sample.RegionId;
    }
    if (!WaterState.HasSameState(Next))
    {
        WaterState = Next;
        OnWaterStateChanged.Broadcast(WaterState);
        if(!IsRegistered()||!IsValid(GetOwner())||!GetWorld()||GetWorld()->bIsTearingDown)return;
        OnWaterStateChangedNative.Broadcast(Owner, WaterState);
        Owner->ForceNetUpdate();
    }
}
