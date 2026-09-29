param(
    # StaticOnly（仅静态）跳过UE构建、Automation和资产检查，只运行契约与源码边界门禁。
    [switch]$StaticOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# 本脚本从自身位置解析唯一工作空间根，禁止依赖个人磁盘路径或当前目录。
$workspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$contractPath = Join-Path $workspaceRoot 'Shared/Contracts/GamePlatform/OpenAPI/commerce.openapi.yaml'
$pluginRoot = Join-Path $workspaceRoot 'Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI'

if (-not (Test-Path -LiteralPath $contractPath -PathType Leaf)) {
    throw "缺少Commerce唯一契约: $contractPath"
}

$contract = Get-Content -LiteralPath $contractPath -Raw -Encoding UTF8
$requiredContractMarkers = @(
    '/v1/commerce/catalog:',
    '/v1/commerce/purchase-intents:',
    '/v1/commerce/orders:',
    'CommerceInt64:',
    "pattern: '^-?(0|[1-9][0-9]{0,18})$'",
    'receiptSubmissionId:'
)
foreach ($marker in $requiredContractMarkers) {
    if (-not $contract.Contains($marker)) {
        throw "Commerce契约缺少语义: $marker"
    }
}

if ($contract -match '(?i)playerId\s*:') {
    throw 'Commerce契约不得接受客户端playerId字段；玩家身份只能来自Bearer认证。'
}

$publicTransport = Join-Path $pluginRoot 'Source/GamePlatformCommerceUIClient/Public/Transport/GamePlatformCommerceGatewayHttpTransport.h'
if ((Test-Path -LiteralPath $publicTransport) -and
    ((Get-Content -LiteralPath $publicTransport -Raw -Encoding UTF8) -match 'AccessToken|GatewayBaseUrl')) {
    Write-Warning '当前仍是迁移前公开Raw Token传输；Task 4完成前该项为已知失败。'
}

Push-Location (Join-Path $workspaceRoot 'Backend')
try {
    & go test ./tests -run 'TestCommerceContract' -count=1
    if ($LASTEXITCODE -ne 0) {
        throw "Commerce契约Go门禁失败，退出码=$LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

if (-not $StaticOnly) {
    Write-Output '非StaticOnly阶段由最终验证任务调用锁定UE5.8构建与Automation；本脚本不猜测引擎位置。'
}

Write-Output 'Commerce静态契约门禁通过。此结果不替代UE5.8编译、Automation、后端集成、Cook、联机支付或人工验收。'
