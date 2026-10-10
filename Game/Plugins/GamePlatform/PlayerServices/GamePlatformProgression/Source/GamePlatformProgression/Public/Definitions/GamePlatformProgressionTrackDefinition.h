// GamePlatformProgression公开共享纯值/静态定义合同：上层调用方遵循下述线程/参数/终态；后端拥有长期数据权威，资产由数据/内容服务拥有。
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/GamePlatformProgressionTypes.h"
#include "GamePlatformProgressionTrackDefinition.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMPROGRESSION_API UGamePlatformProgressionTrackDefinition final
    : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** 稳定成长轨道身份；None无效，项目以内容实例提供具体曲线。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
    FName ProgressionTrackId = NAME_None;

    /** 适用成长主体类别，默认Character；Unknown无效。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
    EGamePlatformProgressionSubjectType SubjectType =
        EGamePlatformProgressionSubjectType::Character;

    /** 正整数曲线版本，默认1；必须匹配权威快照版本才派生进度。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin="1"))
    int32 CurveVersion = 1;

    /** 曲线起始正整数等级，默认1；不能大于MaxLevel。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin="1"))
    int32 MinLevel = 1;

    /** 曲线结束等级，默认1；展示不能改写后端权威等级上限。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin="1"))
    int32 MaxLevel = 1;

    /** 每等级累计XP单位整数阈值；数量为MaxLevel-MinLevel+1，首项0并严格递增。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
    TArray<int64> CumulativeXPThresholds;

    /** 满级后XP策略定义，目前仅ClampAtMax；后端负责权威应用。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression")
    EGamePlatformProgressionPostMaxXPPolicy PostMaxXPPolicy =
        EGamePlatformProgressionPostMaxXPPolicy::ClampAtMax;

    /** 内容定义正整数版本，默认1，与曲线版本分别管理。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin="1"))
    int32 Version = 1;

    /** 游戏线程或不可变值读取：检查身份/版本/等级/阈值；false写OutReason诊断，true清空原因，不加载资源。 */
    bool Validate(FString& OutReason) const;
    /** 调用前应Validate成功；TotalXP为单位整数，展示夹到[0,最大累计XP]后二分查等级，空曲线返回MinLevel。 */
    int32 CalculateLevel(int64 TotalXP) const;
    /** 返回最后累计XP阈值，空曲线0；纯只读单位整数，不修改权威XP。 */
    int64 GetMaxTotalXP() const;
    /** TotalXP夹到有效范围后返回当前等级内XP单位整数；曲线索引无效返回0。 */
    int64 CalculateXPIntoLevel(int64 TotalXP) const;
    /** 返回下一等级与当前等级累计阈值的单位整数差；满级/无有效阈值返回0。 */
    int64 CalculateXPForNextLevel(int64 TotalXP) const;
};
