#pragma once

#include "CoreMinimal.h"
#include "Characters/DivineBeastsCharacter.h"
#include "DivineBeastsGameplayCharacter.generated.h"

class UDivineBeastsAbilityLoadoutComponent;

/**
 * ADivineBeastsGameplayCharacter（神兽联盟双端可玩角色）。
 * 继承项目权威Pawn的唯一ASC/Combat/CharacterIdentity/GameplayEligibility及死亡、失控和退出保护，
 * 这里只增加唯一技能Loadout；双端仍保留ACharacter运动基础，不在平台或角色定义写死生肖角色类。
 * 反射类/模块身份不变；原HeroIdentity默认子对象合并为基类CharacterIdentity，Blueprint默认值迁移须另行资产验证。
 */
UCLASS()
class DIVINEBEASTSABILITIESRUNTIME_API ADivineBeastsGameplayCharacter
    : public ADivineBeastsCharacter
{
    GENERATED_BODY()

public:
    ADivineBeastsGameplayCharacter();
    /** 保留既有公开虚函数符号，查询返回继承的唯一ASC，不再构造第二套能力系统。 */
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    /** GT正常启动和拥有事件先走权威基类，维持真实GAS/Combat事实与失败关闭。 */
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;

private:
    /** Pawn拥有唯一技能装配组件；只在可信身份Ready后授予，换身份/退出由组件撤销自有租约与授权。 */
    UPROPERTY(VisibleAnywhere, Category="DivineBeasts|Gameplay")
    TObjectPtr<UDivineBeastsAbilityLoadoutComponent> AbilityLoadout;
    /** GT幂等绑定继承ASC；服务器无Controller时只撤销，客户端无Controller只观察GAS，资格仍由Gate拒绝。 */
    void BindAbilityActorInfo();
};
