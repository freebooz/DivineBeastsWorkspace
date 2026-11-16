// SSLM吸血鬼为人型体积与速度，权威死亡和伤害继续复用既有单一Combat链。
#include "Characters/DivineBeastsVillageVampire.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ADivineBeastsVillageVampire::ADivineBeastsVillageVampire()
{
    GetCapsuleComponent()->InitCapsuleSize(48.0f, 98.0f);
    // 人型怪物基础移动速度与玩家构造默认值一致；正式追击仍需核对HeroDefinition中的真实基础速度。
    GetCharacterMovement()->MaxWalkSpeed = 450.0f;
}
