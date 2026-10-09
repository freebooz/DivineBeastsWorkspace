// 平台中立只读领域投影：后端拥有权益/成长/运营权威；游戏线程服务产生值副本，单位/空值语义如下，不保存认证秘密。
#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformProgressionTypes.h"
#include "GamePlatformProgressionClientTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformProgressionClientState : uint8
{
    Uninitialized,
    Loading,
    Ready,
    Reconciling,
    Error
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMPROGRESSIONCLIENT_API FGamePlatformProgressionViewModel
{
    GENERATED_BODY()

    /** 成长轨道稳定身份；None缺失，客户端只读投影不授予XP。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    FName ProgressionTrackId = NAME_None;

    /** 后端主体稳定身份；空值无主体，客户端不能自行构造授权。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    FString SubjectId;

    /** 后端正整数等级；默认1，合法范围[1,MaxLevel]。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int32 Level = 1;

    /** 后端正整数等级上限；不小于Level，客户端不能用本地曲线修改权威上限。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int32 MaxLevel = 1;

    /** 非负累计XP单位整数；0合法，不用浮点表示长期经验。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int64 TotalXP = 0;

    /** 根据兼容曲线派生的本等级内XP单位整数；曲线不兼容时0且不宣称进度有效。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int64 XPIntoLevel = 0;

    /** 兼容曲线计算的下一等级所需XP单位整数；最大等级或不兼容可为0。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int64 XPForNextLevel = 0;

    /** 派生进度比例[0,1]；满级为1，不兼容为0并结合bCurveCompatible解释。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    float ProgressPercent = 0.0f;

    /** 本地曲线身份/版本/主体类型是否兼容后端轨道；默认false，不兼容不伪造进度。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    bool bCurveCompatible = false;
};
