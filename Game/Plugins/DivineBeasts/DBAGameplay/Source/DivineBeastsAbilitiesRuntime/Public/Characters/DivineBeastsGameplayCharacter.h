#pragma once

#include "CoreMinimal.h"
#include "Characters/DivineBeastsCharacter.h"
#include "DivineBeastsGameplayCharacter.generated.h"

class UDivineBeastsAbilityLoadoutComponent;
class USpringArmComponent;
class UCameraComponent;

/**
 * ADivineBeastsGameplayCharacter（神兽联盟双端可玩角色）。
 * 继承项目权威Pawn的唯一ASC/Combat/CharacterIdentity/GameplayEligibility及死亡、失控和退出保护，
 * 本类拥有唯一技能Loadout及主线跟随镜头/输入装配；双端保留ACharacter运动基础，不写死生肖角色类。
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
    /** GT绑定主线原生运动/镜头输入；只使用引擎Pawn输入，不授予技能、不提交坐标或绕过服务器资格。 */
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

private:
    /** Pawn拥有主线稳定名镜头臂/跟随镜头，无项目资源硬引用；默认对象随Pawn释放，服务器仍用原生运动复制。 */
    UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> FollowCamera;
    /** 本地输入轴值沿控制器水平朝向请求运动；无控制器或移动输入被忽略时不请求，服务器运动复制仍权威。 */
    void MoveForward(float Value);
    void MoveRight(float Value);
    /** 本地镜头轴输入交给引擎Controller处理；不修改权威位置、出生身份或技能资格。 */
    void TurnCamera(float Value);
    void LookCamera(float Value);
    /** Pawn拥有唯一技能装配组件；只在可信身份Ready后授予，换身份/退出由组件撤销自有租约与授权。 */
    UPROPERTY(VisibleAnywhere, Category="DivineBeasts|Gameplay")
    TObjectPtr<UDivineBeastsAbilityLoadoutComponent> AbilityLoadout;
    /** GT幂等绑定继承ASC；服务器无Controller时只撤销，客户端无Controller只观察GAS，资格仍由Gate拒绝。 */
    void BindAbilityActorInfo();
};
