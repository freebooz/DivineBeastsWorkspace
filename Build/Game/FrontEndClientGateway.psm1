<#
.SYNOPSIS
校验前端客户端使用的业务网关地址。
.DESCRIPTION
本模块属于构建与验证工具层，仅负责验证调用方明确选择的HTTP(S)网关，不探测、改写或默认选择玩家档案环境。
不同后端环境拥有彼此隔离的身份和玩家档案；调用方必须显式指定地址，避免登录后看到另一环境的角色数据。
拒绝空地址、非绝对HTTP(S)地址、内嵌凭据、查询串和片段；不访问网络、不读取凭据、不修改后端或玩家数据。
#>

function Resolve-FrontEndClientGatewayUrl {
    <#
    .SYNOPSIS
    校验并原样返回显式指定的前端业务网关地址。
    .DESCRIPTION
    调用方必须选择目标档案环境。函数只验证地址形态，不尝试猜测可用后端，也不在失败时切换到其他环境。
    .PARAMETER GatewayUrl
    业务网关绝对HTTP或HTTPS地址；不得包含账号凭据、查询串或片段。
    .OUTPUTS
    System.String。通过校验的原始地址，供子进程环境变量使用。
    .NOTES
    无网络与持久化副作用。无效输入抛出可读异常，调用方应在创建证据目录或启动进程之前处理。
    #>
    [CmdletBinding()]
    param(
        [AllowNull()]
        [string]$GatewayUrl
    )

    if ([string]::IsNullOrWhiteSpace($GatewayUrl)) {
        throw '必须显式指定-GatewayUrl。不同后端环境的角色档案相互隔离，启动器不会默认选择数据环境。'
    }

    $parsedUri = $null
    $isAbsoluteUri = [System.Uri]::TryCreate(
        $GatewayUrl,
        [System.UriKind]::Absolute,
        [ref]$parsedUri
    )
    if (-not $isAbsoluteUri -or $parsedUri.Scheme -notin @('http', 'https')) {
        throw 'GatewayUrl必须是绝对HTTP(S)服务地址。'
    }
    if ($parsedUri.UserInfo -or $parsedUri.Query -or $parsedUri.Fragment) {
        throw 'GatewayUrl不得包含账号凭据、查询参数或片段；请通过受控配置提供不含秘密的网关基础地址。'
    }

    return $GatewayUrl
}

function Get-FrontEndClientHttpArguments {
    <#
    .SYNOPSIS
    为显式回环网关生成UE原生HttpNoProxy参数，避免系统代理把本地认证请求转发为502。
    .DESCRIPTION
    输入沿用网关地址校验；仅Uri.IsLoopback为真时返回一个启动参数，其余地址返回空数组。
    使用解析后的准确主机名，不以字符串前缀识别本机，不使用通配符，不改系统代理或持久配置。
    返回值供ProcessStartInfo.ArgumentList使用；无网络、凭据读取或后端副作用。
    #>
    [CmdletBinding()]
    param([AllowNull()][string]$GatewayUrl)
    $gateway=[uri](Resolve-FrontEndClientGatewayUrl -GatewayUrl $GatewayUrl)
    if($gateway.IsLoopback) {
        # UE FParse::Value会在未加引号的逗号处截断；单主机参数只覆盖实际网关。
        return '-HttpNoProxy=' + $gateway.DnsSafeHost.Trim('[',']')
    }
}

Export-ModuleMember -Function Resolve-FrontEndClientGatewayUrl,Get-FrontEndClientHttpArguments
