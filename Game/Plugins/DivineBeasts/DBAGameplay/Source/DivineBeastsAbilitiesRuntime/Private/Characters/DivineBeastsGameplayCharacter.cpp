// 项目双端技能角色装配：复用权威Pawn全生命周期，只拥有新增Loadout；不重复ASC/战斗/身份或放行失控角色。
#include "Characters/DivineBeastsGameplayCharacter.h"

#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Components/DivineBeastsAbilityLoadoutComponent.h"

ADivineBeastsGameplayCharacter::ADivineBeastsGameplayCharacter()
{
    bReplicates = true;
    // 基类已经拥有AbilitySystem/Combat/CharacterIdentity/GameplayEligibility；只保留本模块的稳定Loadout子对象名。
    AbilityLoadout = CreateDefaultSubobject<UDivineBeastsAbilityLoadoutComponent>(TEXT("AbilityLoadout"));
}

UAbilitySystemComponent* ADivineBeastsGameplayCharacter::GetAbilitySystemComponent() const
{
    return Super::GetAbilitySystemComponent();
}

void ADivineBeastsGameplayCharacter::BeginPlay()
{
    Super::BeginPlay();
    BindAbilityActorInfo();
}

void ADivineBeastsGameplayCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    BindAbilityActorInfo();
}

void ADivineBeastsGameplayCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    BindAbilityActorInfo();
}

void ADivineBeastsGameplayCharacter::BindAbilityActorInfo()
{
    if (UGamePlatformAbilitySystemComponent* AbilitySystem = GetGamePlatformAbilitySystemComponent())
    {
        // 公共复制事实查询保留主分支入口，但服务器失控后不能由PlayerState复制/启动重建Avatar。
        // 客户端Controller可能不向远端观察者复制，ActorInfo可存在，实际激活仍由继承的Gate失败关闭。
        if (GetController() || !HasAuthority()) { AbilitySystem->BindAbilityActorInfo(this, this); }
        else
        {
            // 与基类拥有关系刷新保持同一边界：无Controller的权威Pawn立即失活并撤销Avatar。
            if (auto* Eligibility = FindComponentByClass<UGamePlatformGameplayEligibilityComponent>())
            {
                Eligibility->SetServerPlayerActive(false);
            }
            AbilitySystem->ClearAbilityAvatar();
        }
    }
}
