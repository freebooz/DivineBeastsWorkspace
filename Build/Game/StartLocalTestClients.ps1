#requires -Version 7.0
<#
.SYNOPSIS
启动带十账号快捷登录按钮的本地SSLM验证客户端，仍由用户点击登录。
.DESCRIPTION
复用既有RunFrontEndClients；核对同一工作空间的Compose身份及显式网关端口后，
仅从正在运行的identityservice取得已有账号种子，通过当前进程/子进程环境传递。
不打印或落盘口令、不重建后端、不重置档案、不自动认证；普通发行入口保持原默认。
调用方须先完成Client/Server构建、Cook/Stage及战斗版本门禁。本工具只修复验证启动配置。
从后台启动本脚本时将stdout/stderr重定向至本次Saved证据文件，防止游戏日志刷屏。
#>
param([Parameter(Mandatory)][string]$ClientExe,
    [Parameter(Mandatory)][string]$GatewayUrl,
    [string]$ComposeProject='sslm-validation-8238653a',
    [ValidateRange(1,4)][int]$Count=2,
    [guid]$RunId=[guid]::NewGuid())
$ErrorActionPreference='Stop'
$taskRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $PSScriptRoot 'FrontEndClientGateway.psm1') -Force
$taskGateway=[uri](Resolve-FrontEndClientGatewayUrl -GatewayUrl $GatewayUrl)
if(!$taskGateway.IsLoopback){throw '测试账号验证启动器只允许本机网关'}

function Read-LocalValidationContainer([string]$Service){
    <# 必须是当前Compose项目、当前工作空间的唯一运行服务，避免读取另一套账号环境。 #>
    # 只读运行容器标签，不重新解析Compose文件；后者会要求与此查询无关的控制令牌插值。
    $taskIds=@(docker ps --filter "label=com.docker.compose.project=$ComposeProject" --filter "label=com.docker.compose.service=$Service" --format '{{.ID}}')
    if($LASTEXITCODE-ne0-or$taskIds.Count-ne1-or[string]::IsNullOrWhiteSpace($taskIds[0])){throw '本地验证服务未唯一运行'}
    try{$taskContainer=@(docker inspect $taskIds[0]|ConvertFrom-Json)[0]}
    catch{throw '无法读取本地验证容器配置'}
    if($LASTEXITCODE-ne0-or!$taskContainer.State.Running-or
        $taskContainer.Config.Labels.'com.docker.compose.project'-cne$ComposeProject-or
        [IO.Path]::GetFullPath($taskContainer.Config.Labels.'com.docker.compose.project.working_dir')-ine$taskRoot){throw '本地验证容器身份不匹配'}
    return $taskContainer
}

$taskIdentity=$null;$taskSeed=$null
$taskPrevious=[Environment]::GetEnvironmentVariable('DIVINEBEASTS_DEV_LOGIN_ACCOUNTS_JSON','Process')
try{
    $taskGatewayContainer=Read-LocalValidationContainer 'gatewayservice'
    $taskPorts=@($taskGatewayContainer.HostConfig.PortBindings.'8080/tcp')
    if($taskPorts.Count-ne1-or[int]$taskPorts[0].HostPort-ne$taskGateway.Port){throw '显式网关不属于当前账号环境'}
    $taskIdentity=Read-LocalValidationContainer 'identityservice'
    $taskValues=@($taskIdentity.Config.Env|Where-Object {$_.StartsWith('DIVINEBEASTS_DEV_LOGIN_ACCOUNTS_JSON=')})
    if($taskValues.Count-ne1){throw '现有身份服务缺少完整测试账号种子'}
    $taskSeed=$taskValues[0].Substring('DIVINEBEASTS_DEV_LOGIN_ACCOUNTS_JSON='.Length)
    try{$taskAccounts=@(ConvertFrom-Json -InputObject $taskSeed -AsHashtable)}
    catch{throw '测试账号种子格式无效'}
    if($taskAccounts.Count-ne10){throw '必须提供既有完整十账号配置'}
    [Environment]::SetEnvironmentVariable('DIVINEBEASTS_DEV_LOGIN_ACCOUNTS_JSON',$taskSeed,'Process')
    # 账号配置只传当前新客户端；实际启动器继续做字段、唯一性、口令非空的完整校验。
    # 同时显式启用测试账号开关，普通启动器会清理旧凭据，不能只设置父环境而漏掉此开关。
    & (Join-Path $PSScriptRoot 'RunFrontEndClients.ps1') -ClientExe $ClientExe -Count $Count -GatewayUrl $GatewayUrl `
        -WindowWidth 1920 -WindowHeight 1080 -FrameRateLimit 60 -MovementDiagnostics `
        -DevelopmentSkills -DevelopmentArena -DevelopmentMonsters -DevelopmentTestAccounts -RunId $RunId
}finally{
    [Environment]::SetEnvironmentVariable('DIVINEBEASTS_DEV_LOGIN_ACCOUNTS_JSON',$taskPrevious,'Process')
    $taskSeed=$null;$taskIdentity=$null;$taskAccounts=$null;$taskValues=$null
}
