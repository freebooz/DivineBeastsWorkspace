#pragma once

#include "CoreMinimal.h"
#include "GamePlatformVFXTypes.generated.h"

/** VFX 行为类型。它描述“如何运行”，不代表业务内容分类。 */
UENUM(BlueprintType)
enum class EGamePlatformVFXBehavior : uint8
{
    Instant      UMETA(DisplayName="Instant / 瞬时型"),
    Attached     UMETA(DisplayName="Attached / 附着型"),
    Projectile   UMETA(DisplayName="Projectile / 视觉投射型"),
    Beam         UMETA(DisplayName="Beam / 光束型"),
    Area         UMETA(DisplayName="Area / 区域型"),
    Shield       UMETA(DisplayName="Shield / 护盾表现型"),
    Portal       UMETA(DisplayName="Portal / 传送门表现型"),
    Trail        UMETA(DisplayName="Trail / 拖尾型"),
    World        UMETA(DisplayName="World / 世界环境型"),
    Composite    UMETA(DisplayName="Composite / 复合型")
};

/** Catalog 来源作用域；用于确定性覆盖规则，不等于插件物理路径。 */
UENUM(BlueprintType)
enum class EGamePlatformVFXCatalogScope : uint8
{
    Platform    UMETA(DisplayName="Platform / 平台默认"),
    Moba        UMETA(DisplayName="Moba / MOBA通用"),
    Project     UMETA(DisplayName="Project / 项目默认"),
    ContentPack UMETA(DisplayName="ContentPack / 内容包")
};

/** VFX 生命周期。 */
UENUM(BlueprintType)
enum class EGamePlatformVFXLifecycleState : uint8
{
    Invalid,
    Requested,
    Loading,
    Spawning,
    Active,
    Stopping,
    Completed,
    Cancelled,
    Failed
};

/** VFX 请求的同步接收结果。异步加载失败通过实例状态和诊断系统体现。 */
UENUM(BlueprintType)
enum class EGamePlatformVFXPlayResultCode : uint8
{
    Accepted,
    InvalidRequest,
    ResolveFailed,
    WorldUnavailable,
    BudgetRejected
};

/** 表现重要等级；用于桥接 Niagara Scalability，而不是另造一套渲染预算系统。 */
UENUM(BlueprintType)
enum class EGamePlatformVFXImportance : uint8
{
    Critical,
    High,
    Normal,
    Ambient
};

/** 平台抽象池化模式，避免将 Niagara 的具体枚举泄漏到高层请求结构。 */
UENUM(BlueprintType)
enum class EGamePlatformVFXPoolingMode : uint8
{
    None,
    AutoRelease,
    ManualRelease
};
