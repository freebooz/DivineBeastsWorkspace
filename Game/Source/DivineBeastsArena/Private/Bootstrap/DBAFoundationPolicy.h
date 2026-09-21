#pragma once
#include <string_view>

namespace DBA::Foundation
{
/** 不依赖UE的入口决策，供真实协调器与原生边界回归共同使用。 */
enum class EMode { Disabled, PlayerDevelopment, ServerDiagnostics };

/** 开发构建且显式启用才能运行；Commandlet永不启动；专用服务器仅作世界日志诊断。 */
inline EMode ResolveMode(bool bDevelopmentBuild, bool bExplicitlyEnabled, bool bCommandlet, bool bDedicatedServer)
{
    if (!bDevelopmentBuild || !bExplicitlyEnabled || bCommandlet) { return EMode::Disabled; }
    return bDedicatedServer ? EMode::ServerDiagnostics : EMode::PlayerDevelopment;
}

/** 只接纳本实例、本次操作及精确目标包的就绪通知；字符串由调用者在本次调用期间持有。 */
inline bool AcceptsWorldReady(bool bSameInstance, bool bBegunPlay,
    std::string_view ActualPackage, std::string_view ExpectedPackage,
    std::string_view ActualOperation, std::string_view ExpectedOperation)
{
    return bSameInstance && bBegunPlay && !ExpectedPackage.empty() &&
        ActualPackage == ExpectedPackage && !ExpectedOperation.empty() && ActualOperation == ExpectedOperation;
}

/** 单调秒截止；即使世界尚未创建也检查，已启动的流程由自身节点截止时间管理。 */
inline bool StartupDeadlineExceeded(bool bStartAttempted, bool bStopping, double NowSeconds, double DeadlineSeconds)
{
    return !bStartAttempted && !bStopping && NowSeconds >= DeadlineSeconds;
}

/** 只有失败/取消时、目标及不可复用操作身份完全匹配，才可撤销尚未消费的项目切图请求。 */
inline bool OwnsPendingTravel(bool bAborting, std::string_view ActualPackage, std::string_view ExpectedPackage,
    std::string_view ActualOperation, std::string_view ExpectedOperation)
{
    return bAborting && !ExpectedPackage.empty() && ActualPackage == ExpectedPackage &&
        !ExpectedOperation.empty() && ActualOperation == ExpectedOperation;
}
}
