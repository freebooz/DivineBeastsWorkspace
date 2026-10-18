// 平台双端中立运动参数；不认识项目法术/生肖/MOBA，不持有任何表现资源。仅可信服务器消费。
#pragma once
#include "CoreMinimal.h"
#include "GamePlatformCombatProjectileMotion.generated.h"

/** 直线/螺旋参数；厘米/秒及转/秒。螺旋本身不寻敌，只有服务器规格显式目标才启用追踪。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMCOMBAT_API FGamePlatformCombatProjectileMotion
{
    GENERATED_BODY()
    /** 轴向速度，合法范围(0,20000]厘米/秒；不接受客户端请求直接写入。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile")
    float SpeedCmPerSecond = 1000.0f;
    /** 锁定射程/基础轴向预算，合法范围(0,100000]厘米；初次弯弧可补偿最多0.5秒，不扩大锁定距离。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile")
    float RangeCm = 2000.0f;
    /** 球体碰撞半径，合法范围[1,100]厘米；与Niagara视觉大小无关。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile")
    float CollisionRadiusCm = 8.0f;
    /** 螺旋半径[0,100]厘米，0必须配0频率表示直线。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile")
    float HelixRadiusCm = 0.0f;
    /** 螺旋频率[0,10]转/秒；非零半径时必须大于0。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile")
    float HelixTurnsPerSecond = 0.0f;
    /** 初相位[0,1)转；连发可错开相位，起点仍为真实Actor出生位置。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Projectile")
    float InitialPhaseTurns = 0.0f;

    /** 只检查有限值及运动上界；此上界是实现合法区间，不冒充已经批准的CPU/GPU性能预算。 */
    bool IsValid() const;
};
