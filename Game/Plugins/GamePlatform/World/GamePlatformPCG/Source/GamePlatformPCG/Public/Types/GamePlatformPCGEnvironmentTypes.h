#pragma once

#include "CoreMinimal.h"
#include "GamePlatformPCGEnvironmentTypes.generated.h"

/** P0-P9稳定原语；1.x禁止通过额外编号绕过总体设计，人工覆盖始终作为跨原语辅助机制。 */
UENUM(BlueprintType)
enum class EGamePlatformPCGPrimitive : uint8
{
    P0_Field,
    P1_Scatter,
    P2_Linear,
    P3_Connector,
    P4_Parcel,
    P5_Assembly,
    P6_InterfaceBand,
    P7_Cavity,
    P8_SpatialGraph,
    P9_State
};

/** 世界PCG固定阶段编号；Minor/Patch版本不得重排。 */
UENUM(BlueprintType)
enum class EGamePlatformPCGWorldStage : uint8
{
    FieldRead = 0,
    TerrainWrite = 1,
    WaterBody = 2,
    Networks = 3,
    CutFillRequest = 4,
    Connectors = 5,
    Parcels = 6,
    Enclosures = 7,
    Buildings = 8,
    Interiors = 9,
    Scatter = 10,
    GameplayAnchors = 11,
    InterfaceBands = 12,
    ApplyState = 13,
    StreamHooks = 14,
    RuntimeDetail = 15
};

UENUM(BlueprintType)
enum class EGamePlatformPCGConnectorType : uint8
{
    Gate,
    Bridge,
    Intersection,
    Break
};

UENUM(BlueprintType)
enum class EGamePlatformPCGEnclosureKind : uint8
{
    Fence,
    Railing,
    Barrier,
    Wall,
    Hedge
};

UENUM(BlueprintType)
enum class EGamePlatformPCGParcelUse : uint8
{
    Field,
    Orchard,
    Yard,
    Plaza,
    Lake,
    Pen,
    Block,
    Plot
};

UENUM(BlueprintType)
enum class EGamePlatformPCGSide : uint8
{
    Left,
    Right,
    Both,
    DownslopeOnly
};

/** 人工覆盖属于跨原语编辑器辅助机制，不占用新的Primitive编号。 */
UENUM(BlueprintType)
enum class EGamePlatformPCGOverrideMode : uint8
{
    None,
    Freeze,
    PaintExclude,
    ForceSpline,
    ManualConnector
};

UENUM(BlueprintType)
enum class EGamePlatformPCGNetPolicy : uint8
{
    ClientGen,
    ServerAuth,
    ReplicatedState
};

UENUM(BlueprintType)
enum class EGamePlatformPCGBakeState : uint8
{
    None,
    Dirty,
    Generating,
    Baked,
    ValidationFailed
};

UENUM(BlueprintType)
enum class EGamePlatformPCGExclusionMode : uint8
{
    Hard,
    Soft,
    DensityScale
};
