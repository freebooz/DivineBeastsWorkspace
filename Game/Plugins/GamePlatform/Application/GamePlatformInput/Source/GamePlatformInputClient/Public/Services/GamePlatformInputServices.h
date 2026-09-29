#pragma once
#include "Types/GamePlatformInputTypes.h"
#include "GameplayTagContainer.h"
struct FGamePlatformInputActionDefinition;

/** 唯一语义命名与单位转换入口；纯查询不加载配置、不触发输入或网络。 */
namespace GamePlatformInputServices
{
    /** 返回平台长期稳定Built-in语义Tag；新项目通用语义代码优先使用该入口。 */
    GAMEPLATFORMINPUTCLIENT_API FGameplayTag GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic Semantic);
    /** 返回平台Built-in语义的完整Descriptor；Profile/项目桥可直接复用，不经过Legacy枚举。 */
    GAMEPLATFORMINPUTCLIENT_API FGamePlatformInputSemanticDescriptor GetBuiltInSemanticDescriptor(EGamePlatformBuiltInInputSemantic Semantic);
    /** 标签由本模块原生注册，未知枚举返回空标签；不依赖Config/Tags自动扫描。 */
    GAMEPLATFORMINPUTCLIENT_API FGameplayTag GetSemanticTag(EGamePlatformInputSemantic Semantic);
    /** Move归一轴，LookDelta为度增量，LookRate为度/秒，其余布尔请求。 */
    GAMEPLATFORMINPUTCLIENT_API EGamePlatformInputUnit GetUnit(EGamePlatformInputSemantic Semantic);
    /** 动作归属唯一通道，菜单/确认/取消不会因仅屏蔽Gameplay而失效。 */
    GAMEPLATFORMINPUTCLIENT_API uint8 GetChannel(EGamePlatformInputSemantic Semantic);
    /** 旧枚举兼容描述；新代码优先使用Profile中的Descriptor。 */
    GAMEPLATFORMINPUTCLIENT_API FGamePlatformInputSemanticDescriptor GetLegacySemanticDescriptor(EGamePlatformInputSemantic Semantic);
    /** 校验一个数据化语义描述是否满足平台运行时约束。 */
    GAMEPLATFORMINPUTCLIENT_API bool IsValidSemanticDescriptor(const FGamePlatformInputSemanticDescriptor& Descriptor);
    /** 解析新版Descriptor或旧枚举兼容字段；高频路径不调用本函数。 */
    GAMEPLATFORMINPUTCLIENT_API bool ResolveActionDescriptor(
        const FGamePlatformInputActionDefinition& Action,
        FGamePlatformInputSemanticDescriptor& OutDescriptor,
        bool& bOutHasLegacySemantic,
        EGamePlatformInputSemantic& OutLegacySemantic);
}
