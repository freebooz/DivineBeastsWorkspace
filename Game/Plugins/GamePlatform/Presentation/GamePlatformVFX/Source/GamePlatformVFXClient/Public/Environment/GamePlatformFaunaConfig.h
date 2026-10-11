// 平台客户端群落值合同：厘米、厘米每秒和秒；只描述本地环境表现，不含项目物种或权威玩法。
#pragma once
#include "CoreMinimal.h"
#include "GamePlatformFaunaConfig.generated.h"

/** 通用运动模型。项目物种由第三层目录和资产指定。 */
UENUM(BlueprintType)
enum class EGamePlatformFaunaKind : uint8 { Butterfly, Fish };

/** 花点局部位置和停栖法线；容量包含接近中的预约，防止同时占用。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformFaunaFlower
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna") FVector Position = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna") FVector Normal = FVector::UpVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna", meta=(ClampMin="1",ClampMax="32")) int32 Capacity = 1;
};

/** 水中岛石或空中禁入球；以群落中心为原点，半径厘米。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformFaunaObstacle
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna") FVector Center = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna", meta=(ClampMin="0.1")) float Radius = 40.0f;
};

/** WorldDefinition的可选组合配置；序列化在既有Data定义中，不建立第二资源加载器。
 * 所有点均为局部厘米；水域为简单XY多边形，空数组使用盒边界，Z由HalfExtent限定。
 * Count硬上限是输入保护，不代表目标设备已验证的性能容量。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformFaunaConfig
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna") EGamePlatformFaunaKind Kind = EGamePlatformFaunaKind::Butterfly;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna", meta=(ClampMin="0",ClampMax="512")) int32 Count = 32;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna", meta=(ClampMin="0")) int32 Seed = 137;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna", meta=(Units="cm")) FVector HalfExtent = FVector(350.0,350.0,180.0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna", meta=(Units="cm/s",ClampMin="0.1")) float CruiseSpeed = 65.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna", meta=(Units="cm",ClampMin="0.1")) float BodyRadius = 7.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna", meta=(Units="cm",ClampMin="0.1")) float NeighborRadius = 150.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna", meta=(Units="cm",ClampMin="0.1")) float SeparationRadius = 35.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna", meta=(ClampMin="1",ClampMax="32")) int32 MaxNeighbors = 16;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna") TArray<FGamePlatformFaunaFlower> Flowers;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna") TArray<FGamePlatformFaunaObstacle> Obstacles;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fauna") TArray<FVector> WaterPolygon;
    /** 宽阶段和预约等结构有界校验；失败不生成实例。 */
    bool Validate(FString& OutError) const;
};

/** CPU行为诊断，不表示GPU可见数或帧耗。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformFaunaSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Fauna") int32 SimulatedCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="Fauna") int32 PerchedCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="Fauna") int32 ReservedFlowerSlots = 0;
    UPROPERTY(BlueprintReadOnly, Category="Fauna") bool bRenderingActive = false;
    UPROPERTY(BlueprintReadOnly, Category="Fauna") TArray<FVector> LocalPositions;
};
