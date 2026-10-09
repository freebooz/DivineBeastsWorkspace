#pragma once

#include "CoreMinimal.h"

class GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsServerTimeEstimator
{
public:
    /** 由所属LocalPlayer在游戏线程撤销时间样本；之后EstimatedServerNowUtc返回无效UTC。 */
    void Reset();
    /** 游戏线程输入正ticks服务器UTC；用单调秒时钟记录采样，非法UTC忽略，客户端不据此授奖。 */
    void Update(const FDateTime& ServerTimeUtc);
    /** 只读当前样本有效标志；同一实例只在游戏线程读写，不提供并发锁。 */
    bool IsValid() const { return bValid; }
    /** 返回服务器样本加单调时钟差的估计UTC；无样本返回无效时间，不读取系统时区。 */
    FDateTime EstimatedServerNowUtc() const;

private:
    FDateTime ServerTimeUtc;
    double MonotonicSecondsAtSnapshot = 0.0;
    bool bValid = false;
};
