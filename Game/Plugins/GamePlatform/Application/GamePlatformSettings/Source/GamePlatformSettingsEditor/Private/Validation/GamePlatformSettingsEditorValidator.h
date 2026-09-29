#pragma once

#include "CoreMinimal.h"

/** Editor/CI 静态验证入口；不修改设置、不创建用户档案。 */
struct FGamePlatformSettingsEditorValidator final
{
    /** 同时验证Client和Server Provider图；任一失败返回false并输出中文诊断。 */
    static bool ValidateAll(TArray<FString>& OutErrors);
};
