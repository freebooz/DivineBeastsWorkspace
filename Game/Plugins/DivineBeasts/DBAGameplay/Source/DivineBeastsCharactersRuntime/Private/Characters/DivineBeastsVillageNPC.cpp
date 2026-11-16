// SSLM新手村NPC的权威碰撞与复制；模型、材质和动画仅由客户端内容包提供。
#include "Characters/DivineBeastsVillageNPC.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ADivineBeastsVillageNPC::ADivineBeastsVillageNPC()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = false;
    AutoPossessAI = EAutoPossessAI::Disabled;
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
    GetCapsuleComponent()->InitCapsuleSize(42.0f, 90.0f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    GetCharacterMovement()->MaxWalkSpeed = 180.0f;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    bUseControllerRotationYaw = false;
}
