#pragma once

#include "CoreMinimal.h"
#include "GamePlatformVFXTypes.generated.h"

/** VFX 行为类型。保持与项目层美术语义解耦。 */
UENUM(BlueprintType)
enum class EGamePlatformVFXBehavior : uint8
{
    Instant,
    Attached,
    Projectile,
    Beam,
    Area,
    Shield,
    Portal,
    Trail,
    World,
    Composite
};

/** VFX内容制作/检索分类，不参与Gameplay权威逻辑。 */
UENUM(BlueprintType)
enum class EGamePlatformVFXContentCategory : uint8
{
    Core,
    EnergyShield,
    CastMagic,
    ProjectileBeam,
    HitDamageExplosion,
    AreaWarning,
    StatusControl,
    CharacterWeapon,
    Movement,
    SummonTransform,
    PortalSpacetime,
    Environment,
    InteractionFeedback,
    UIScreen,
    UltimateComposite,
    Experimental,
    Recovery
};

/** VFX Catalog作用域。Shared可由中间复用层使用，平台层不依赖具体上层模块。 */
UENUM(BlueprintType)
enum class EGamePlatformVFXCatalogScope : uint8
{
    Platform = 0,
    Shared = 1,
    Project = 2,
    ContentPack = 3
};

/** 跨平台质量档，仅选择表现资源，不改变Gameplay尺寸或时序。 */
UENUM(BlueprintType)
enum class EGamePlatformVFXQualityTier : uint8
{
    Low,
    Medium,
    High,
    Epic
};

/** 客户端表现预测状态，用于预测/确认去重与拒绝清理。 */
UENUM(BlueprintType)
enum class EGamePlatformVFXPredictionState : uint8
{
    None,
    Predicted,
    Confirmed,
    Cancelled
};

/** 业务重要度用于可伸缩策略，不等同于 Niagara 自身质量等级。 */
UENUM(BlueprintType)
enum class EGamePlatformVFXImportance : uint8
{
    Critical,
    Combat,
    Status,
    Ambient
};

/** 播放请求的确定性结果。 */
UENUM(BlueprintType)
enum class EGamePlatformVFXResultCode : uint8
{
    Played,
    Queued,
    Cancelled,
    RejectedByScalability,
    CatalogMiss,
    CatalogAmbiguous,
    DefinitionLoadFailed,
    InvalidWorld,
    InvalidRequest
};
