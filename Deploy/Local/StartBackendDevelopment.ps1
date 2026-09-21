[CmdletBinding()]
param(
    [ValidateRange(10, 600)]
    [int]$StartupTimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$composeFile = Join-Path $workspaceRoot 'Deploy\Docker\docker-compose.backend.local.yml'
$composeArguments = @('compose', '--project-directory', $workspaceRoot, '-f', $composeFile, 'up', '--detach', '--build', '--remove-orphans')

try {
    & docker info | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw 'Docker Engine 不可用。请先启动 Docker Desktop。'
    }

    & docker @composeArguments
    if ($LASTEXITCODE -ne 0) {
        throw '业务后端 Docker Compose 启动失败。'
    }

    $endpoints = @(
        'http://127.0.0.1:28080/health/ready',
        'http://127.0.0.1:8081/health/ready',
        'http://127.0.0.1:8082/health/ready',
        'http://127.0.0.1:8083/health/ready',
        'http://127.0.0.1:8084/health/ready'
    )
    $pendingEndpoints = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($endpoint in $endpoints) {
        [void]$pendingEndpoints.Add($endpoint)
    }
    $deadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)

    while ($pendingEndpoints.Count -gt 0 -and [DateTime]::UtcNow -lt $deadline) {
        foreach ($endpoint in @($pendingEndpoints)) {
            try {
                $response = Invoke-WebRequest -UseBasicParsing -Uri $endpoint -TimeoutSec 2
                if ($response.StatusCode -eq 200) {
                    [void]$pendingEndpoints.Remove($endpoint)
                }
            }
            catch {
                # 服务可能仍在首次构建或启动；超时前继续轮询。
            }
        }
        if ($pendingEndpoints.Count -gt 0) {
            Start-Sleep -Milliseconds 500
        }
    }

    if ($pendingEndpoints.Count -gt 0) {
        & docker compose --project-directory $workspaceRoot -f $composeFile logs --tail 200
        throw "业务后端未在 $StartupTimeoutSeconds 秒内就绪：$($pendingEndpoints -join ', ')"
    }

    Write-Output '业务后端本地开发部署完成，五个 HTTP 健康端点均已就绪。'
}
catch {
    Write-Error $_
    exit 1
}
