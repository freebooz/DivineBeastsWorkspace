// SSLM项目层Village专服狼AI：继承平台AI控制器，不复制感知、导航或伤害机制。
#pragma once

#include "Controllers/GamePlatformAIController.h"
#include "DivineBeastsVillageWolfAIController.generated.h"

/** 原生轻量脑近战适配：仅服务器可执行；不保存玩家硬引用，不向客户端提供任意伤害RPC。 */
UCLASS()
class ADivineBeastsVillageWolfAIController : public AGamePlatformAIController
{
    GENERATED_BODY()
public:
    ADivineBeastsVillageWolfAIController();

protected:
    /** 人型怪物读取已验证目标英雄定义的基础移动速度；狼保持原有速度。 */
    virtual TOptional<float> ResolveCombatTargetBaseWalkSpeed(const AActor& ValidatedTarget) const override;

    /** 再次核对目标、视线、距离、安全区与生命代次；最后通过平台Combat唯一入口提交伤害。 */
    virtual bool TryExecuteNativeAttack(AActor& ValidatedTarget) override;
};
