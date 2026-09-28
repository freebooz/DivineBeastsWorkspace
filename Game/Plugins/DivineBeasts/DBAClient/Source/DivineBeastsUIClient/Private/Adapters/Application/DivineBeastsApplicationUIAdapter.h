#pragma once

#include "CoreMinimal.h"
#include "Contracts/DivineBeastsUIContracts.h"
#include "UObject/Object.h"
#include "DivineBeastsApplicationUIAdapter.generated.h"

class UDivineBeastsApplicationFlowSubsystem;
class ULocalPlayer;
struct FDivineBeastsFlowViewState;

/**
 * UDivineBeastsApplicationUIAdapter（神兽联盟应用流程UI适配器）。
 *
 * 这是 DBAClient（神兽联盟客户端组合插件）内部的单向组合适配器：
 * DivineBeastsUIClient（项目UI） -> DivineBeastsApplicationFlowClient（项目流程）。
 *
 * 职责：
 * - 把 ApplicationFlow（应用流程）的只读快照转换为统一 FDivineBeastsUIViewState；
 * - 把 UI 语义命令转发给项目流程公开入口；
 * - 只报告“命令是否被当前状态接纳”，最终业务成功仍由后续状态事件确认；
 * - 不直接访问 HTTP、后端、Actor、ASC 或资产加载器。
 *
 * 性能：
 * - 完全事件驱动，不创建 Tick；
 * - 只在流程快照真实变化时重建一次 UI 投影；
 * - 当前流程 Run 改变时更新 StateRunId，Revision 在同一 Run 内单调增加。
 */
UCLASS(Transient)
class UDivineBeastsApplicationUIAdapter final
    : public UObject,
      public IDivineBeastsUIQuerySource,
      public IDivineBeastsUICommandPort
{
    GENERATED_BODY()

public:
    /** 绑定当前 LocalPlayer 所属 GameInstance 的项目流程；仅第一个本地玩家负责首次启动主流程。 */
    bool Initialize(ULocalPlayer& LocalPlayer);

    /** 解绑流程事件并清理瞬态状态；重复调用安全。 */
    void Shutdown();

    virtual FDivineBeastsUIViewState GetUIViewState() const override
    {
        return ViewState;
    }

    virtual FDivineBeastsUIViewStateChangedNative& OnUIViewStateChanged() override
    {
        return StateChanged;
    }

    virtual void SubmitUICommand(
        const FDivineBeastsUICommand& Command,
        TFunction<void(const FDivineBeastsUICommandResult&)> Completion) override;

    virtual bool CancelUICommand(const FGuid& RequestId) override;

private:
    /** ApplicationFlow 快照变化时的唯一投影入口。 */
    void HandleFlowStateChanged(const FDivineBeastsFlowViewState& FlowState);

    /** 把当前流程状态转换为项目 UI 只读状态。 */
    FDivineBeastsUIViewState BuildViewState(
        const FDivineBeastsFlowViewState& FlowState);

    /** 立即返回一次命令接纳结果；业务终态通过 StateChanged 事件另行到达。 */
    static void CompleteCommand(
        const FDivineBeastsUICommand& Command,
        bool bAccepted,
        FName ErrorCode,
        TFunction<void(const FDivineBeastsUICommandResult&)> Completion);

    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsApplicationFlowSubsystem> Flow = nullptr;

    FDelegateHandle FlowStateHandle;
    FDivineBeastsUIViewState ViewState;
    FDivineBeastsUIViewStateChangedNative StateChanged;

    /** 当前流程运行身份，用于识别新 Run 并重置 UI Revision。 */
    int64 LastFlowRunId = 0;
};
