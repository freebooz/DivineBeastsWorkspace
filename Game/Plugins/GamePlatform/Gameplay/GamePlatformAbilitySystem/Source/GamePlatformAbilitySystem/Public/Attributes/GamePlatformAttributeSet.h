#pragma once

#include "AttributeSet.h"
#include "GamePlatformAttributeSet.generated.h"

/**
 * UGamePlatformAttributeSet（游戏平台属性集基类）。
 *
 * 该类型仅建立跨游戏可复用的 GAS AttributeSet（属性集）公开类型边界，
 * 供 AbilitySet Definition（技能集合定义）安全限制 SoftClass（软类）类型。
 *
 * 平台层不预设生命、法力、攻击力等具体属性；这些属性必须由真正拥有该领域的
 * 通用机制插件或项目派生类型定义，避免基础层反向认识神兽联盟业务规则。
 */
UCLASS(Abstract, BlueprintType)
class GAMEPLATFORMABILITYSYSTEM_API UGamePlatformAttributeSet
    : public UAttributeSet
{
    GENERATED_BODY()
};
