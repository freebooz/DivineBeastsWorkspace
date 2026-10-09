// 项目双端技能角色装配：复用权威Pawn全生命周期，拥有Loadout及主线镜头/输入；不重复ASC/战斗/身份或放行失控角色。
#include "Characters/DivineBeastsGameplayCharacter.h"

#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InputComponent.h"

ADivineBeastsGameplayCharacter::ADivineBeastsGameplayCharacter()
{
    bReplicates = true;
    // 保留主线镜头的默认子对象名、相对布局和运动默认值；可信Definition就绪后仍由继承身份组件应用配置。
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent()); CameraBoom->TargetArmLength = 380.f;
    CameraBoom->SocketOffset = FVector(0, 0, 80); CameraBoom->bUsePawnControlRotation = true;
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom); FollowCamera->bUsePawnControlRotation = false;
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->MaxWalkSpeed = 450.f; GetCharacterMovement()->JumpZVelocity = 500.f;
    // 基类已拥有唯一ASC/Combat/CharacterIdentity/GameplayEligibility；不能重建主线旧HeroIdentity或遮蔽继承成员。
    AbilityLoadout = CreateDefaultSubobject<UDivineBeastsAbilityLoadoutComponent>(TEXT("AbilityLoadout"));
}
void ADivineBeastsGameplayCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    // 原生输入只请求ACharacter运动，不提交坐标RPC；服务器仍校验运动与准入资格。
    Input->BindAxis(TEXT("DBA.MoveForward"), this, &ThisClass::MoveForward);
    Input->BindAxis(TEXT("DBA.MoveRight"), this, &ThisClass::MoveRight);
    Input->BindAxis(TEXT("DBA.Turn"), this, &ThisClass::TurnCamera);
    Input->BindAxis(TEXT("DBA.Look"), this, &ThisClass::LookCamera);
    Input->BindAction(TEXT("DBA.Jump"), IE_Pressed, this, &ACharacter::Jump);
    Input->BindAction(TEXT("DBA.Jump"), IE_Released, this, &ACharacter::StopJumping);
}
void ADivineBeastsGameplayCharacter::MoveForward(float Value)
{
    if (Controller && !Controller->IsMoveInputIgnored())
    { AddMovementInput(FRotationMatrix(FRotator(0, Controller->GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::X), Value); }
}
void ADivineBeastsGameplayCharacter::MoveRight(float Value)
{
    if (Controller && !Controller->IsMoveInputIgnored())
    { AddMovementInput(FRotationMatrix(FRotator(0, Controller->GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::Y), Value); }
}
void ADivineBeastsGameplayCharacter::TurnCamera(float Value) { AddControllerYawInput(Value); }
void ADivineBeastsGameplayCharacter::LookCamera(float Value) { AddControllerPitchInput(Value); }

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
    if (UGamePlatformAbilitySystemComponent* CharacterAbilitySystem = GetGamePlatformAbilitySystemComponent())
    {
        // 公共复制事实查询保留主分支入口，但服务器失控后不能由PlayerState复制/启动重建Avatar。
        // 客户端Controller可能不向远端观察者复制，ActorInfo可存在，实际激活仍由继承的Gate失败关闭。
        if (GetController() || !HasAuthority()) { CharacterAbilitySystem->BindAbilityActorInfo(this, this); }
        else
        {
            // 与基类拥有关系刷新保持同一边界：无Controller的权威Pawn立即失活并撤销Avatar。
            if (auto* Eligibility = FindComponentByClass<UGamePlatformGameplayEligibilityComponent>())
            {
                Eligibility->SetServerPlayerActive(false);
            }
            CharacterAbilitySystem->ClearAbilityAvatar();
        }
    }
}
