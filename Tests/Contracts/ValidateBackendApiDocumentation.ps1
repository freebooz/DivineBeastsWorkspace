[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$sharedContractsRoot = Join-Path $workspaceRoot 'Shared\Contracts'
$documentationRoot = Join-Path $workspaceRoot 'Docs\Backend'

$requiredSpecificationFiles = @(
    (Join-Path $sharedContractsRoot 'GamePlatform\OpenAPI\gateway.openapi.yaml'),
    (Join-Path $sharedContractsRoot 'GamePlatform\OpenAPI\identity.openapi.yaml'),
    (Join-Path $sharedContractsRoot 'GamePlatform\OpenAPI\player-data.openapi.yaml'),
    (Join-Path $sharedContractsRoot 'GamePlatform\OpenAPI\party.openapi.yaml'),
    (Join-Path $sharedContractsRoot 'GamePlatform\OpenAPI\matchmaking.openapi.yaml'),
    (Join-Path $sharedContractsRoot 'GamePlatform\OpenAPI\game-server-control.openapi.yaml')
)

foreach ($requiredFile in $requiredSpecificationFiles) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Missing backend API or service document: $requiredFile"
    }
}

$documentationFiles = @(Get-ChildItem -LiteralPath $documentationRoot -File -Filter '*.md')
if ($documentationFiles.Count -lt 3) {
    throw "Expected at least three backend documentation files in: $documentationRoot"
}

function Find-DocumentationFile {
    param([Parameter(Mandatory = $true)][string]$RequiredText)

    foreach ($documentationFile in $documentationFiles) {
        if ((Get-Content -Raw -LiteralPath $documentationFile.FullName) -match [regex]::Escape($RequiredText)) {
            return $documentationFile.FullName
        }
    }
    throw "Unable to find backend documentation containing: $RequiredText"
}

function Assert-OpenApiOperation {
    param(
        [Parameter(Mandatory = $true)][string]$SpecificationPath,
        [Parameter(Mandatory = $true)][string]$Method,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $content = Get-Content -Raw -LiteralPath $SpecificationPath
    $escapedPath = [regex]::Escape($Path)
    $escapedMethod = [regex]::Escape($Method.ToLowerInvariant())
    $pattern = "(?ms)^  ${escapedPath}:\r?\n(?:(?!^  /).)*?^    ${escapedMethod}:"
    if ($content -notmatch $pattern) {
        throw "OpenAPI does not cover runtime route: $Method $Path (specification: $SpecificationPath)"
    }
}

$routeSources = @(
    @{ Source = 'Backend\internal\app\gateway\api.go'; Specification = 'GamePlatform\OpenAPI\gateway.openapi.yaml' },
    @{ Source = 'Backend\internal\transport\httpadapter\identity_server.go'; Specification = 'GamePlatform\OpenAPI\identity.openapi.yaml' },
    @{ Source = 'Backend\internal\transport\httpadapter\playerdata_server.go'; Specification = 'GamePlatform\OpenAPI\player-data.openapi.yaml' },
    @{ Source = 'Backend\internal\transport\httpadapter\match_server.go'; Specification = 'GamePlatform\OpenAPI\party.openapi.yaml' },
    @{ Source = 'Backend\internal\transport\httpadapter\match_server.go'; Specification = 'GamePlatform\OpenAPI\matchmaking.openapi.yaml' },
    @{ Source = 'Backend\internal\transport\httpadapter\gameservercontrol_server.go'; Specification = 'GamePlatform\OpenAPI\game-server-control.openapi.yaml' }
)

foreach ($routeSource in $routeSources) {
    $sourcePath = Join-Path $workspaceRoot $routeSource.Source
    $specificationPath = Join-Path $sharedContractsRoot $routeSource.Specification
    $sourceContent = Get-Content -Raw -LiteralPath $sourcePath
    $matches = [regex]::Matches($sourceContent, '(?:handler\.)?mux\.HandleFunc\("(?<method>GET|POST) (?<path>/[^"]+)"')
    foreach ($match in $matches) {
        $routePath = $match.Groups['path'].Value
        $method = $match.Groups['method'].Value
        $isPartyRoute = $routePath -eq '/internal/v1/match/party'
        $isMatchmakingRoute = $routePath -eq '/internal/v1/match/tickets'
        if (($routeSource.Specification -like '*party.openapi.yaml' -and -not $isPartyRoute) -or
            ($routeSource.Specification -like '*matchmaking.openapi.yaml' -and -not $isMatchmakingRoute)) {
            continue
        }
        Assert-OpenApiOperation -SpecificationPath $specificationPath -Method $method -Path $routePath
    }
}

$serviceSpecifications = @(
    'GamePlatform\OpenAPI\gateway.openapi.yaml',
    'GamePlatform\OpenAPI\identity.openapi.yaml',
    'GamePlatform\OpenAPI\player-data.openapi.yaml',
    'GamePlatform\OpenAPI\party.openapi.yaml',
    'GamePlatform\OpenAPI\matchmaking.openapi.yaml',
    'GamePlatform\OpenAPI\game-server-control.openapi.yaml'
)
foreach ($relativeSpecificationPath in $serviceSpecifications) {
    $specificationPath = Join-Path $sharedContractsRoot $relativeSpecificationPath
    foreach ($operation in @(
        @{ Method = 'GET'; Path = '/health/live' },
        @{ Method = 'GET'; Path = '/health/ready' },
        @{ Method = 'GET'; Path = '/version' }
    )) {
        Assert-OpenApiOperation -SpecificationPath $specificationPath -Method $operation.Method -Path $operation.Path
    }
}

$swaggerGuide = Get-Content -Raw -LiteralPath (Find-DocumentationFile -RequiredText 'http://127.0.0.1:28080/swagger/')
foreach ($requiredText in @('http://127.0.0.1:28080/swagger/', 'GameServerControlService')) {
    if ($swaggerGuide -notmatch [regex]::Escape($requiredText)) {
        throw "Swagger guide is missing required content: $requiredText"
    }
}

$messageBusGuide = Get-Content -Raw -LiteralPath (Find-DocumentationFile -RequiredText 'WithMsgID')
foreach ($requiredText in @('Match.Completed', 'NATS JetStream', 'Outbox', 'idempotency')) {
    if ($messageBusGuide -notmatch [regex]::Escape($requiredText)) {
        throw "Message bus guide is missing required content: $requiredText"
    }
}

$serviceGuide = Get-Content -Raw -LiteralPath (Find-DocumentationFile -RequiredText 'StopBackendDevelopment')
foreach ($requiredText in @('GatewayService', 'IdentityService', 'PlayerDataService', 'MatchService', 'GameServerControlService', 'PostgreSQL', 'Redis', 'NATS', 'Agones')) {
    if ($serviceGuide -notmatch [regex]::Escape($requiredText)) {
        throw "Service guide is missing required content: $requiredText"
    }
}

Write-Output 'Backend Swagger, message bus, and service documentation contract validation passed.'
