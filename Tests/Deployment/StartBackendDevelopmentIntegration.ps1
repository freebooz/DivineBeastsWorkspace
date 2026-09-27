[CmdletBinding()]
param(
    [ValidateRange(10, 600)]
    [int]$StartupTimeoutSeconds = 120,
    [ValidateRange(1024, 65535)]
    [int]$GatewayHostPort = 28080,
    [ValidateRange(1024, 65535)]
    [int]$IdentityHostPort = 8081,
    [ValidateRange(1024, 65535)]
    [int]$PlayerDataHostPort = 8082,
    [ValidateRange(1024, 65535)]
    [int]$MatchHostPort = 8083,
    [ValidateRange(1024, 65535)]
    [int]$GameServerControlHostPort = 8084
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$startScript = Join-Path $workspaceRoot 'Deploy\Local\StartBackendDevelopment.ps1'

& powershell -NoProfile -ExecutionPolicy Bypass -File $startScript `
    -StartupTimeoutSeconds $StartupTimeoutSeconds `
    -GatewayHostPort $GatewayHostPort `
    -IdentityHostPort $IdentityHostPort `
    -PlayerDataHostPort $PlayerDataHostPort `
    -MatchHostPort $MatchHostPort `
    -GameServerControlHostPort $GameServerControlHostPort
if ($LASTEXITCODE -ne 0) {
    throw '后端一键启动脚本返回失败。'
}

foreach ($endpoint in @(
    "http://127.0.0.1:$GatewayHostPort/health/ready",
    "http://127.0.0.1:$IdentityHostPort/health/ready",
    "http://127.0.0.1:$PlayerDataHostPort/health/ready",
    "http://127.0.0.1:$MatchHostPort/health/ready",
    "http://127.0.0.1:$GameServerControlHostPort/health/ready",
    "http://127.0.0.1:$GatewayHostPort/swagger/"
)) {
    $response = Invoke-WebRequest -UseBasicParsing -Uri $endpoint -TimeoutSec 5
    if ($response.StatusCode -ne 200) {
        throw "服务就绪端点返回非成功状态：$endpoint"
    }
}

Write-Output '业务后端一键启动与 Swagger 文档入口集成校验通过。'
