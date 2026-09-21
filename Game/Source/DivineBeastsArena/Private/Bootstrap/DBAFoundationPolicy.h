#pragma once

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
}
