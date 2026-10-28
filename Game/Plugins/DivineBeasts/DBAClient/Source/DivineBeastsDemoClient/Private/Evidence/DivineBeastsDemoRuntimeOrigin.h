// SSLM开发运行来源：项目Private诊断，Writer消费真实模块观察；不宣称构建资格、不签人工结果。
#pragma once
#include "CoreMinimal.h"
#include "Types/GamePlatformResult.h"
class FJsonObject;
namespace DivineBeasts::Demo
{
    /** 私有来源文件读取：拒绝链接、缺失、空、超64MiB或读取期间变化；不赋予模块来源资格。 */
    FString HashRuntimeOriginFile(const FString& Path);
    /** 游戏线程解析固定启动身份/内容摘要及禁热重载标志；失败清空输出，不执行任意参数或文件路径。 */
    FGamePlatformResult ParseRuntimeOriginArguments(const TCHAR* Arguments, FGuid& OutLaunchId, FString& OutContentHash);
    /** 游戏线程观察实际已加载模块文件路径/摘要，读取失败保留具名诊断但不伪造证明。
     * 仅Editor受控来源可被后续独立工具补证；Client/手工启动仍可导出unverified原始证据。
     */
    TSharedRef<FJsonObject> CaptureRuntimeOrigin();
}
