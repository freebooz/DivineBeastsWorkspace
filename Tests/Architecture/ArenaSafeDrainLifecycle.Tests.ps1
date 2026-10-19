#requires -Version 7.0
# 本文件为只读跨模块源码装配门禁：校验竞技结算必须先获得服务器权威结果与零人成功回执。
# 它不会加载引擎、发出Agones SDK请求或冒充UE原生测试；真实网络和竞赛退出还需独立验收。
Describe 'MainArena赛后安全回收合同' {
    $root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
    $arena = Join-Path $root 'Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArenaServer/Private/Server/GamePlatformArenaServerSubsystem.cpp'
    $platform = Join-Path $root 'Game/Plugins/GamePlatform/OnlineServices/GamePlatformServer/Source/GamePlatformServer/Private/Server/GamePlatformServerLifecycleSubsystem.cpp'
    $agones = Join-Path $root 'Game/Plugins/GamePlatform/OnlineServices/GamePlatformServer/Source/GamePlatformServer/Private/Agones/GamePlatformAgonesBridgeSubsystem.cpp'
    $arenaText = [IO.File]::ReadAllText($arena)
    $platformText = [IO.File]::ReadAllText($platform)
    $agonesText = [IO.File]::ReadAllText($agones)

    It '必须由后端结果真正提交且竞技权威Completed才允许进入结束流程' {
        $arenaText | Should Match 'SubmitPendingResult'
        $arenaText | Should Match 'bResultCommitted = true'
        $arenaText | Should Match 'EGamePlatformArenaMatchPhase::Completed'
        $arenaText | Should Match 'CanRequestSafeDrain'
    }

    It '主竞技服务器不得在仍有真实PlayerController时请求排空' {
        $arenaText | Should Match 'GetPlayerControllerIterator'
        $arenaText | Should Match 'bHasLiveControllers = true'
        $arenaText | Should Match '!bHasLivePlayerControllers'
        $arenaText | Should Match 'StopAcceptingAdmissions'
    }

    It '零人心跳必须在结果后发起且由平台明确成功回执确认' {
        $arenaText | Should Match 'GetIssuedOperationGeneration'
        $arenaText | Should Match 'GetLastAcknowledgedHeartbeatGeneration'
        $arenaText | Should Match 'GetLastAcknowledgedHeartbeatPlayers'
        $arenaText | Should Match 'SendHeartbeat\(0\)'
        $platformText | Should Match 'if \(SubmittedPlayers >= 0\)'
        $platformText | Should Match 'if \(!bSucceeded\)'
    }

    It 'Agones只能在平台Drain完成进入Stopped状态后终止实例' {
        $arenaText | Should Match 'Lifecycle->BeginDrain\(\)'
        $arenaText | Should Match 'Lifecycle->CompleteDrain\(\)'
        $agonesText | Should Match 'Snapshot.State == EGamePlatformServerLifecycleState::Stopped'
        $agonesText | Should Match 'bShutdownRequested = true'
    }

    It '不同层级各有独立原生回归且退出清理定时器与通知' {
        (Test-Path (Join-Path $root 'Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArenaServer/Private/Tests/GamePlatformArenaSafeDrainGateTests.cpp')) | Should Be $true
        (Test-Path (Join-Path $root 'Game/Plugins/GamePlatform/OnlineServices/GamePlatformServer/Source/GamePlatformServer/Private/Tests/ServerHeartbeatAckFenceTests.cpp')) | Should Be $true
        $arenaText | Should Match 'ClearTimer\(SafeDrainTimer\)'
        $arenaText | Should Match 'OnResultPending\(\).Remove\(ResultPendingHandle\)'
    }
}
