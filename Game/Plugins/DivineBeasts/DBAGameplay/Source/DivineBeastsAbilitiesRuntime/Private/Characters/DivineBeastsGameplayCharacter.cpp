#include "Characters/DivineBeastsGameplayCharacter.h"

#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InputComponent.h"

ADivineBeastsGameplayCharacter::ADivineBeastsGameplayCharacter()
{
    bReplicates = true;
    CameraBoom=CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent()); CameraBoom->TargetArmLength=380.f;
    CameraBoom->SocketOffset=FVector(0,0,80); CameraBoom->bUsePawnControlRotation=true;
    FollowCamera=CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom); FollowCamera->bUsePawnControlRotation=false;
    bUseControllerRotationYaw=false;
    GetCharacterMovement()->bOrientRotationToMovement=true;
    GetCharacterMovement()->MaxWalkSpeed=450.f; GetCharacterMovement()->JumpZVelocity=500.f;
    AbilitySystem = CreateDefaultSubobject<UGamePlatformAbilitySystemComponent>(TEXT("AbilitySystem"));
    CharacterIdentity = CreateDefaultSubobject<UDivineBeastsCharacterComponent>(TEXT("HeroIdentity"));
    AbilityLoadout = CreateDefaultSubobject<UDivineBeastsAbilityLoadoutComponent>(TEXT("AbilityLoadout"));
    Combat = CreateDefaultSubobject<UGamePlatformCombatComponent>(TEXT("Combat"));
    AbilitySystem->SetIsReplicated(true);
}
void ADivineBeastsGameplayCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    // 原生输入只请求ACharacter运动，不提交坐标RPC；服务器仍校验运动与准入资格。
    Input->BindAxis(TEXT("DBA.MoveForward"),this,&ThisClass::MoveForward);
    Input->BindAxis(TEXT("DBA.MoveRight"),this,&ThisClass::MoveRight);
    Input->BindAxis(TEXT("DBA.Turn"),this,&ThisClass::TurnCamera);
    Input->BindAxis(TEXT("DBA.Look"),this,&ThisClass::LookCamera);
    Input->BindAction(TEXT("DBA.Jump"),IE_Pressed,this,&ACharacter::Jump);
    Input->BindAction(TEXT("DBA.Jump"),IE_Released,this,&ACharacter::StopJumping);
}
void ADivineBeastsGameplayCharacter::MoveForward(float V){if(Controller && !Controller->IsMoveInputIgnored()) AddMovementInput(FRotationMatrix(FRotator(0,Controller->GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::X),V);}
void ADivineBeastsGameplayCharacter::MoveRight(float V){if(Controller && !Controller->IsMoveInputIgnored()) AddMovementInput(FRotationMatrix(FRotator(0,Controller->GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::Y),V);}
void ADivineBeastsGameplayCharacter::TurnCamera(float V){AddControllerYawInput(V);}
void ADivineBeastsGameplayCharacter::LookCamera(float V){AddControllerPitchInput(V);}

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
