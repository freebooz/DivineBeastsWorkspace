// AI目标实体身份与服务器资格：游戏线程仅Authority Owner修改，随组件生命周期提供中立目标事实。
#include "Components/GamePlatformAITargetComponent.h"

// HasAuthority直接调用AActor成员；NoPCH/独立编译不能依靠别的源文件间接包含完整类型。
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UGamePlatformAITargetComponent::UGamePlatformAITargetComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UGamePlatformAITargetComponent::BeginPlay()
{
    Super::BeginPlay();

    if (GetOwner() && GetOwner()->HasAuthority() &&
        !TargetEntityId.IsValid())
    {
        TargetEntityId = FGuid::NewGuid();
        TargetGeneration = FMath::Max(1, TargetGeneration);
    }
}

void UGamePlatformAITargetComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UGamePlatformAITargetComponent, TargetEntityId);
    DOREPLIFETIME(UGamePlatformAITargetComponent, TargetGeneration);
    DOREPLIFETIME(UGamePlatformAITargetComponent, bAITargetEnabled);
}

void UGamePlatformAITargetComponent::SetAITargetEnabled(bool bEnabled)
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        bAITargetEnabled = bEnabled;
    }
}

void UGamePlatformAITargetComponent::AdvanceTargetGeneration()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        ++TargetGeneration;
    }
}
