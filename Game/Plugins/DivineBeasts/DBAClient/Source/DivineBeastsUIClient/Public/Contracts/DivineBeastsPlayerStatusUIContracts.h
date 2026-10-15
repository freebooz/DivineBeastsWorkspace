#pragma once

#include "CoreMinimal.h"
#include "DivineBeastsPlayerStatusUIContracts.generated.h"

/**
 * FDivineBeastsPlayerStatusViewData（神兽联盟玩家状态只读投影）。
 * 不是 Gameplay 真源，只允许由事件驱动 ViewModel 从 GAS/战斗状态生成。
 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsPlayerStatusViewData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    double Health = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    double MaxHealth = 0.0;

    /** 独立的通用能量，只驱动血条下方蓝条；气势有自己的MomentumBar。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    double Energy = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    double MaxEnergy = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    double Momentum = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    double MaxMomentum = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    bool bDead = false;
};
