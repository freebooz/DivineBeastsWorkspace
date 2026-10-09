#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "DivineBeastsGameplayCharacter.generated.h"

class UGamePlatformAbilitySystemComponent;
class UDivineBeastsCharacterComponent;
class UDivineBeastsAbilityLoadoutComponent;
class UGamePlatformCombatComponent;

/**
 * ADivineBeastsGameplayCharacter（神兽联盟双端可玩角色）。
 * 只组合一套平台 ASC、原有英雄身份组件和技能装配组件，保留 ACharacter 运动基础；
 * 不在基础平台或角色定义类写死子鼠、龙等具体英雄类。
 */
UCLASS()
class DIVINEBEASTSABILITIESRUNTIME_API ADivineBeastsGameplayCharacter
    : public ACharacter
    , public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ADivineBeastsGameplayCharacter();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;

private:
    /** ASC 具有 Owner/Avatar 生命周期；仅服务器给能力，客户端使用 GAS 原生复制。 */
    UPROPERTY(VisibleAnywhere, Category="DivineBeasts|Gameplay")
    TObjectPtr<UGamePlatformAbilitySystemComponent> AbilitySystem;

    UPROPERTY(VisibleAnywhere, Category="DivineBeasts|Gameplay")
    TObjectPtr<UDivineBeastsCharacterComponent> CharacterIdentity;

    UPROPERTY(VisibleAnywhere, Category="DivineBeasts|Gameplay")
    TObjectPtr<UDivineBeastsAbilityLoadoutComponent> AbilityLoadout;

    /** 单个角色统一权威战斗组件；项目技能复用平台原有伤害、护盾与命中审查。 */
    UPROPERTY(VisibleAnywhere, Category="DivineBeasts|Gameplay")
    TObjectPtr<UGamePlatformCombatComponent> Combat;

    void BindAbilityActorInfo();
};
