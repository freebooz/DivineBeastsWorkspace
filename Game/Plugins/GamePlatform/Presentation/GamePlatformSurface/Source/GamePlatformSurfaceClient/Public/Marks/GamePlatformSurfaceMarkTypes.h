// 通用表面命中痕迹合同：仅客户端视觉，不承担伤害、碰撞或网络权威。
#pragma once
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Chaos/ChaosEngineInterface.h" // EPhysicalSurface（物理表面类型）在PhysicsCore内声明，公共合同不得依赖间接包含。
#include "GamePlatformSurfaceMarkTypes.generated.h"

class UPrimitiveComponent;

/** 五种通用命中表面痕迹，项目层可替换液体为能量或墨迹。 */
UENUM(BlueprintType)
enum class EGamePlatformSurfaceMarkKind : uint8
{
    Bullet UMETA(DisplayName="弹孔"),
    Scorch UMETA(DisplayName="爆炸焦痕"),
    Slash UMETA(DisplayName="刀砍／撞击痕"),
    Fluid UMETA(DisplayName="液体／能量飞溅"),
    Crack UMETA(DisplayName="墙壁／地面裂痕")
};

/** Pending仅表示已进入异步资源队列，不代表完成渲染。 */
UENUM(BlueprintType)
enum class EGamePlatformSurfaceMarkStatus : uint8
{
    Spawned UMETA(DisplayName="已生成"),
    Pending UMETA(DisplayName="资源准备中"),
    Duplicate UMETA(DisplayName="重复事件"),
    Disabled UMETA(DisplayName="功能关闭"),
    InvalidWorld UMETA(DisplayName="世界不可用"),
    InvalidRequest UMETA(DisplayName="命中参数非法"),
    MissingAssets UMETA(DisplayName="资源目录或材质不可用"),
    BudgetExceeded UMETA(DisplayName="队列或预算超限")
};

/** 厘米单位；只传已确认命中的客户端表现事实。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSURFACECLIENT_API FGamePlatformSurfaceMarkRequest
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SurfaceMark")
    EGamePlatformSurfaceMarkKind Kind = EGamePlatformSurfaceMarkKind::Bullet;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SurfaceMark")
    FGuid EventId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SurfaceMark", meta=(Units="cm"))
    FVector Location = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SurfaceMark")
    FVector Normal = FVector::UpVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SurfaceMark")
    FVector Direction = FVector::ForwardVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SurfaceMark")
    TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SurfaceMark")
    TObjectPtr<UPrimitiveComponent> HitComponent;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SurfaceMark", meta=(ClampMin="0.1", ClampMax="8.0"))
    float SizeScale = 1.0f;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMSURFACECLIENT_API FGamePlatformSurfaceMarkResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="SurfaceMark")
    EGamePlatformSurfaceMarkStatus Status = EGamePlatformSurfaceMarkStatus::InvalidRequest;
    UPROPERTY(BlueprintReadOnly, Category="SurfaceMark")
    int32 ActiveCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="SurfaceMark")
    int32 PendingCount = 0;
};

/** 纯值边界，不读取世界或资产。 */
struct GAMEPLATFORMSURFACECLIENT_API FGamePlatformSurfaceMarkPolicy
{
    static bool IsValidRequest(const FGamePlatformSurfaceMarkRequest& Request);
    static bool IsValidKind(EGamePlatformSurfaceMarkKind Kind);
};
