// 平台客户端雪接触合同；厘米单位，仅影响表面表现，不拥有移动/碰撞/天气权威。
#pragma once
#include "CoreMinimal.h"
#include "GamePlatformSnowContactTypes.generated.h"

/** 一次已经由真实足底接地确认的压雪事实；EventId在所属世界内去重。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSURFACECLIENT_API FGamePlatformSnowContact
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface|Snow") FGuid EventId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface|Snow") FVector Location = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface|Snow") FVector Forward = FVector::ForwardVector;
    /** 完整鞋底长宽，厘米；不以角色胶囊中心假装足底。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface|Snow") float LengthCm = 30.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface|Snow") float WidthCm = 13.f;
    /** 归一化压实比例；实际凹陷仍被接收材质雪层高度限制。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface|Snow") float Pressure = .95f;
    bool IsValid() const;
};

/** 每只脚独立的接地门闩，供最终骨骼事件调用；不访问UObject或网络。 */
struct GAMEPLATFORMSURFACECLIENT_API FGamePlatformSnowFootContactGate
{
    bool bNearGround = false;
    bool bHasStamped = false;
    FVector LastLocation = FVector::ZeroVector;
    double LastTime = -1.;
    /** 近地12cm，离地18cm重置；移动中每脚相隔至少12cm/80ms。非法/空中输入不生成事件。 */
    bool Update(const FVector& Foot, float HeightAboveFloorCm, float SpeedCmPerSecond, bool bGrounded, double Now);
};
