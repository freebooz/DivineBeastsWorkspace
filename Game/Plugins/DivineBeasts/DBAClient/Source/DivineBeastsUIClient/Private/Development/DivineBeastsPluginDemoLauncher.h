#pragma once

// SSLM项目客户端组合层：仅启动隔离开发进程，不修改本进程认证、命令行或应用流程。
// GameInstance持有启动句柄；页面退出只撤销UI订阅，不结束已启动的人工演示会话。
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProcess.h"
#include "DivineBeastsPluginDemoLauncher.generated.h"

/** 所有方法与事件仅游戏线程；启动成功不等于门户Ready或插件验收通过。 */
UCLASS(Transient)
class UDivineBeastsPluginDemoLauncher final : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    /** Shipping/Test/Commandlet/专服不创建；打包Client尚无启动证据支持，也不开放。 */
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    /** 仅关闭本进程句柄及维护Ticker，不终止子进程或其他编辑器。 */
    virtual void Deinitialize() override;
    /** 宿主为本地非联网EditorGame/PIE且存在工作空间启动器时可展示。 */
    bool IsEntryAvailable() const;
    /** 已有本次启动检查或演示进程时不重复发起。 */
    bool IsBusy() const { return Process.IsValid(); }
    /** 同步发起隐藏PowerShell桥；真实构建证据及资源检查由唯一RunPluginDemo启动器执行。 */
    void RequestLaunch();
    /** 只含固定中文提示和RunId，不投影脚本异常、环境或凭据。 */
    const FText& GetStatusText() const { return StatusText; }
    /** 状态真正变化才广播，Widget不轮询登录/认证/网络状态。 */
    FSimpleMulticastDelegate OnStatusChanged;
private:
    /** 仅活动子进程每0.5秒维护一次；退出后立即移除，非全局常驻Tick。 */
    bool PollProcess(float DeltaSeconds);
    /** 固定状态提示发布，不允许由脚本回传文本注入UI。 */
    void SetStatus(const FText& Text);
    /** 规范化工作空间固定脚本路径，不接受按钮提供命令/地图/可执行程序。 */
    FString GetScriptPath() const;
    FProcHandle Process;
    FTSTicker::FDelegateHandle PollHandle;
    FGuid LaunchId;
    FString StatusFile;
    FString LastStatus;
    FText StatusText;
};
