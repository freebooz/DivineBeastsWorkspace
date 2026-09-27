#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GamePlatformAbilityInput.generated.h"

/** 单实例技能输入触发策略；释放永远通知运行Spec，Held仅在未活动时尝试激活。 */
UENUM(BlueprintType)
enum class EGamePlatformAbilityActivationPolicy : uint8 { OnPressed, WhileHeld };
/** 当前端输入令牌，不上网传输；跨组件或旧Avatar输入会被拒绝。 */
struct GAMEPLATFORMABILITYSYSTEM_API FGamePlatformAbilityInputToken
{
    FGuid ScopeId;
    int64 AvatarGeneration = 0;
    bool IsValid() const { return ScopeId.IsValid() && AvatarGeneration > 0; }
};
