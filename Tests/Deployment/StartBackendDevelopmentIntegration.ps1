[CmdletBinding()]
param(
    [ValidateRange(10, 600)]
    [int]$StartupTimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$startScript = Join-Path $workspaceRoot 'Deploy\Local\StartBackendDevelopment.ps1'

& powershell -NoProfile -ExecutionPolicy Bypass -File $startScript -StartupTimeoutSeconds $StartupTimeoutSeconds
if ($LASTEXITCODE -ne 0) {
    throw '后端一键启动脚本返回失败。'
}

foreach ($endpoint in @(
    'http://127.0.0.1:28080/health/ready',
    'http://127.0.0.1:8081/health/ready',
    'http://127.0.0.1:8082/health/ready',
    'http://127.0.0.1:8083/health/ready',
    'http://127.0.0.1:8084/health/ready'
)) {
    $response = Invoke-WebRequest -UseBasicParsing -Uri $endpoint -TimeoutSec 5
    if ($response.StatusCode -ne 200) {
        throw "服务就绪端点返回非成功状态：$endpoint"
    }
}

Write-Output '业务后端一键启动集成校验通过。'
