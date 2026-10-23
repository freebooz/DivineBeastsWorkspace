#requires -Version 7.0
<#
.SYNOPSIS
启动可见前端客户端，首屏保留用户登录表单，不自动认证或进入角色页。
.DESCRIPTION
输入已验证客户端可执行文件、窗口数量及本地后端地址；只启动新进程，不终止其他客户端。
普通启动清除开发凭据；显式开发工具可向登录页提供十账号按钮和隔离插件门户入口。
十账号种子只从父进程环境校验并传给本次子进程，不进入命令行、日志或运行JSON；点击后才真实登录。
输出Saved下独占运行证据和准确进程身份；启动成功不代表登录、角色选择或Village准入通过。
#>
param([Parameter(Mandatory)][string]$ClientExe,[ValidateRange(1,4)][int]$Count=2,
    # 用户放大后的角色创建面板至少需要1920×1080工作区；仅调整窗口，不缩放文字/输入。
    # 保留960×540可显式选择用于登录小窗口回归，但不代表大型角色页完整可见。
    [ValidateRange(960,3840)][int]$WindowWidth=1920,
    [ValidateRange(540,2160)][int]$WindowHeight=1080,
    # 性能监视默认不限帧；0显式清除本次进程的t.MaxFPS限制，正数仅供调用方主动限帧，不写玩家偏好。
    [ValidateRange(0,240)][int]$FrameRateLimit=0,
    # 开发联调默认显示根布局左上角FPS；可用-ShowFPS:$false关闭，独立于限帧且不写玩家设置。
    [switch]$ShowFPS=$true,
    # 只记录运动/复制的事件采样，供人工移动后按三端时间戳定位延迟，不包含登录信息。
    [switch]$MovementDiagnostics,
    # 仅显式开发验证加载已批准候选技能；三个参数共同选择文本INI、开发配置及运行开关。
    # 首屏仍为人工登录，不向进程传账号或密码；默认false保持正式前端流程。
    [switch]$DevelopmentSkills,
    # 怪物仅在已启用开发技能的人工验证配置内创建，仍须独立运行开关，默认禁止正式流程生成。
    [switch]$DevelopmentMonsters,
    # 真实组队竞技开发入口；必须使用同一开发技能/内容Cook，首屏仍为人工登录。
    [switch]$DevelopmentArena,
    # 非Shipping联调可显式读取当前源码默认输入；使用UE原生DEFInputINI合同，避免旧Cook包遮盖已修正映射。
    [switch]$UseWorkspaceInputConfig,
    # 显式允许本地Development Client启动既有EditorGame插件门户；不能作为Cook包含全部演示的证明。
    [switch]$DevelopmentTools,
    # 必须指向当前完整Editor构建结果；点击时仍由唯一RunPluginDemo门禁重新核对源码与产物。
    [string]$DevelopmentBuildResult,
    # 真实锁定UE引擎根，不能从打包Client内嵌的Engine目录推断编辑器位置。
    [string]$EngineRoot,
    # 只开放点击登录按钮；不设置DBADevAutoLogin，也不将口令写入Widget默认值。
    [switch]$DevelopmentTestAccounts,
    # 历史28081与28181拥有独立身份和玩家资料；当前本地保留28181，调用方仍须明确选择目标网关。
    [string]$GatewayUrl,
    [guid]$RunId=[guid]::NewGuid())
$ErrorActionPreference='Stop'

function Resolve-FrontEndDevelopmentTestAccounts {
    <#
    仅构建工具层使用的十账号输入校验。Enabled=false时返回空配置；启用时要求完整、唯一的十个账号。
    输入JSON只来自当前进程种子环境；返回口令仍属瞬态秘密，调用方只能传给本次子进程环境。
    所有失败使用固定说明，禁止将JSON解析异常、账号口令或原文拼入错误、日志和证据。
    #>
    param([switch]$Enabled,[AllowNull()][string]$AccountsJson)
    if (!$Enabled) { return [pscustomobject]@{AccountsJson='';AccountNames=@()} }
    if ([string]::IsNullOrWhiteSpace($AccountsJson) -or $AccountsJson.Length -gt 32768) {
        throw '开发测试账号需要父进程提供完整十账号种子配置；未启动客户端。'
    }
    try { $accounts=@(ConvertFrom-Json -InputObject $AccountsJson -AsHashtable -ErrorAction Stop) }
    catch { throw '开发测试账号JSON无效；未启动客户端。' }
    if ($accounts.Count -ne 10) { throw '开发测试账号必须恰好十个；未启动客户端。' }
    $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $normalized=@()
    foreach($account in $accounts) {
        if ($account -isnot [Collections.IDictionary] -or $account.Count -ne 2 -or
            !$account.Contains('accountName') -or !$account.Contains('password') -or
            $account.accountName -isnot [string] -or $account.password -isnot [string] -or
            [string]::IsNullOrWhiteSpace($account.accountName) -or $account.accountName.Length -gt 128 -or
            $account.accountName -cne $account.accountName.Trim() -or
            [string]::IsNullOrWhiteSpace($account.password) -or $account.password.Length -gt 72 -or
            !$names.Add($account.accountName)) {
            throw '开发测试账号存在重复、空值或非法字段；未启动客户端。'
        }
        $normalized+=@{accountName=$account.accountName;password=$account.password}
    }
    return [pscustomobject]@{AccountsJson=(ConvertTo-Json -InputObject $normalized -Compress);AccountNames=@($normalized|ForEach-Object{$_.accountName})}
}

function Set-FrontEndDevelopmentTestAccountEnvironment {
    <#
    为一个尚未启动的ProcessStartInfo清理认证环境，再按显式开关注入已校验专用JSON。
    不改变父进程环境，不打印或持久化凭据；普通启动也会清除可能由宿主继承的旧值。
    #>
    param([Parameter(Mandatory)][Diagnostics.ProcessStartInfo]$StartInfo,[Parameter(Mandatory)]$Configuration)
    foreach($key in @('DBA_DEV_LOGIN_SECRET','DIVINEBEASTS_DEV_LOGIN_USER','DIVINEBEASTS_DEV_LOGIN_PASSWORD','DIVINEBEASTS_DEV_LOGIN_ACCOUNTS_JSON','DBA_DEV_TEST_LOGIN_ACCOUNTS_JSON')) {
        $null=$StartInfo.Environment.Remove($key)
    }
    if ($Configuration.AccountsJson) { $StartInfo.Environment['DBA_DEV_TEST_LOGIN_ACCOUNTS_JSON']=$Configuration.AccountsJson }
}

if ($DevelopmentArena -and !$DevelopmentSkills) { throw '开发竞技必须使用包含必要英雄定义和技能的DevelopmentSkills配置。' }
if ($DevelopmentMonsters -and !$DevelopmentSkills) { throw '开发怪物验证必须同时显式启用DevelopmentSkills。' }
$gatewayModulePath=Join-Path $PSScriptRoot 'FrontEndClientGateway.psm1'
Import-Module $gatewayModulePath -Force
# 在创建任何证据目录或启动客户端前拒绝缺失/非法网关，防止把用户引导到错误的角色档案环境。
$GatewayUrl=Resolve-FrontEndClientGatewayUrl -GatewayUrl $GatewayUrl
$gatewayHttpArguments=@(Get-FrontEndClientHttpArguments -GatewayUrl $GatewayUrl)
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$accountConfiguration=Resolve-FrontEndDevelopmentTestAccounts -Enabled:$DevelopmentTestAccounts -AccountsJson ([Environment]::GetEnvironmentVariable('DIVINEBEASTS_DEV_LOGIN_ACCOUNTS_JSON','Process'))
$developmentToolArguments=@()
if ($DevelopmentTools) {
    # 此处只确认调用方明确选择真实本机路径；完整输入/产物绑定仍由点击后的既有启动门禁负责。
    if ([string]::IsNullOrWhiteSpace($EngineRoot)) { throw 'DevelopmentTools必须明确提供EngineRoot；普通启动不需要此参数。' }
    if ([string]::IsNullOrWhiteSpace($DevelopmentBuildResult) -or !(Test-Path -LiteralPath $DevelopmentBuildResult -PathType Leaf)) {
        throw 'DevelopmentTools必须明确提供存在的完整Editor构建结果DevelopmentBuildResult。'
    }
    Import-Module (Join-Path $PSScriptRoot 'FoundationTools.psm1') -Force
    $developmentEngine=Get-FoundationEngine $EngineRoot
    $developmentPowerShell=Join-Path $PSHOME 'pwsh.exe'
    if (!(Test-Path -LiteralPath $developmentPowerShell -PathType Leaf)) { throw 'DevelopmentTools需要当前PowerShell 7可执行文件。' }
    $developmentToolArguments=@('-DBADevelopmentTools',"-DBADevelopmentWorkspace=$workspace",
        "-DBADevelopmentEngineRoot=$($developmentEngine.Root)",
        ('-DBADevelopmentBuildResult='+[IO.Path]::GetFullPath($DevelopmentBuildResult)),
        "-DBADevelopmentPowerShell=$developmentPowerShell")
}
$exe=(Resolve-Path -LiteralPath $ClientExe).Path
if ([IO.Path]::GetFileName($exe)-ne'DivineBeastsArenaClient.exe') { throw '必须指定正式Client Target产物。' }
$directory=Join-Path $workspace "Saved/Validation/FrontEndClients/$RunId"
if (Test-Path -LiteralPath $directory) { throw '拒绝复用本次启动证据目录。' }
$null=New-Item -ItemType Directory -Path $directory
$records=@()
foreach($i in 1..$Count) {
    $info=[Diagnostics.ProcessStartInfo]::new()
    $info.FileName=$exe; $info.WorkingDirectory=Split-Path $exe
    $info.UseShellExecute=$false; $info.CreateNoWindow=$false; $info.WindowStyle=[Diagnostics.ProcessWindowStyle]::Normal
    Set-FrontEndDevelopmentTestAccountEnvironment -StartInfo $info -Configuration $accountConfiguration
    $info.Environment['DIVINEBEASTS_GATEWAY_BASE_URL']=$GatewayUrl
    # 只对本机开发网关直连，保留其它HTTP请求的系统代理；参数来自解析后的网关身份。
    foreach($argument in $gatewayHttpArguments) { $info.ArgumentList.Add($argument) }
    $log=Join-Path $directory "Client$i.log"
    foreach($argument in @('/DBAFrontEndPack/Maps/L_DBA_FrontEnd','-windowed',"-ResX=$WindowWidth","-ResY=$WindowHeight","-WinX=$((($i-1)*40)+20)",'-WinY=40','-nosplash',"-abslog=$log")) {
        $info.ArgumentList.Add($argument)
    }
    # FPS由平台根布局消费帧结束通知；原生stat fps固定右侧和彩色，不满足项目视觉合同。
    $startupCommands=@("t.MaxFPS $FrameRateLimit")
    if ($ShowFPS) { $info.ArgumentList.Add('-GamePlatformShowFPS') }
    if ($startupCommands.Count) { $info.ArgumentList.Add('-ExecCmds='+($startupCommands -join ',')) }
    if ($MovementDiagnostics) { $info.ArgumentList.Add('-DBAMovementDiagnostics') }
    if ($DevelopmentSkills) {
        foreach ($argument in @('-CustomConfig=VillageDevelopmentClient','-DBADevelopmentSkills','-textconfig','-dumpiniloads')) {
            $info.ArgumentList.Add($argument)
        }
    }
    if ($DevelopmentMonsters) { $info.ArgumentList.Add('-DBADevelopmentMonsters') }
    if ($DevelopmentArena) { $info.ArgumentList.Add('-DBADevelopmentArena') }
    if ($UseWorkspaceInputConfig) { $info.ArgumentList.Add('-DEFInputINI='+(Join-Path $workspace 'Game/Config/DefaultInput.ini')) }
    foreach($argument in $developmentToolArguments) { $info.ArgumentList.Add($argument) }
    if ($DevelopmentTestAccounts) { $info.ArgumentList.Add('-DBADevelopmentTestAccounts') }
    $process=[Diagnostics.Process]::Start($info)
    $records+=@{PID=$process.Id;Exe=$exe;StartUtcTicks=$process.StartTime.ToUniversalTime().Ticks;Log=$log;GatewayUrl=$GatewayUrl;InitialScreen='Login';AutoLogin=$false;FrameRateLimit=$FrameRateLimit;ShowFPS=[bool]$ShowFPS;WindowWidth=$WindowWidth;WindowHeight=$WindowHeight;MovementDiagnostics=[bool]$MovementDiagnostics;DevelopmentSkills=[bool]$DevelopmentSkills;DevelopmentMonsters=[bool]$DevelopmentMonsters;DevelopmentArena=[bool]$DevelopmentArena;UseWorkspaceInputConfig=[bool]$UseWorkspaceInputConfig;DevelopmentTools=[bool]$DevelopmentTools;DevelopmentTestAccounts=[bool]$DevelopmentTestAccounts;DevelopmentTestAccountNames=$accountConfiguration.AccountNames}
}
$records|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $directory 'Clients.json') -Encoding utf8
$records|Select-Object PID,InitialScreen,AutoLogin|ConvertTo-Json
Write-Host "Evidence: $directory"
