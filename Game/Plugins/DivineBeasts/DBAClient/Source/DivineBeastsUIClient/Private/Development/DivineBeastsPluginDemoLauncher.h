#pragma once

// SSLM项目客户端组合层：仅启动隔离开发进程，不修改本进程认证、命令行或应用流程。
// GameInstance持有启动句柄；页面退出只撤销UI订阅，不结束已启动的人工演示会话。
#include "Subsystems/GameInstanceSubsystem.h"
#include "DivineBeastsPluginDemoLauncher.generated.h"

/**
 * 仅游戏线程访问的启动身份与代次。异步等待不拥有UObject；完成时必须先通过此对象收敛一次终态。
 * GameInstance失活只撤销本地显示许可，不终止用户演示进程；旧完成不得影响之后的启动。
 */
class FDivineBeastsPluginDemoLaunchState
{
public:
    /** 原子式占有一个本地启动意图；已有活动请求时拒绝且不改变身份。仅游戏线程。 */
    bool TryBegin()
    {
        if (bInProgress) { return false; }
        bInProgress = true;
        LaunchId = FGuid::NewGuid();
        ++Generation;
        return true;
    }
    /** 只有当前身份/代次可完成一次；失败与正常结束使用相同所有权检查，不吞掉错误。 */
    bool TryComplete(const FGuid& CompletedLaunchId, uint64 CompletedGeneration)
    {
        if (!bInProgress || CompletedLaunchId != LaunchId || CompletedGeneration != Generation) { return false; }
        bInProgress = false;
        return true;
    }
    /** 失活撤销旧完成许可，异步工作自行等待并关闭其进程句柄，不再访问此对象。 */
    void Invalidate() { ++Generation; bInProgress = false; LaunchId.Invalidate(); }
    /** 仅投影本地命令状态，不查询进程或业务状态。 */
    bool IsBusy() const { return bInProgress; }
    /** 当前启动身份；仅供创建异步回调令牌。 */
    FGuid GetLaunchId() const { return LaunchId; }
    /** 当前生命周期代次；失活与新请求均推进。 */
    uint64 GetGeneration() const { return Generation; }
private:
    FGuid LaunchId;
    uint64 Generation = 0;
    bool bInProgress = false;
};

/** 所有方法与事件仅游戏线程；启动成功不等于门户Ready或插件验收通过。 */
UCLASS(Transient)
class UDivineBeastsPluginDemoLauncher final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    /** Shipping/Test/Commandlet/专服不创建；打包开发Client还需显式DBADevelopmentTools。 */
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    /** 撤销旧回调代次并清理UI订阅；等待线程最终关闭自身句柄，不终止用户演示。 */
    virtual void Deinitialize() override;
    /** 仅本地Standalone与真实源码工作空间可展示；打包Client仍只启动受控EditorGame。 */
    bool IsEntryAvailable() const;
    /** 已有本次启动检查或演示进程时不重复发起。 */
    bool IsBusy() const { return LaunchState.IsBusy(); }
    /** 同步发起隐藏PowerShell桥；真实构建证据及资源检查由唯一RunPluginDemo启动器执行。 */
    void RequestLaunch();
    /** 只含固定中文提示和RunId，不投影脚本异常、环境或凭据。 */
    const FText& GetStatusText() const { return StatusText; }
    /** 状态真正变化才广播，Widget不轮询登录/认证/网络状态。 */
    FSimpleMulticastDelegate OnStatusChanged;
private:
    /** 固定状态提示发布，不允许由脚本回传文本注入UI。 */
    void SetStatus(const FText& Text);
    /** 校验显式源码根/固定项目与脚本；Editor宿主可从当前工程推导，Cook包不得猜测源码根。 */
    bool TryGetWorkspace(FString& OutWorkspace) const;
    /** 只在游戏线程修改的请求身份；后台线程仅保存其值副本。 */
    FDivineBeastsPluginDemoLaunchState LaunchState;
    /** 固定无敏感信息提示，由开始/结束通知更新。 */
    FText StatusText;
};
