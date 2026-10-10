#pragma once

// 平台层GI遥测作用域，供客户端/服务器组合根与玩法诊断调用；只拥有自身缓冲、冻结Schema、采样与Sink。
// 记录不改变玩法权威结果；上下文只保存非敏感身份，账号/旅行边界和GI关闭按接口清理自有数据。

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "HAL/ThreadSafeCounter64.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/GamePlatformTelemetryTypes.h"
#include "GamePlatformTelemetrySubsystem.generated.h"

class FGamePlatformTelemetryBoundedBuffer;
class FGamePlatformTelemetryRateLimiter;
class FGamePlatformTelemetrySchemaRegistry;
class IGamePlatformTelemetrySink;

UCLASS()
class GAMEPLATFORMTELEMETRY_API UGamePlatformTelemetrySubsystem final
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

    /**
     * 线程契约：本子系统公开控制、上下文、记录与刷新接口均为 Game Thread Only（仅游戏线程）。
     * HTTP完成回调显式投递回游戏线程；后台生产事实由调用方投递到游戏线程并核实例/会话代次，
     * 不得从工作线程直接访问UObject，也不假定存在另一套Recorder实现。
     */

public:
    /** 构造/销毁在Private完整类型处定义；公开头只前置声明内部策略，不让UHT生成单元删除不完整类型。 */
    UGamePlatformTelemetrySubsystem();
    UGamePlatformTelemetrySubsystem(FVTableHelper& Helper);
    virtual ~UGamePlatformTelemetrySubsystem() override;

    /** 引擎GI首次初始化注册默认Schema/空Sink并冻结规则；关闭对象不重新初始化，后继GI使用新对象，模块Startup不创建用户会话。 */
    virtual void Initialize(
        FSubsystemCollectionBase& Collection) override;

    /** GI关闭先永久拒绝新命令/记录并撤销调度，再受控有界Drain与Shutdown；同步重入幂等，关闭实例不重新Initialize。 */
    virtual void Deinitialize() override;

    /** 先Start候选Sink，失败保留旧Sink并清理候选；成功替换后Flush/Shutdown旧Sink。
     * 仅游戏线程：关闭期或正在配置时嵌套Configure明确false，不Start候选；同步关闭后已Start候选必须Shutdown。
     * true表示返回时候选仍属于当前实例/Sink代次，不代表网络确认；同一已安装Sink须查询非Stopped且代次有效，不重复Start。 */
    bool ConfigureSink(
        TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe>
            InSink);

    /** 默认启用；关闭时尽力刷新已接纳数据并取消调度，之后新记录按关闭结果拒绝。 */
    void SetEnabled(bool bInEnabled);

    /** 设置确定性采样种子，默认gameplatform；空串按空种子参与采样，不传入凭据或密码。 */
    void SetSamplingSeed(FString InSamplingSeed);

    /** 默认关闭引擎Trace桥；开启只增加诊断输出，不改变记录授权、隐私或权威结果。 */
    void SetTraceBridgeEnabled(bool bInEnabled);

    /** 更新非敏感内容修订/环境字符串；空串表示未指定，统一去换行并限长度。 */
    void SetContentRevision(FString InContentRevision);

    void SetEnvironment(FString InEnvironment);

    /** 更新角色、区域、服务器实例标识；只描述本实例，不能执行分配或认证。 */
    void SetServerContext(
        FString InServerRole,
        FString InRegion,
        FString InServerInstanceId);

    /** 先结束旧会话，再记录非敏感SessionId和匿名化玩家ID并推进代次；不得传认证Token。
     * Sink换代不取消账号边界；Flush回调发布后继会话/旅行/世界操作时旧栈停止，不能覆盖后继上下文。 */
    void BeginSession(
        FString InSessionId,
        FString InPseudonymousPlayerId);

    /** 尽力刷新旧会话，清会话/匿名玩家/关联/比赛身份并推进代次；不代表业务登出已完成。
     * 换Sink后仍清理；仅关闭或真实后继上下文边界接管可使本次操作停止。 */
    void EndSession();

    /** 更新本世界的地图/世界/体验/比赛/模式只读诊断身份；空比赛/模式允许普通世界，字段均有界。
     * 发布后继世界同时取得上下文操作身份，旧旅行/会话Flush返回栈不能再覆盖本次发布。 */
    void UpdateWorldContext(
        FString InMapId,
        FString InWorldId,
        FString InExperienceId,
        FString InMatchId,
        FString InArenaModeId);

    /** 设置脱敏链路/业务事务身份，空串清空；关联ID不提供幂等或经济事务授权。 */
    void UpdateCorrelationContext(
        FString InCorrelationId,
        FString InTransactionId);

    /** 旅行前尽力刷新当前世界诊断并清世界字段；新世界由组合根重新发布上下文。
     * 输出器替换不取消旅行清理；回调中真正后继会话/旅行/世界操作接管后旧栈停止。 */
    void BeforeWorldTravel();

    /** 按值接收事件，验证Schema/隐私/采样/速率/容量后接纳；返回明确记录结果，接纳不是远端送达。 */
    EGamePlatformTelemetryRecordResult RecordEvent(
        FGamePlatformTelemetryEvent Event);

    /** 按值接收中立指标及标签，受同一Schema和缓冲预算约束；失败不影响业务动作。 */
    EGamePlatformTelemetryRecordResult RecordMetric(
        FGamePlatformTelemetryMetric Metric);

    /** 记录Counter增量，Delta单位由对应Schema决定；标签必须在Schema许可集合，默认空集合。 */
    EGamePlatformTelemetryRecordResult IncrementCounter(
        FName MetricName,
        double Delta,
        const TMap<FName, FString>& Labels = {});

    /** 记录Gauge瞬时值，单位/范围由Schema决定；非法值或标签返回明确拒绝结果。 */
    EGamePlatformTelemetryRecordResult RecordGauge(
        FName MetricName,
        double Value,
        const TMap<FName, FString>& Labels = {});

    /** 记录Histogram样本值，不在此创建第二个性能预算引擎；单位由Schema声明。 */
    EGamePlatformTelemetryRecordResult RecordHistogram(
        FName MetricName,
        double Value,
        const TMap<FName, FString>& Labels = {});

    /** 记录Duration，Milliseconds以毫秒计且须符合Schema；不会测量或伪造调用方耗时。 */
    EGamePlatformTelemetryRecordResult RecordDuration(
        FName MetricName,
        double Milliseconds,
        const TMap<FName, FString>& Labels = {});

    /** 非等待式向当前Sink转交批次；外部GetHealth/Submit换代立即停止旧栈，true仅表示已有批次转交，不保证确认或落盘。关闭拒绝此公开入口。 */
    bool FlushBestEffort();

    /**
     * 账号切换/隐私边界专用：撤销待刷新任务并丢弃尚未进入Sink的旧上下文记录。
     * 已进入旧NetworkSink的批次必须通过切换/Shutdown旧Sink终止其Retry链。
     */
    int32 DiscardBufferedRecordsForPrivacyBoundary();

    /** 返回本实例诊断值快照；只观察队列/丢弃/刷新，不暴露其他用户或可变内部注册表。 */
    UFUNCTION(BlueprintPure, Category="Telemetry")
    FGamePlatformTelemetryDiagnostics GetDiagnostics() const;

    /** 按值复制当前非敏感上下文，不能持有该副本作为后继世界或账号的权威身份。 */
    FGamePlatformTelemetryContext GetContextSnapshot() const;

    /** 旧C++不透明句柄兼容入口；Schema策略已内收Private，外部只提交事件/指标与读取诊断，不直接修改注册表。 */
    UE_DEPRECATED(5.8, "Schema注册表由遥测模块内部拥有，请使用RecordEvent/RecordMetric与GetDiagnostics。")
    TSharedRef<FGamePlatformTelemetrySchemaRegistry, ESPMode::ThreadSafe>
        GetSchemaRegistry() const;

private:
#if WITH_DEV_AUTOMATION_TESTS
    // 仅回归制造“有数据但已撤销调度”的前置，不暴露生产可变状态或复制实现。
    friend class FGamePlatformTelemetryCompletionGenerationTest;
#endif
    /** 分离上下文和Sink锁；外部Sink调用在锁外执行，避免同步完成重入死锁。 */
    mutable FCriticalSection ContextMutex;
    mutable FCriticalSection SinkMutex;

    /** GI拥有的当前上下文与采样值；账号/世界字段必须由真实组合根事件更新。 */
    FGamePlatformTelemetryContext Context;
    FString SamplingSeed = TEXT("gameplatform");

    /** 现行有界容量、间隔和关停秒预算；不把这些安全上限称为设备性能验收阈值。 */
    FGamePlatformTelemetryLimits Limits;

    TSharedPtr<
        FGamePlatformTelemetrySchemaRegistry,
        ESPMode::ThreadSafe> SchemaRegistry;

    /** 内部策略GI独占，销毁定义留Private；Schema冻结后只读，公开接口不操作私有实现。 */
    TUniquePtr<FGamePlatformTelemetryBoundedBuffer> Buffer;
    TUniquePtr<FGamePlatformTelemetryRateLimiter> RateLimiter;

    TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe> Sink;

    /** 本实例诊断序号、一次性刷新句柄与状态；不充当资源租约/玩法实体或网络复制身份。 */
    FThreadSafeCounter64 Sequence;
    FTSTicker::FDelegateHandle FlushTickerHandle;

    /** 每个UObject只初始化一个GI作用域；Deinitialize先永久关闭，任何Sink回调都不能重建本实例。 */
    bool bInitialized = false;
    bool bClosing = false;
    /** Start与旧Sink清理期间拒绝嵌套重配；Deinitialize仍可同步关闭并使候选失效。 */
    bool bConfiguringSink = false;
    uint64 LifecycleGeneration = 0;
    /** 每次发布/撤回实际Sink推进；旧GetHealth/Submit/Completion不得操作后继Sink的队列或调度。 */
    uint64 SinkGeneration = 0;
    /** 独立一次性调度资格，取消/消费后旧Ticker不得清除新句柄。 */
    uint64 FlushScheduleGeneration = 0;
    /** 游戏线程上下文边界操作身份：Begin/End/Travel/发布世界推进，和Sink代次及采样会话代次独立。
     * 最近真实边界拥有发布权；仅换输出器不取消账号/世界清理，外部Flush后旧栈不得覆盖后继边界。 */
    uint64 ContextOperationGeneration = 0;
    bool bEnabled = true;
    bool bTraceBridgeEnabled = false;
    bool bFlushInProgress = false;
    /** 会话边界代次、UTC最近刷新时刻与最近转交条数；零/默认时间表示尚未有该诊断。 */
    uint64 SessionGeneration = 0;
    FDateTime LastFlushUtc;
    int32 LastFlushRecords = 0;

    /** 核关闭/实例/上下文操作身份，不核Sink；该身份只用于边界发布权，不是业务认证或网络权威。 */
    bool IsContextOperationCurrent(uint64 ExpectedLifecycleGeneration, uint64 ExpectedContextOperationGeneration) const;
    /** 在调用方已登记的同一边界操作内尽力刷新并清旧会话；后继接管/关闭返回false，不清后继字段。
     * Begin不能调用公开End再建立另一身份，否则自己的后半程会误判为过期；参数为游戏线程捕获的代次。 */
    bool EndSessionInternal(uint64 ExpectedLifecycleGeneration, uint64 ExpectedContextOperationGeneration);
    /** 仅游戏线程核实际Sink/实例身份；最终Drain只允许关闭入口摘下的Sink，不能从公开请求绕过关闭。 */
    bool IsSinkScopeCurrent(uint64 ExpectedLifecycleGeneration, uint64 ExpectedSinkGeneration,
        const TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe>& ExpectedSink, bool bFinalDrain = false) const;
    /** 外部Sink调用后复核代次才继续BuildBatch/诊断/调度；最终Drain不允许创建Ticker。 */
    bool FlushToSink(const TSharedPtr<IGamePlatformTelemetrySink, ESPMode::ThreadSafe>& LocalSink,
        uint64 ExpectedLifecycleGeneration, uint64 ExpectedSinkGeneration, bool bFinalDrain);
    /** 一次性有工作刷新回调，DeltaSeconds以秒计；无缓冲不保留常驻逐帧扫描。 */
    bool TickFlush(float DeltaSeconds);
    /** 按需安排一次刷新；Buffer为空时不保留常驻Ticker。 */
    void ScheduleFlush(float DelaySeconds);
    /** 撤销尚未触发的一次性刷新。 */
    void CancelScheduledFlush();
    /** 新记录入队后根据批次阈值决定立即刷新还是安排延迟刷新。 */
    void RequestFlushAfterRecord();
    /** 上下文字段统一去除换行并限制长度，防止异常ID放大每条遥测记录。 */
    FString SanitizeContextValue(FString Value) const;

    /** 内部统一指标验证/封装入口，遵循游戏线程、Schema/隐私/容量及明确记录结果合同。 */
    EGamePlatformTelemetryRecordResult RecordMetricInternal(
        FName MetricName,
        EGamePlatformTelemetryMetricType Type,
        double Value,
        const TMap<FName, FString>& Labels);

    /** 根据不可变上下文派生采样键；仅诊断确定性用途，不作为认证或跨实例所有权证明。 */
    FString StableSamplingKey(
        const FGamePlatformTelemetryContext& ContextSnapshot) const;
};
