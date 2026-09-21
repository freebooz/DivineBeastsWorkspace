#pragma once
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Types/GamePlatformInputTypes.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GamePlatformInputProfileDefinition.generated.h"

/** 一条稳定语义只绑定一个IA，五个原生阶段均保留；不在事件回调加载资源。 */
USTRUCT(BlueprintType)
struct FGamePlatformInputActionDefinition
{
    GENERATED_BODY()
    /** 中立语义；不是具体英雄、技能或任意RPC名称。 */
    UPROPERTY(EditDefaultsOnly, Category="Input") EGamePlatformInputSemantic Semantic = EGamePlatformInputSemantic::Move;
    /** Data的Input分组软引用，激活整个配置期间保有租约。 */
    UPROPERTY(EditDefaultsOnly, Category="Input", meta=(AssetBundles="Input")) TSoftObjectPtr<UInputAction> Action;
    /** 明确单位，校验必须与语义匹配；视角消费者不得二次缩放。 */
    UPROPERTY(EditDefaultsOnly, Category="Input") EGamePlatformInputUnit Unit = EGamePlatformInputUnit::NormalizedAxis;
};
/** IMC使用权独立于Data租约，优先级固定范围0..100；共享时取活跃调用者最大值。 */
USTRUCT(BlueprintType)
struct FGamePlatformInputContextDefinition
{
    GENERATED_BODY()
    /** 配置内稳定唯一名字，不以软引用数组位置定位。 */
    UPROPERTY(EditDefaultsOnly, Category="Input") FName Name;
    /** 源资产运行时只读；不会ClearAllMappings或修改其MapKey。 */
    UPROPERTY(EditDefaultsOnly, Category="Input", meta=(AssetBundles="Input")) TSoftObjectPtr<UInputMappingContext> Context;
    /** 非共享上下文第二个拥有者会被拒绝，不由最后一次Add抢占。 */
    UPROPERTY(EditDefaultsOnly, Category="Input") bool bAllowSharing = true;
};
/** 本地输入定义，身份/版本继承Data；不包含任何认证、世界或玩法权威状态。 */
UCLASS(BlueprintType)
class GAMEPLATFORMINPUTCLIENT_API UGamePlatformInputProfileDefinition final : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    /** 至多32个动作；必须声明Move/Menu/Cancel，开发配置另包含全部请求动作。 */
    UPROPERTY(EditDefaultsOnly, Category="Input") TArray<FGamePlatformInputActionDefinition> Actions;
    /** 至多8个映射集合；登记可重绑与实际激活分离。 */
    UPROPERTY(EditDefaultsOnly, Category="Input") TArray<FGamePlatformInputContextDefinition> Contexts;
    /** 鼠标/触摸原始增量到角度，度/计数；0.001..10，不乘DeltaTime。 */
    UPROPERTY(EditDefaultsOnly, Category="Input") double LookDegreesPerCount = 0.1;
    /** 摇杆满幅速度，度/秒，1..720，由消费层乘帧间隔。 */
    UPROPERTY(EditDefaultsOnly, Category="Input") double LookDegreesPerSecond = 90;
    /** 模拟移动/观察径向死区0..0.5，只在本服务应用，不应再在IMC重复设置死区。 */
    UPROPERTY(EditDefaultsOnly, Category="Input") double AnalogDeadZone = 0.2;
    /** 配置基础字段校验不加载UObject；已加载原生类型另在准备阶段校验。 */
    virtual FGamePlatformResult ValidateDefinition() const override;
};
