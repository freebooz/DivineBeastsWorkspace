[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$composeFile = Join-Path $workspaceRoot 'Deploy\Docker\docker-compose.backend.local.yml'
$dockerfile = Join-Path $workspaceRoot 'Deploy\Docker\Backend.Dockerfile'
$startScript = Join-Path $workspaceRoot 'Deploy\Local\StartBackendDevelopment.ps1'
$stopScript = Join-Path $workspaceRoot 'Deploy\Local\StopBackendDevelopment.ps1'

foreach ($requiredPath in @($composeFile, $dockerfile, $startScript, $stopScript)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "缺少业务后端本地部署文件：$requiredPath"
    }
}

$composeJson = & docker compose --project-directory $workspaceRoot -f $composeFile config --format json
if ($LASTEXITCODE -ne 0) {
    throw '业务后端 Docker Compose 配置无法解析。'
}

$composeConfiguration = $composeJson | ConvertFrom-Json
$expectedServices = @('gatewayservice', 'identityservice', 'playerdataservice', 'matchservice', 'gameservercontrolservice')
$actualServices = @($composeConfiguration.services.PSObject.Properties.Name)

foreach ($expectedService in $expectedServices) {
    if ($actualServices -notcontains $expectedService) {
        throw "Docker Compose 缺少服务：$expectedService"
    }
}

foreach ($expectedService in $expectedServices) {
    $service = $composeConfiguration.services.$expectedService
    if ($null -eq $service.build -or [string]::IsNullOrWhiteSpace([string]$service.build.dockerfile)) {
        throw "服务未声明 Docker 构建定义：$expectedService"
    }
    $resolvedBuildContext = [System.IO.Path]::GetFullPath([string]$service.build.context)
    if ($resolvedBuildContext -ne $workspaceRoot) {
        throw "服务 Docker 构建上下文必须是工作空间根目录：$expectedService"
    }
    if ($null -eq $service.ports -or $service.ports.Count -lt 1) {
        throw "服务未声明本地 HTTP 端口：$expectedService"
    }
}

$gatewayHttpPort = @($composeConfiguration.services.gatewayservice.ports | Where-Object { [string]$_.target -eq '8080' })
if ($gatewayHttpPort.Count -ne 1 -or [string]$gatewayHttpPort[0].published -ne '28080') {
    throw 'GatewayService 必须将主机端口 28080 映射到容器端口 8080。'
}

$gatewayEnvironment = $composeConfiguration.services.gatewayservice.environment
$expectedGatewayEndpoints = @{
    IDENTITY_SERVICE_URL = 'http://identityservice:8081'
    PLAYER_DATA_SERVICE_URL = 'http://playerdataservice:8082'
    MATCH_SERVICE_URL = 'http://matchservice:8083'
}
foreach ($environmentName in $expectedGatewayEndpoints.Keys) {
    $actualValue = $gatewayEnvironment.PSObject.Properties[$environmentName].Value
    if ([string]$actualValue -ne $expectedGatewayEndpoints[$environmentName]) {
        throw "GatewayService 缺少内部服务地址：$environmentName"
    }
}

$swaggerContractsRootProperty = $gatewayEnvironment.PSObject.Properties['DIVINEBEASTS_SWAGGER_CONTRACTS_ROOT']
if ($null -eq $swaggerContractsRootProperty -or [string]$swaggerContractsRootProperty.Value -ne '/contracts') {
    throw 'GatewayService 必须把共享契约挂载根目录配置为 /contracts。'
}

$swaggerVolume = @($composeConfiguration.services.gatewayservice.volumes | Where-Object { [string]$_.target -eq '/contracts' })
if ($swaggerVolume.Count -ne 1 -or -not [bool]$swaggerVolume[0].read_only) {
    throw 'GatewayService 必须以只读方式挂载共享契约目录到 /contracts。'
}
$expectedContractsDirectory = Join-Path $workspaceRoot 'Shared\Contracts'
$actualContractsDirectory = [System.IO.Path]::GetFullPath([string]$swaggerVolume[0].source)
if ($actualContractsDirectory -ne $expectedContractsDirectory) {
    throw "GatewayService Swagger 契约挂载源不正确：$actualContractsDirectory"
}

Write-Output '业务后端本地部署配置校验通过。'
