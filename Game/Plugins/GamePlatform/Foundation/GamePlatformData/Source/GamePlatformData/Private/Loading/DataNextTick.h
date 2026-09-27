#pragma once
#include "Containers/Ticker.h"

namespace GamePlatform::Data
{
/**
 * 游戏线程下一调度轮或更晚执行，不使用世界计时器。UE5.8会在同次Tick消费新AddTicker；
 * 首次回调只return true，使同一个元素进入TickedElements，下一次外层Tick才执行Work。
 * 零Delta、Ticker内部提交、持续未发现后重排均不会同轮递归；外部提交可能需要两轮。
 * Ticker参数允许实际独立FTSTicker的零Delta回归验证，不引入模拟调度器。
 */
inline void NextTick(TFunction<void()> Work, FTSTicker& Ticker = FTSTicker::GetCoreTicker())
{
    check(IsInGameThread());
    Ticker.AddTicker(FTickerDelegate::CreateLambda(
        [Work = MoveTemp(Work), bHasDeferred = false](float) mutable
        {
            if (!bHasDeferred) { bHasDeferred = true; return true; }
            Work();
            return false;
        }));
}
}
