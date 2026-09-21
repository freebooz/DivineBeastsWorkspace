#pragma once
#include "CoreMinimal.h"
class UGamePlatformPCGProfileDefinition;

namespace GamePlatformPCGEditor
{
    /** 游戏线程。哈希已保存包及递归硬/软包依赖的真实字节、工具/PCG源码和引擎版本；脏包/未知包拒绝。 */
    bool CalculateSourceFingerprint(const UGamePlatformPCGProfileDefinition& Profile,
        FString& Fingerprint, TArray<FString>& Dependencies, FString& Error);
    /** 稳定排序的UTF-8记录摘要；SHA-1仅作内容变化检测，绝不作安全签名。 */
    FString HashRecords(TArray<FString> Records);
}
