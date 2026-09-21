# 仅脚本测试使用：不加载UE，不创建资产，不证明游戏验收。
param([ValidateSet('Exit','Ready','Host','WrongRun','Hang','Echo','ReadyCrash','Fields','CommandLine','Server','HeldLog')][string]$Mode,
    [string]$LogPath, [string]$RunId, [int]$Code = 0, [string]$Value)
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
switch ($Mode) {
    'Exit' { exit $Code }
    'Echo' { [Console]::WriteLine($Value); exit 0 }
    'Ready' { [IO.File]::WriteAllText($LogPath, "LogFoundation: FoundationReady RunId=$RunId`n") }
    'Host' { [IO.File]::WriteAllText($LogPath, "LogFoundation: FoundationHostReady RunId=$RunId`n") }
    'WrongRun' { [IO.File]::WriteAllText($LogPath, "LogFoundation: FoundationReady RunId=00000000-0000-0000-0000-000000000001`n") }
    'ReadyCrash' { [IO.File]::WriteAllText($LogPath, "LogFoundation: FoundationReady RunId=$RunId`n"); exit 17 }
    'Fields' { [IO.File]::WriteAllText($LogPath, "[2026.09.21-01.02.03:001][  0]LogDBAFoundation: Display: FoundationReady RunId=$RunId Map=/Game/Development/Foundation/Maps/L_FoundationSandbox`n") }
    'CommandLine' { [IO.File]::WriteAllText($LogPath, "LogInit: Command Line: FoundationReady RunId=$RunId`n") }
    'Server' { [IO.File]::WriteAllText($LogPath, "LogDBAFoundation: Display: FoundationServerReady RunId=$RunId Map=/Game/Development/Foundation/Maps/L_FoundationBootstrap`n") }
    'HeldLog' {
        # 模拟UE持续持有日志写句柄；读者必须允许共享写入。
        $stream = [IO.File]::Open($LogPath,[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::ReadWrite)
        $writer = [IO.StreamWriter]::new($stream,[Text.UTF8Encoding]::new($false))
        $writer.WriteLine("LogFoundation: FoundationReady RunId=$RunId"); $writer.Flush()
    }
}
Start-Sleep -Seconds 30
