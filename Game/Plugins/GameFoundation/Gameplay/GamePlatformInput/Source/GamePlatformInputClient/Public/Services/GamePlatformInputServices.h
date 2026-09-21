#pragma once
#include "Types/GamePlatformInputTypes.h"
#include "GameplayTagContainer.h"

/** 唯一语义命名与单位转换入口；纯查询不加载配置、不触发输入或网络。 */
namespace GamePlatformInputServices
{
    /** 标签由本模块原生注册，未知枚举返回空标签；不依赖Config/Tags自动扫描。 */
    GAMEPLATFORMINPUTCLIENT_API FGameplayTag GetSemanticTag(EGamePlatformInputSemantic Semantic);
    /** Move归一轴，LookDelta为度增量，LookRate为度/秒，其余布尔请求。 */
    GAMEPLATFORMINPUTCLIENT_API EGamePlatformInputUnit GetUnit(EGamePlatformInputSemantic Semantic);
    /** 动作归属唯一通道，菜单/确认/取消不会因仅屏蔽Gameplay而失效。 */
    GAMEPLATFORMINPUTCLIENT_API uint8 GetChannel(EGamePlatformInputSemantic Semantic);
}
