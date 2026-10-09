#include "Characters/DivineBeastsGameplayCharacter.h"

#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "Components/GamePlatformCombatComponent.h"

ADivineBeastsGameplayCharacter::ADivineBeastsGameplayCharacter()
{
    bReplicates = true;
    AbilitySystem = CreateDefaultSubobject<UGamePlatformAbilitySystemComponent>(TEXT("AbilitySystem"));
    CharacterIdentity = CreateDefaultSubobject<UDivineBeastsCharacterComponent>(TEXT("HeroIdentity"));
    AbilityLoadout = CreateDefaultSubobject<UDivineBeastsAbilityLoadoutComponent>(TEXT("AbilityLoadout"));
    Combat = CreateDefaultSubobject<UGamePlatformCombatComponent>(TEXT("Combat"));
    AbilitySystem->SetIsReplicated(true);
}

UAbilitySystemComponent* ADivineBeastsGameplayCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystem;
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
    if (AbilitySystem)
    {
        // 组件幂等检查 Owner/Avatar；角色替换会自动推进 ASC 的 AvatarGeneration。
        AbilitySystem->BindAbilityActorInfo(this, this);
    }
}
