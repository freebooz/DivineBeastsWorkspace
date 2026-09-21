[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$composeFile = Join-Path $workspaceRoot 'Deploy\Docker\docker-compose.backend.local.yml'

& docker compose --project-directory $workspaceRoot -f $composeFile down --remove-orphans
if ($LASTEXITCODE -ne 0) {
    throw '业务后端 Docker Compose 停止失败。'
}

Write-Output '业务后端本地开发部署已停止；未删除任何卷或镜像。'
