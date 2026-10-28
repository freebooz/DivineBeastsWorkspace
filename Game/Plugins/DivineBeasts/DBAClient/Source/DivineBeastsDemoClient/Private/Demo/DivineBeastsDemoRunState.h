// SSLM演示纯状态机：世界私有，持值身份/单调截止时间，不持UObject、委托或资源。
#pragma once
#include "Contracts/DivineBeastsDemoRunTypes.h"

namespace DivineBeasts::Demo
{
/** 游戏线程串行驱动，终态/操作完成按完整身份与序号至多一次；测试可注入时钟。 */
class FRunState
{
public:
    /** 仅空闲/终态可开始，拒绝非法身份/时间/超时；成功进入Preparing。 */
    bool Begin(const FDivineBeastsDemoRunToken& Token, FName ScenarioId, FName FeatureId, double NowSeconds, double PrepareTimeoutSeconds);
    /** 只接受当前准备完成；成功进入Running，失败保持Preparing供调用方启动清理。 */
    bool CompletePreparation(const FDivineBeastsDemoRunToken& Token, const FGamePlatformResult& Result, double NowSeconds);
    /** Running且无在途命令时登记操作；OutSerial仅成功才非零。 */
    bool BeginOperation(const FDivineBeastsDemoRunToken& Token, uint64& OutSerial);
    /** 完整Token/活动阶段/单调时钟校验的只读观察；不改操作与清理结果，重复同值不广播。 */
    bool RecordObservation(const FDivineBeastsDemoRunToken& Token, const FString& Text, double NowSeconds);
    /** 完整Token及操作序号都一致才写原始结果；重复/旧回调返回false。 */
    bool CompleteOperation(const FDivineBeastsDemoRunToken& Token, uint64 Serial, const FGamePlatformResult& Result);
    /** 准备或运行中可停止；先失效在途操作，终态等待清理完成，重复Stop不重复调用适配器。 */
    bool BeginStop(const FDivineBeastsDemoRunToken& Token, const FGamePlatformResult& DesiredTerminal,
        double NowSeconds, double StopTimeoutSeconds);
    /** 只完成一次停止；清理失败覆盖生命周期终态，但保留原始OperationResult。 */
    bool CompleteStop(const FDivineBeastsDemoRunToken& Token, uint64 Attempt, const FGamePlatformResult& CleanupResult, double NowSeconds);
    /** 必要一次性截止检查：准备超时记录失败供清理，停止超时直接失败；不按帧轮询。 */
    bool Expire(double NowSeconds);
    /** 世界销毁立即撤销有效性并保存取消；不等待异步、不访问Actor。 */
    void AbandonWorld();
    /** 当前完整运行快照，调用者复制后再发布事件。 */
    const FDivineBeastsDemoRunSnapshot& GetSnapshot() const { return Snapshot; }
    /** 当前阶段单调截止秒数；0表示没有等待中的超时。 */
    double GetDeadlineSeconds() const { return DeadlineSeconds; }
    /** 是否正在持有此世界的唯一可写案例。 */
    bool IsActive() const;
    /** 清理失败仍保留所有权及重试资格，不能据终态字样覆盖实例。 */
    bool HasPendingCleanup() const { return Snapshot.bCleanupPending; }
    uint64 GetStopAttemptSerial() const { return StopSerial; }
private:
    /** 检查全部运行身份；终态后所有完成通知均被拒绝。 */
    bool IsCurrent(const FDivineBeastsDemoRunToken& Token) const;
    FDivineBeastsDemoRunSnapshot Snapshot;
    FGamePlatformResult DesiredTerminal;
    double DeadlineSeconds = 0.0;
    uint64 OperationSerial = 0;
    uint64 StopSerial = 0;
    bool bPrepareExpired = false;
};
}
