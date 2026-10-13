// SSLM项目开发目录内部验证器；只读调用者提供的定义，不公开索引/缓存，不拥有资源。
#pragma once
#include "Types/GamePlatformResult.h"
class UDivineBeastsDemoCatalogDefinition;

namespace DivineBeasts::Demo
{
    /** 游戏线程对完整候选定义做无副作用预检；返回第一项确定性错误，不查询资产可用性。 */
    FGamePlatformResult ValidateCatalog(const UDivineBeastsDemoCatalogDefinition& Catalog);
}
