#Requires -Version 7.0
<#
Online隔离联调公共实现。仅管理本次清单中的精确资源，不读取或输出Docker环境变量。
实施顺序：归属边界测试→启动/迁移→所属领域账号准备→真实HTTP测试→只重启自身→按清单清理。
风险门禁：名称复用、清单伪造/漂移、越界路径、秘密文件ACL、迁移重放；首次环境启动须经人工核对。
所有外部失败仅输出步骤/退出码，不传播可能含连接串或认证正文的原始stderr。
#>
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:OnlineWorkspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

function Assert-OnlineRunId([string]$RunId) {
    $parsed=[Guid]::Empty
    if (-not [Guid]::TryParseExact($RunId,'D',[ref]$parsed) -or $parsed -eq [Guid]::Empty -or $RunId -cne $parsed.ToString('D')) { throw 'RunId必须为非空、小写D格式GUID。' }
}
function Assert-OnlineChildPath([string]$Root, [string]$Path) {
    $base = [IO.Path]::GetFullPath($Root).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    $full = [IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($base, [StringComparison]::OrdinalIgnoreCase)) { throw '路径不在本次授权输出目录内。' }
    # 拒绝已有重解析点，避免生成物/秘密经符号链接逃逸。
    $current = $full
    while ($current.Length -ge $base.TrimEnd([IO.Path]::DirectorySeparatorChar).Length) {
        if ((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw '输出路径包含重解析点。' }
        $parent = [IO.Path]::GetDirectoryName($current)
        if (-not $parent -or $parent -eq $current) { break }; $current = $parent
    }
    return $full
}
function Get-OnlineRunDirectory([string]$RunId) {
    Assert-OnlineRunId $RunId
    return Assert-OnlineChildPath $script:OnlineWorkspace (Join-Path $script:OnlineWorkspace "Saved/Validation/GamePlatformOnline/$RunId")
}
function Protect-OnlineDirectory([string]$Path) {
    if (-not $IsWindows) { throw '本阶段秘密文件ACL脚本仅支持Windows宿主。' }
    [void][IO.Directory]::CreateDirectory($Path)
    $sid = [Security.Principal.WindowsIdentity]::GetCurrent().User
    $acl = [Security.AccessControl.DirectorySecurity]::new()
    $acl.SetOwner($sid); $acl.SetAccessRuleProtection($true, $false)
    $rule = [Security.AccessControl.FileSystemAccessRule]::new($sid, 'FullControl', 'ContainerInherit,ObjectInherit', 'None', 'Allow')
    [void]$acl.AddAccessRule($rule)
    Set-Acl -LiteralPath $Path -AclObject $acl
}
function Assert-OnlinePrivateFile([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw '受限输入文件不存在。' }
    $sid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $acl = Get-Acl -LiteralPath $Path
    foreach ($rule in $acl.Access) {
        $ruleSid = $rule.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value
        if ($rule.AccessControlType -eq 'Allow' -and $ruleSid -ne $sid) { throw '秘密文件存在非当前用户的允许ACL。' }
    }
}
function New-OnlineSecret {
    $bytes = [byte[]]::new(32)
    [Security.Cryptography.RandomNumberGenerator]::Fill($bytes)
    return [Convert]::ToBase64String($bytes).TrimEnd('=').Replace('+','-').Replace('/','_')
}
function Write-OnlinePrivateText([string]$Path, [string]$Text) {
    # 调用者必须先将父目录设为仅当前用户继承；首次写入不会出现宽松权限窗口。
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
    Assert-OnlinePrivateFile $Path
}
function Get-OnlineDockerPath {
    $command = Get-Command docker -ErrorAction SilentlyContinue
    if (-not $command) { throw '未找到Docker可执行文件。' }
    return $command.Source
}
function Invoke-OnlineDocker {
    param([Parameter(Mandatory)][string[]]$Arguments, [int]$TimeoutSeconds=60, [AllowNull()][string]$InputText=$null, [switch]$AllowFailure)
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = Get-OnlineDockerPath
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    $start.RedirectStandardInput = $true
    foreach ($argument in $Arguments) { [void]$start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new(); $process.StartInfo = $start
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
        if ($null -ne $InputText) { $process.StandardInput.Write($InputText) }; $process.StandardInput.Close()
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            $process.Kill($true); $process.WaitForExit()
            throw 'Docker命令超时；不输出原始诊断，已登记容器由调用者按精确身份停止。'
        }
        $output = $stdout.GetAwaiter().GetResult()
        $null = $stderr.GetAwaiter().GetResult()
        $result = [pscustomobject]@{ ExitCode=$process.ExitCode; Output=$output.Trim() }
        if ($result.ExitCode -ne 0 -and -not $AllowFailure) { throw "Docker步骤失败，退出码=$($result.ExitCode)；原始输出已抑制。" }
        return $result
    } finally { $process.Dispose() }
}
function Get-OnlineResource([string]$Kind, [string]$Identity) {
    if ($Kind -eq 'container') {
        $format = '{"Id":{{json .Id}},"Name":{{json .Name}},"Config":{"Labels":{{json .Config.Labels}}},"Status":{{json .State.Status}},"ExitCode":{{json .State.ExitCode}}}'
    } elseif ($Kind -eq 'network') {
        $format = '{"Id":{{json .Id}},"Name":{{json .Name}},"Labels":{{json .Labels}}}'
    } elseif ($Kind -eq 'volume') {
        $format = '{"Id":{{json .CreatedAt}},"Name":{{json .Name}},"Labels":{{json .Labels}}}'
    } else { throw '未知资源种类。' }
    $result = Invoke-OnlineDocker -Arguments @($Kind,'inspect','--format',$format,$Identity) -AllowFailure
    if ($result.ExitCode -ne 0) { throw '无法读取指定Docker资源；拒绝将未知错误当作资源不存在。' }
    return $result.Output | ConvertFrom-Json -Depth 12
}
function Assert-OnlineOwnedResource($State, $Record, $Actual) {
    $expectedName = "dba-online-$($State.RunId)-$($Record.Role)"
    if ($Record.Name -cne $expectedName -or $Actual.Name.TrimStart('/') -cne $expectedName -or $Actual.Id -cne $Record.Id) {
        throw '资源名称或完整ID与本轮清单不一致；拒绝操作。'
    }
    $labels = if ($Record.Kind -eq 'container') { $Actual.Config.Labels } else { $Actual.Labels }
    foreach ($entry in @{
        'com.divinebeasts.online.run'=$State.RunId; 'com.divinebeasts.online.owner'=$State.OwnerToken
        'com.divinebeasts.online.workspace'=$State.Workspace; 'com.divinebeasts.online.role'=$Record.Role
    }.GetEnumerator()) {
        $actualValue = if ($labels -is [Collections.IDictionary]) { $labels[$entry.Key] } else { $labels.PSObject.Properties[$entry.Key].Value }
        if ($actualValue -cne $entry.Value) { throw '资源所有权标签不匹配；拒绝操作。' }
    }
}
function Get-OnlineLabels($State, [string]$Role) {
    return @('--label',"com.divinebeasts.online.run=$($State.RunId)",'--label',"com.divinebeasts.online.owner=$($State.OwnerToken)",
        '--label',"com.divinebeasts.online.workspace=$($State.Workspace)",'--label',"com.divinebeasts.online.role=$Role")
}
function Save-OnlineState($State) {
    $directory = Get-OnlineRunDirectory $State.RunId
    $path = Join-Path $directory 'run.json'
    $temporary = Join-Path $directory 'run.json.new'
    [IO.File]::WriteAllText($temporary, ($State | ConvertTo-Json -Depth 15), [Text.UTF8Encoding]::new($false))
    [IO.File]::Move($temporary,$path,$true)
}
function Read-OnlineState([string]$RunId) {
    $path = Join-Path (Get-OnlineRunDirectory $RunId) 'run.json'
    $state = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json -Depth 15
    if ($state.RunId -cne $RunId -or $state.Workspace -cne $script:OnlineWorkspace -or $state.OwnerToken -cnotmatch '^[a-f0-9]{32}$') { throw '清单与工作区/运行身份不匹配。' }
    return $state
}
function Add-OnlineResource($State, [string]$Kind, [string]$Role, [string[]]$CreateArguments) {
    $name = "dba-online-$($State.RunId)-$Role"
    # create遇同名资源直接失败，不接管、覆盖或删除它。
    $arguments = if ($Kind -eq 'container') { @('create','--name',$name) } else { @($Kind,'create') }
    $arguments += @(Get-OnlineLabels $State $Role)
    $arguments += $CreateArguments
    if ($Kind -ne 'container') { $arguments += $name }
    $created = Invoke-OnlineDocker -Arguments $arguments -TimeoutSeconds 120
    $identity = if ($Kind -eq 'container' -or $Kind -eq 'network') { $created.Output } else { $name }
    $actual = Get-OnlineResource $Kind $identity
    $record = [pscustomobject]@{ Kind=$Kind; Role=$Role; Name=$name; Id=$actual.Id; Removed=$false }
    Assert-OnlineOwnedResource $State $record $actual
    $State.Resources = @($State.Resources) + $record
    Save-OnlineState $State
    return $record
}
function Get-OnlineOwnedRecord($State, [string]$Role) {
    $found = @($State.Resources | Where-Object { $_.Role -ceq $Role -and -not $_.Removed })
    if ($found.Count -ne 1) { throw '清单必须恰好包含一个目标资源。' }
    $record = $found[0]
    $identity = if ($record.Kind -eq 'volume') { $record.Name } else { $record.Id }
    Assert-OnlineOwnedResource $State $record (Get-OnlineResource $record.Kind $identity)
    return $record
}
function Invoke-OnlineJob($State, [string]$Role, [string[]]$CreateArguments, [int]$TimeoutSeconds=900) {
    $record = Add-OnlineResource $State 'container' $Role $CreateArguments
    try {
        $result = Invoke-OnlineDocker -Arguments @('start','--attach',$record.Id) -TimeoutSeconds $TimeoutSeconds
        $actual = Get-OnlineResource 'container' $record.Id
        Assert-OnlineOwnedResource $State $record $actual
        if ($actual.ExitCode -ne 0 -or $actual.Status -ne 'exited') { throw '一次性工具未正常完成。' }
        return $result.Output
    } catch {
        $null = Get-OnlineOwnedRecord $State $Role
        $null = Invoke-OnlineDocker -Arguments @('stop','--time','5',$record.Id) -AllowFailure
        throw "本轮$Role工具失败或超时；容器已按所有权清单停止，未输出原始诊断。"
    }
}
function Write-OnlineResult($State, [string]$FileName, [string]$Status, [int]$ExitCode, [object[]]$Cases, [string[]]$Evidence=@()) {
    if ($Status -notin @('Passed','Failed','NotExecuted')) { throw '非法结果状态。' }
    $directory = Get-OnlineRunDirectory $State.RunId
    $path = Assert-OnlineChildPath $directory (Join-Path $directory $FileName)
    $report = [ordered]@{
        RunId=$State.RunId; Status=$Status; ExitCode=$ExitCode; Cases=@($Cases)
        ProcessOrContainerIds=@($State.Resources | Where-Object Kind -eq 'container' | Select-Object Role,Name,Id,Removed)
        Evidence=@($Evidence); SourceFingerprint=$State.SourceFingerprint; RecordedAt=[DateTimeOffset]::UtcNow.ToString('o')
    }
    [IO.File]::WriteAllText($path, ($report | ConvertTo-Json -Depth 15), [Text.UTF8Encoding]::new($false))
    return $path
}
function Wait-OnlineProbe($State, [int]$TimeoutSeconds=60) {
    $handler = [Net.Http.HttpClientHandler]::new(); $handler.AllowAutoRedirect=$false
    $client = [Net.Http.HttpClient]::new($handler); $client.Timeout=[TimeSpan]::FromSeconds(6)
    try {
        $deadline=[DateTimeOffset]::UtcNow.AddSeconds($TimeoutSeconds)
        do {
            try {
                $response=$client.GetAsync("http://127.0.0.1:$($State.GatewayPort)/v1/online/probe").GetAwaiter().GetResult()
                try {
                    if ([int]$response.StatusCode -eq 200) {
                        $body=$response.Content.ReadAsStringAsync().GetAwaiter().GetResult() | ConvertFrom-Json
                        if ($body.ready -eq $true -and $body.contractVersion -eq '1.0.0' -and $body.service -eq 'gatewayservice') { return }
                    }
                } finally { $response.Dispose() }
            } catch { }
            Start-Sleep -Milliseconds 500
        } while ([DateTimeOffset]::UtcNow -lt $deadline)
        throw '真实Online依赖探测未就绪或协议不匹配。'
    } finally { $client.Dispose(); $handler.Dispose() }
}
Export-ModuleMember -Function *-Online*
