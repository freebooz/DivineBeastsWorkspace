<# 不连接Docker、不启动服务：验证生产脚本的RunId、资源归属及路径约束，不以源码字符串断言代替行为。 #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'OnlineIntegrationTools.psm1') -Force
$script:checks = 0
function Expect-Rejected([scriptblock]$Action) {
    $rejected = $false
    try { & $Action } catch { $rejected = $true }
    if (-not $rejected) { throw '归属/输入边界没有拒绝非法请求。' }
    $script:checks++
}
Assert-OnlineRunId '11111111-1111-4111-8111-111111111111'
$script:checks++
foreach ($bad in @('../other', '', 'UPPERCASE', 'x y', 'abc;stop', ('x' * 33))) {
    Expect-Rejected { Assert-OnlineRunId $bad }
}
$state = [pscustomobject]@{ RunId='11111111-1111-4111-8111-111111111111'; OwnerToken='owner123'; Workspace='test-workspace' }
$record = [pscustomobject]@{ Kind='container'; Role='gateway'; Name='dba-online-11111111-1111-4111-8111-111111111111-gateway'; Id=('a' * 64) }
$labels = @{ 'com.divinebeasts.online.run'='11111111-1111-4111-8111-111111111111'; 'com.divinebeasts.online.owner'='owner123'; 'com.divinebeasts.online.workspace'='test-workspace'; 'com.divinebeasts.online.role'='gateway' }
$actual = [pscustomobject]@{ Id=('a' * 64); Name='/dba-online-11111111-1111-4111-8111-111111111111-gateway'; Config=[pscustomobject]@{ Labels=$labels } }
Assert-OnlineOwnedResource $state $record $actual
$script:checks++
$actual.Id = 'b' * 64
Expect-Rejected { Assert-OnlineOwnedResource $state $record $actual }
$actual.Id = 'a' * 64
$actual.Name = '/someone-else'
Expect-Rejected { Assert-OnlineOwnedResource $state $record $actual }
$actual.Name = '/dba-online-11111111-1111-4111-8111-111111111111-gateway'
foreach ($key in @('com.divinebeasts.online.run','com.divinebeasts.online.owner','com.divinebeasts.online.workspace','com.divinebeasts.online.role')) {
    $old = $labels[$key]; $labels[$key] = 'foreign'
    Expect-Rejected { Assert-OnlineOwnedResource $state $record $actual }
    $labels[$key] = $old
}
$root = Join-Path ([IO.Path]::GetTempPath()) 'online-safety-no-files'
$safe = Assert-OnlineChildPath $root (Join-Path $root 'credentials.json')
if ($safe -ne [IO.Path]::GetFullPath((Join-Path $root 'credentials.json'))) { throw '合法子路径解析失败。' }
$script:checks++
Expect-Rejected { Assert-OnlineChildPath $root (Join-Path $root '../foreign.json') }
Expect-Rejected { Assert-OnlineChildPath $root $root }
Write-Output "OnlineScriptSafety Cases=$script:checks Failed=0; DockerNotInvoked=true"
