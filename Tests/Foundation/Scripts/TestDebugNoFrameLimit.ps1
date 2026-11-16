# 使用不存在的可执行文件，验证参数在任何进程启动前拒绝正数限帧。
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$launcher=Join-Path $root 'Build/Game/RunFrontEndClients.ps1'
$badExe=Join-Path $root 'Saved/NoSuchDebugClient.exe'
$positive=& (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File $launcher -ClientExe $badExe -GatewayUrl 'http://127.0.0.1:28181' -FrameRateLimit 60 2>&1 | Out-String
if($LASTEXITCODE -eq 0 -or $positive -notmatch "parameter 'FrameRateLimit'"){throw '调试启动器没有在参数校验阶段拒绝限帧'}
$unlimited=& (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File $launcher -ClientExe $badExe -GatewayUrl 'http://127.0.0.1:28181' -FrameRateLimit 0 2>&1 | Out-String
if($LASTEXITCODE -eq 0 -or $unlimited -match "parameter 'FrameRateLimit'"){throw '不限帧参数应通过校验并继续检查可执行文件'}
'DEBUG_FRAME_LIMIT_POLICY_PASS'
