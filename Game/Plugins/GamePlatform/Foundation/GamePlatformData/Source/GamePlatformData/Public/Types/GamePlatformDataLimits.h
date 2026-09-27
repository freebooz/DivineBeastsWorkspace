#pragma once

#include "CoreTypes.h"

/** GamePlatformData（游戏平台数据）运行期与编辑器共同使用的安全上限；保持单一真源，避免两端漂移。 */
namespace GamePlatform::Data::Limits
{
/** 单条依赖路径允许的最大Definition（定义）深度。 */
inline constexpr int32 MaxDependencyDepth = 128;
/** 单个根租约最多追踪的唯一Definition数量。 */
inline constexpr int32 MaxDefinitionsPerRequest = 4096;
/** 单个租约允许的最大去重Asset Bundle（资产束）数量。 */
inline constexpr int32 MaxBundlesPerLease = 64;
}
