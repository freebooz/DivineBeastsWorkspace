#pragma once
#include "Interfaces/IGamePlatformAbilityActivationGate.h"
#include "UObject/WeakObjectPtr.h"
class UDivineBeastsCharacterComponent;

/** 项目层只读激活资格适配；不保存业务权威状态，不发HTTP，所有事实在每次游戏线程查询时重新读取。 */
class FDivineBeastsCharacterActivationGate final : public IGamePlatformAbilityActivationGate
{
public:
    /** 注入时记录当前角色出生与Avatar代次；弱组件结束后不会继续授权。两个代次都必须来自可信初始化。 */
    FDivineBeastsCharacterActivationGate(UDivineBeastsCharacterComponent& Component, int32 SpawnGeneration, int32 AvatarGeneration);
    /** 当前Pawn/Owner/PC/PS/Gameplay Active任一不一致则失败关闭；客户端成功仅允许预测尝试。 */
    virtual FGamePlatformResult Evaluate(const UGamePlatformAbilitySystemComponent& Component) const override;
private:
    TWeakObjectPtr<UDivineBeastsCharacterComponent> CharacterComponent;
    int32 ExpectedSpawnGeneration = 0;
    int32 ExpectedAvatarGeneration = 0;
};
