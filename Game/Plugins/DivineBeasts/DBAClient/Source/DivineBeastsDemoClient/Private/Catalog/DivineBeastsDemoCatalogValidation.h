// SSLM项目开发目录内部验证器；只读调用者提供的定义，不公开索引/缓存，不拥有资源。
#pragma once
#include "Types/GamePlatformResult.h"
class UDivineBeastsDemoCatalogDefinition;

namespace DivineBeasts::Demo
{
    /** 身份原文在构造FName或提交类型化参数前检查；仅ASCII字母起始标识、长度小于NAME_SIZE，拒绝None。
     * 无副作用，不执行文本；作者定义、控件输入和直接C++命令共用同一规则，不能绕过UI输入检查。
     */
    bool IsStableIdentityText(const FString& Text);
    /** 无副作用资源路径语法；拒绝磁盘、子对象、空白和脚本文本，不代表资源存在或拥有。 */
    bool IsStableAssetPathText(const FString& Text);
    /** 游戏线程对完整候选定义做无副作用预检；返回第一项确定性错误，不查询资产可用性。 */
    FGamePlatformResult ValidateCatalog(const UDivineBeastsDemoCatalogDefinition& Catalog);
}
