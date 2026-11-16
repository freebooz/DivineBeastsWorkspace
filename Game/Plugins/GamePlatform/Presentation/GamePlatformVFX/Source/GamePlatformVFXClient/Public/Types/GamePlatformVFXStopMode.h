#pragma once
#include "CoreMinimal.h"

/** 游戏线程停止模式。Immediate用于取消/世界退出；Drain仅供已确认结束的表现保留短尾粒子。 */
enum class EGamePlatformVFXStopMode : uint8
{
    Immediate,
    Drain
};
