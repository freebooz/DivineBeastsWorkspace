#requires -Version 7.0
<# SSLM登录开发入口的无UE回归：测试宿主资格与脚本接线，不把静态检查当作点击或资产验收。 #>
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$passed=0
# 直接执行启动脚本中的纯配置函数，不运行脚本顶层或创建UE进程。
# 若十账号校验或子进程隔离退化，本组测试会在真实函数返回值上失败；测试口令仅为内存夹具。
$frontEndPath=Join-Path $root 'Build/Game/RunFrontEndClients.ps1'
$tokens=$null; $parseErrors=$null
$frontEndAst=[Management.Automation.Language.Parser]::ParseFile($frontEndPath,[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw '前端启动脚本语法错误'}
foreach($name in @('Resolve-FrontEndDevelopmentTestAccounts','Set-FrontEndDevelopmentTestAccountEnvironment')) {
    $definition=$frontEndAst.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name},$true)
    if(!$definition){throw "开发登录配置函数尚未实现：$name"}
    . ([scriptblock]::Create($definition.Extent.Text))
}
$seeds=@(1..10|ForEach-Object{@{accountName=('player{0:d2}' -f $_);password='fixture-only-development-password'}})
$fixtureJson=ConvertTo-Json -InputObject $seeds -Compress
$configuration=Resolve-FrontEndDevelopmentTestAccounts -Enabled -AccountsJson $fixtureJson
if($configuration.AccountNames.Count -ne 10 -or $configuration.AccountNames[0] -cne 'player01' -or $configuration.AccountNames[9] -cne 'player10') {throw '十个真实账号名称未保留'}
if(@($configuration.AccountsJson|ConvertFrom-Json).Count -ne 10){throw '子进程JSON未保留十账号'}; $passed++
# 开关关闭时，即使父环境有种子也不能意外开放测试账号。
$disabled=Resolve-FrontEndDevelopmentTestAccounts -AccountsJson $fixtureJson
if($disabled.AccountsJson -or $disabled.AccountNames.Count){throw '默认启动泄露了测试账号'}; $passed++
$invalidJson=@('', '{invalid', (ConvertTo-Json -InputObject $seeds[0..8] -Compress))
$duplicate=@($seeds|ForEach-Object{@{accountName=$_.accountName;password=$_.password}}); $duplicate[9].accountName='player01'
$invalidJson+=ConvertTo-Json -InputObject $duplicate -Compress
$blank=@($seeds|ForEach-Object{@{accountName=$_.accountName;password=$_.password}}); $blank[9].password=''
$invalidJson+=ConvertTo-Json -InputObject $blank -Compress
foreach($invalid in $invalidJson){
    $rejected=$false
    try{$null=Resolve-FrontEndDevelopmentTestAccounts -Enabled -AccountsJson $invalid}catch{$rejected=$true; if($_.Exception.Message.Contains('fixture-only')){throw '配置失败泄露了口令'}}
    if(!$rejected){throw '无效或不完整十账号配置未被拒绝'}; $passed++
}
# 真正检查ProcessStartInfo环境：默认清除旧密码、父种子和新测试变量；启用后只传专用JSON。
foreach($enabled in @($false,$true)){
    $start=[Diagnostics.ProcessStartInfo]::new()
    foreach($key in @('DBA_DEV_LOGIN_SECRET','DIVINEBEASTS_DEV_LOGIN_USER','DIVINEBEASTS_DEV_LOGIN_PASSWORD','DIVINEBEASTS_DEV_LOGIN_ACCOUNTS_JSON','DBA_DEV_TEST_LOGIN_ACCOUNTS_JSON')){$start.Environment[$key]='fixture-only'}
    Set-FrontEndDevelopmentTestAccountEnvironment -StartInfo $start -Configuration $(if($enabled){$configuration}else{$disabled})
    foreach($key in @('DBA_DEV_LOGIN_SECRET','DIVINEBEASTS_DEV_LOGIN_USER','DIVINEBEASTS_DEV_LOGIN_PASSWORD','DIVINEBEASTS_DEV_LOGIN_ACCOUNTS_JSON')){if($start.Environment.ContainsKey($key)){throw "子进程保留父认证变量：$key"}}
    if($start.Environment.ContainsKey('DBA_DEV_TEST_LOGIN_ACCOUNTS_JSON') -ne $enabled){throw '专用测试凭据环境与开关不一致'}
    if($enabled -and $start.Environment['DBA_DEV_TEST_LOGIN_ACCOUNTS_JSON'] -cne $configuration.AccountsJson){throw '测试凭据JSON未按校验结果传递'}
    $passed++
}
# 演示启动的唯一入口必须主动清除全部开发登录环境；桥的白名单不是直接入口的替代。
$demoAst=[Management.Automation.Language.Parser]::ParseFile((Join-Path $root 'Build/Game/RunPluginDemo.ps1'),[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw '演示启动脚本语法错误'}
$cleanup=$demoAst.Find({param($node) $node -is [Management.Automation.Language.ForEachStatementAst] -and $node.Variable.VariablePath.UserPath -eq 'key' -and $node.Extent.Text.Contains('DBA_DEV_LOGIN_SECRET')},$true)
if(!$cleanup){throw '演示入口缺少子进程凭据清理'}
$info=[Diagnostics.ProcessStartInfo]::new()
$credentialKeys=@('DBA_DEV_LOGIN_SECRET','DBA_DEV_TEST_LOGIN_ACCOUNTS_JSON','DIVINEBEASTS_DEV_LOGIN_ACCOUNTS_JSON','DIVINEBEASTS_DEV_LOGIN_USER','DIVINEBEASTS_DEV_LOGIN_PASSWORD','DIVINEBEASTS_GATEWAY_BASE_URL')
foreach($key in $credentialKeys){$info.Environment[$key]='fixture-only'}
. ([scriptblock]::Create($cleanup.Extent.Text))
foreach($key in $credentialKeys){if($info.Environment.ContainsKey($key)){throw "演示入口遗留认证环境：$key"}; $passed++}
# 参数元数据回归不执行启动副作用；源码根来自脚本自身，EngineRoot与构建证据均需调用方明确选择。
foreach($name in @('DevelopmentTools','DevelopmentBuildResult','EngineRoot','DevelopmentTestAccounts')){
    if(!($frontEndAst.ParamBlock.Parameters|Where-Object{$_.Name.VariablePath.UserPath -eq $name})){throw "前端启动器缺少显式参数：$name"}; $passed++
}
$runner=Get-Content -LiteralPath (Join-Path $root 'Build/Game/LaunchPluginDemoFromLogin.ps1') -Raw
foreach($required in @('RunPluginDemo.ps1','-Start','-EngineRoot','-BuildResultPath $BuildResultPath','WaitForExit','status.txt','Failed')) {
    if(!$runner.Contains($required)){throw "启动器缺少合同：$required"}; $passed++
}
if($runner -match 'BuildFoundation|UnrealBuildTool|FCommandLine::Append'){throw '按钮不得自动构建或修改父进程模式'}
# 真正执行隔离脚本的失败路径：不存在引擎不得启动UE，须留下可读失败结果并传播非零。
$runId=[guid]::NewGuid()
& (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $root 'Build/Game/LaunchPluginDemoFromLogin.ps1') -RunId $runId -EngineRoot (Join-Path $root 'Saved/DoesNotExist-LoginLauncherTest')
if($LASTEXITCODE -eq 0){throw '缺失引擎错误被吞掉'}; $passed++
$status=Join-Path $root "Saved/Validation/PluginDemos/LoginLaunch/$runId/status.txt"
if((Get-Content $status -Raw).Trim() -ne 'Failed'){throw '缺失失败状态'}; $passed++
& (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $root 'Build/Game/LaunchPluginDemoFromLogin.ps1') -RunId $runId -EngineRoot (Join-Path $root 'Saved/DoesNotExist-LoginLauncherTest')
if($LASTEXITCODE -eq 0){throw '重复RunId未拒绝'}; $passed++
Write-Output "LoginPluginDemoContractCases=$passed Failed=0 UEExecuted=False"
