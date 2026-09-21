#pragma once
#include "Containers/Ticker.h"

namespace GamePlatform::Data
{
/** 全部调用来自游戏线程；核心Ticker脱离旧世界计时器，缓存命中、空句柄也遵循延后完成。 */
inline void NextTick(TFunction<void()> Work)
{
    check(IsInGameThread());
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
        [Work = MoveTemp(Work)](float) mutable { Work(); return false; }));
}
}
