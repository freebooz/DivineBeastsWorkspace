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
/**
 * 本地输入Definition（定义）基类，身份/版本继承GamePlatformData；不包含认证、世界或玩法权威状态。
 * 项目层仅在确有新增结构/校验时允许单向派生；纯PC/移动设备差异优先使用同一Profile的数据实例，避免机械增加C++子类。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMINPUTCLIENT_API UGamePlatformInputProfileDefinition : public UGamePlatformDefinitionBase
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
    /** PC键鼠是否允许使用该Profile；关闭只影响平台层接入策略，不卸载原生设备。 */
    UPROPERTY(EditDefaultsOnly, Category="Input|Devices") bool bEnableKeyboardMouse = true;
    /** PC/主机手柄是否允许使用该Profile。 */
    UPROPERTY(EditDefaultsOnly, Category="Input|Devices") bool bEnableGamepad = true;
    /** Android/iOS等移动端Touch（触控）是否允许通过统一语义注入。 */
    UPROPERTY(EditDefaultsOnly, Category="Input|Devices") bool bEnableTouch = true;
    /** 移动端虚拟摇杆径向死区0..0.5；仅Touch Move使用。 */
    UPROPERTY(EditDefaultsOnly, Category="Input|Devices") double TouchAnalogDeadZone = 0.12;
    /** 默认视角无障碍/舒适度偏好；玩家本地偏好可以在运行时覆盖。 */
    UPROPERTY(EditDefaultsOnly, Category="Input|Accessibility") FGamePlatformInputAccessibilitySettings DefaultAccessibility;
    /** 配置基础字段校验不加载UObject；已加载原生类型另在准备阶段校验。 */
    virtual FGamePlatformResult ValidateDefinition() const override;
};
