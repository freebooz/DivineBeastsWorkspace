// SSLM吸血鬼为人型体积与速度，权威死亡和伤害继续复用既有单一Combat链。
#include "Characters/DivineBeastsVillageVampire.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ADivineBeastsVillageVampire::ADivineBeastsVillageVampire()
{
    GetCapsuleComponent()->InitCapsuleSize(48.0f, 98.0f);
    GetCharacterMovement()->MaxWalkSpeed = 320.0f;
}
