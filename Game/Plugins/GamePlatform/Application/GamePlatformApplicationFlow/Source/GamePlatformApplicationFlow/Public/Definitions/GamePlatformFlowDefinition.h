#pragma once

#include "Definitions/GamePlatformDefinitionBase.h"
#include "GamePlatformFlowDefinition.generated.h"

/** 只描述中立节点与路由，不存节点实例、世界、网络地址或项目业务代码。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMAPPLICATIONFLOW_API FGamePlatformFlowNodeDefinition
{
    GENERATED_BODY()
    /** 图内唯一节点名称；不可为空，FName大小写等价。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Flow")
    FName NodeId = NAME_None;
    /** 由当前GameInstance组合根显式注册的执行器键，不通过字符串反射创建任意类。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Flow")
    FName ExecutorId = NAME_None;
    /** 可选输入主资产身份；平台仅传给Context，实际读取与租约由节点所属领域负责。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Flow")
    FPrimaryAssetId InputDefinitionId;
    /** 单次执行真实秒数，必须有限且大于零；资产模式不隐式重试。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Flow")
    double TimeoutSeconds = 30.0;
    /** 默认成功边，None表示成功终点。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Flow")
    FName NextNodeId = NAME_None;
    /** 具名成功事件到后继的映射；键不能为None，值为None表示终点。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Flow")
    TMap<FName, FName> Routes;
};

/** 只读流程资产，经Data成功租约消费；继承的身份、版本和必需依赖仍由Data负责。 */
UCLASS(BlueprintType)
class GAMEPLATFORMAPPLICATIONFLOW_API UGamePlatformFlowDefinition : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    /** 必须对应Nodes中的节点，所有节点必须从此入口可达。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Flow")
    FName EntryNodeId = NAME_None;
    /** 原子校验的完整节点集合；启动前验证所有路由和所有工厂注册。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Flow")
    TArray<FGamePlatformFlowNodeDefinition> Nodes;
    /** 显式允许资产图循环；默认拒绝，旧Configure始终保留DAG合同。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Flow")
    bool bAllowCycles = false;
    /** 连续即时完成片段中的重复节点跳转上限；只限制循环重访，不限制2000节点等长无环链。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Flow", meta=(ClampMin="1"))
    int32 MaxImmediateCycleTransitions = 64;
    /** 先执行Data基类校验，再校验图结构、输入身份和预算；不创建节点，不加载资源。 */
    virtual FGamePlatformResult ValidateDefinition() const override;
};
