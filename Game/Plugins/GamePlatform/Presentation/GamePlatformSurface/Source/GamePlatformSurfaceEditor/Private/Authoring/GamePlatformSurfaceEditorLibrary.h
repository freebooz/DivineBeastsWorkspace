#pragma once

#include "CoreMinimal.h"

/** GamePlatformSurface编辑器显式资产生成入口；禁止模块启动时自动改写内容。 */
namespace GamePlatformSurfaceEditor
{
    /**
     * 创建或验证标准MPC。
     * ValidateOnly=true时只验证，不创建；默认模式仅在目标不存在时首次创建，绝不覆盖已有资产。
     */
    bool EnsureCoreParameterCollection(bool bValidateOnly, FString& OutError);
}
