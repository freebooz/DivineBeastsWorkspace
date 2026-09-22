#pragma once
#include "CoreMinimal.h"
#include "Types/GamePlatformResult.h"
class UGamePlatformAbilitySystemComponent;

/**
 * 项目组合根提供的当前玩法资格读取器。服务器查询真实Gameplay Active及当前Pawn拥有关系；
 * 客户端读取自身复制事实，只决定是否可尝试预测。查询必须同步有界，不发HTTP、不重入ASC写接口。
 */
class GAMEPLATFORMABILITYSYSTEM_API IGamePlatformAbilityActivationGate
{
public:
    virtual ~IGamePlatformAbilityActivationGate() = default;
    /** 非成功即拒绝，默认无提供者拒绝；不能将客户端报告解释成服务器授权。 */
    virtual FGamePlatformResult Evaluate(const UGamePlatformAbilitySystemComponent& Component) const = 0;
};
