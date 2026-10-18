#Requires -Version 7.0
<#
.SYNOPSIS
验证前端客户端必须显式选择业务网关，且网关地址满足安全边界。
.DESCRIPTION
本测试调用真实的启动配置解析函数，并读取PowerShell参数元数据，防止启动器静默落到另一套隔离玩家档案。
测试覆盖旧28081与当前28181地址语法均可显式解析，以及空地址、非HTTP地址、凭据、查询和片段均被拒绝。
地址语法有效不表示对应服务正在运行；当前项目只保留28181后端，旧地址仅作为解析函数的合法输入回归。
不会启动游戏、访问网络、读取账号口令或修改后端数据；失败时以非零退出码报告。
#>
$ErrorActionPreference = 'Stop'

$workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$modulePath = Join-Path $workspaceRoot 'Build/Game/FrontEndClientGateway.psm1'
$launcherPath = Join-Path $workspaceRoot 'Build/Game/RunFrontEndClients.ps1'

Import-Module $modulePath -Force

# 旧、新后端的玩家档案互不共享；正确行为是强制调用方明确选择，而非暗中改变数据源。
$cases = @(
    @{ Name = 'validation gateway'; Value = 'http://127.0.0.1:28181'; Reject = $false },
    @{ Name = 'legacy address syntax remains valid'; Value = 'http://127.0.0.1:28081'; Reject = $false },
    @{ Name = 'missing gateway'; Value = ''; Reject = $true },
    @{ Name = 'relative address'; Value = '/gateway'; Reject = $true },
    @{ Name = 'unsupported scheme'; Value = 'ftp://127.0.0.1:28181'; Reject = $true },
    @{ Name = 'embedded credentials'; Value = 'http://user:placeholder@127.0.0.1:28181'; Reject = $true },
    @{ Name = 'query string'; Value = 'http://127.0.0.1:28181?profile=other'; Reject = $true },
    @{ Name = 'fragment'; Value = 'http://127.0.0.1:28181#profile'; Reject = $true }
)

foreach ($case in $cases) {
    $caught = $false
    try {
        $actual = Resolve-FrontEndClientGatewayUrl -GatewayUrl $case.Value
    }
    catch {
        $caught = $true
        $errorMessage = $_.Exception.Message
    }

    if ($caught -ne $case.Reject) {
        throw "网关契约用例失败：$($case.Name)；Reject=$($case.Reject)，Caught=$caught。"
    }
    if (-not $case.Reject -and $actual -cne $case.Value) {
        throw "网关契约用例失败：$($case.Name) 应原样保留显式网关地址。"
    }
}

# 启动脚本有真实启动副作用，不能在单元测试中省略地址来试错；用PowerShell AST检查调用方必须显式传参。
$tokens = $null
$parseErrors = $null
$launcherAst = [System.Management.Automation.Language.Parser]::ParseFile(
    $launcherPath,
    [ref]$tokens,
    [ref]$parseErrors
)
if ($parseErrors.Count -gt 0) {
    throw "前端启动脚本无法解析：$($parseErrors[0].Message)"
}
$gatewayParameter = $launcherAst.ParamBlock.Parameters |
    Where-Object { $_.Name.VariablePath.UserPath -eq 'GatewayUrl' } |
    Select-Object -First 1
if (-not $gatewayParameter -or $gatewayParameter.DefaultValue) {
    throw 'GatewayUrl必须由调用方显式指定，禁止启动器默认切换玩家档案库。'
}

# 本地联调必须直连回环网关；远程/局域网服务仍沿用用户代理，不能扩大绕过范围。
$proxyCases = @(
    @{ Value='http://127.0.0.1:28181'; HostName='127.0.0.1'; Bypass=$true },
    @{ Value='http://[::1]:28181'; HostName='::1'; Bypass=$true },
    @{ Value='http://localhost:28181'; HostName='localhost'; Bypass=$true },
    @{ Value='http://127.0.0.2:28181'; HostName='127.0.0.2'; Bypass=$true },
    @{ Value='https://gateway.example.test'; Bypass=$false },
    @{ Value='http://192.168.1.2:28181'; Bypass=$false },
    @{ Value='http://127.0.0.1.example.test:28181'; Bypass=$false }
)
foreach($case in $proxyCases) {
    $arguments=@(Get-FrontEndClientHttpArguments -GatewayUrl $case.Value)
    if($case.Bypass) {
        if($arguments.Count -ne 1 -or !$arguments[0].StartsWith('-HttpNoProxy=')) {
            throw "回环网关缺少UE原生代理绕过参数：$($case.Value)"
        }
        # UE FParse::Value默认在未加引号的逗号处截断；准确单主机同时避免扩大绕过范围。
        if($arguments[0] -cne ('-HttpNoProxy=' + $case.HostName)) {
            throw "代理绕过必须仅包含准确回环主机，不能被UE分隔符解析截断：$($case.Value)"
        }
    } elseif($arguments.Count -ne 0) {
        throw "非回环网关不得改变HTTP代理策略：$($case.Value)"
    }
}

Write-Host 'FRONTEND_CLIENT_GATEWAY_CONTRACT_PASS'
